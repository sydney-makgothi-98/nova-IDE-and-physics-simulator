# JSON and Protocols: How Programs Talk

## Why this chapter exists

NOVA is a team of programs: the window, clangd, gdb, the compiler. Programs cannot read each other's minds. They must agree on **how** to send messages. An agreed way of talking is a **protocol**. In this chapter you learn the three "languages" NOVA speaks with its helpers: **JSON**, the **Language Server Protocol** (LSP) and **GDB/MI**, plus the terminal's secret code, **ANSI escape sequences**.

::: def Protocol
A **protocol** is a set of rules for a conversation between programs: what a message looks like, who speaks first, how to say "I did not understand", and how to know a message has ended.
:::

## JSON: the common alphabet

**JSON** (JavaScript Object Notation) is a simple text format for data. It is easy for people to read and easy for every language to parse. It has only six kinds of value:

| Kind | Example |
|---|---|
| string | `"main.cpp"` |
| number | `42`, `3.14`, `-7` |
| boolean | `true`, `false` |
| null | `null` |
| array (a list) | `[1, 2, 3]` |
| object (named parts) | `{"name": "main.cpp", "line": 12}` |

```json
{
  "theme": "dark",
  "fontSize": 14,
  "ligatures": true,
  "recent": ["C:/projects/hello_nova", "C:/projects/geokit"],
  "debugExtConsole": null
}
```

This is, almost exactly, NOVA's settings file.

```js run
const settings = { theme: "dark", fontSize: 14, recent: ["a", "b"], extra: undefined };
const text = JSON.stringify(settings);
console.log(text);                          // undefined fields are dropped
console.log(JSON.stringify(settings, null, 2));   // pretty, with 2-space indent

const back = JSON.parse(text);
console.log(back.fontSize + 1, back.recent[1]);

try { JSON.parse("{ 'bad': 1 }"); } catch (e) { console.log("invalid JSON:", e.name); }
```

::: why Why JSON's strict rules?
JSON requires **double quotes** and forbids trailing commas and comments. That strictness is what lets any program in any language read it without argument. NOVA wraps `JSON.parse` for its settings in `try/catch`: if a hand-edited file has a mistake, NOVA starts with defaults instead of crashing.
:::

::: warn Watch out
`JSON.stringify` cannot carry functions, `undefined` or circular references, and turns a `Map` or `Set` into `{}`. Convert them first (for example `[...mySet]`).
:::

## Framing: where does one message end?

Programs talk through **pipes**, which are just streams of bytes with no message boundaries. If you write `{"a":1}` and then `{"b":2}`, the reader might receive `{"a":1}{"b` first and `":2}` later. The receiver needs a rule to split the stream into messages. This rule is called **framing**.

LSP uses a header with the byte length:

{{fig:jsonrpc-frame|One LSP message: a header telling how many bytes follow, then exactly that many bytes of JSON.}}

Here is a working reader built the same way as the one in NOVA's `main.js`. It handles messages split at any point.

```js run
function makeReader(onMessage) {
  let buf = Buffer.alloc(0);
  return (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      const headerEnd = buf.indexOf("\r\n\r\n");
      if (headerEnd < 0) return;                           // header not complete yet
      const m = /Content-Length:\s*(\d+)/i.exec(buf.slice(0, headerEnd).toString("ascii"));
      if (!m) { buf = buf.slice(headerEnd + 4); continue; }
      const len = Number(m[1]);
      if (buf.length < headerEnd + 4 + len) return;        // body not complete yet
      const body = buf.slice(headerEnd + 4, headerEnd + 4 + len).toString("utf8");
      buf = buf.slice(headerEnd + 4 + len);
      onMessage(JSON.parse(body));
    }
  };
}

const frame = (obj) => {
  const json = JSON.stringify(obj);
  return Buffer.from(`Content-Length: ${Buffer.byteLength(json, "utf8")}\r\n\r\n${json}`, "utf8");
};

const feed = makeReader((msg) => console.log("got:", JSON.stringify(msg)));
const wire = Buffer.concat([frame({ id: 1, method: "hello" }), frame({ id: 2, method: "naïve ✓" })]);

// Deliver the bytes in awkward pieces: 7 bytes, 5 bytes, then the rest.
feed(wire.slice(0, 7));
feed(wire.slice(7, 12));
feed(wire.slice(12));
```

::: why Why Buffer.byteLength and not text.length?
The header counts **bytes**, not characters. The text `naïve ✓` has 7 characters but 10 bytes in UTF-8, because `ï` and `✓` take more than one byte. Counting characters would make the reader cut a message in the wrong place and every later message would be garbage. NOVA's main process uses `Buffer.byteLength` for exactly this reason.
:::

::: whatif What if a message is incomplete and the reader parses it anyway?
`JSON.parse` would throw on half a message, and the stream would be permanently out of step: the next chunk would begin in the middle of something. Returning and waiting for more bytes (as the code above does) keeps the reader in sync.
:::

## JSON-RPC: asking questions across the pipe

LSP messages follow **JSON-RPC 2.0**. There are three kinds:

| Kind | Has `id`? | Has `method`? | Meaning |
|---|---|---|---|
| **Request** | yes | yes | "Please do this and answer me." |
| **Response** | yes (same id) | no (has `result` or `error`) | The answer to one request. |
| **Notification** | no | yes | "FYI", no answer expected. |

```json
{"jsonrpc":"2.0","id":7,"method":"textDocument/hover",
 "params":{"textDocument":{"uri":"file:///c%3A/proj/main.cpp"},"position":{"line":10,"character":17}}}
```

Reply:

```json
{"jsonrpc":"2.0","id":7,"result":{"contents":{"kind":"markdown","value":"### function `square`..."}}}
```

The **id** is the clever part. Many requests can be in flight at once, and answers may come in any order. The id says which answer belongs to which question.

```js run
const pending = new Map();      // id -> function to call with the answer
let nextId = 0;

function request(send, method, params, timeoutMs = 200) {
  return new Promise((resolve) => {
    const id = ++nextId;
    const timer = setTimeout(() => { pending.delete(id); resolve(null); }, timeoutMs);   // give up
    pending.set(id, (result) => { clearTimeout(timer); resolve(result); });
    send({ jsonrpc: "2.0", id, method, params });
  });
}
function onResponse(msg) {
  const cb = pending.get(msg.id);
  if (cb) { pending.delete(msg.id); cb(msg.result); }
}

// A fake server that answers the SLOW request after the FAST one.
const server = (msg) => {
  const delay = msg.method === "slow" ? 60 : 10;
  setTimeout(() => onResponse({ id: msg.id, result: msg.method + " answer" }), delay);
};
const silent = () => { /* never answers */ };

(async () => {
  const [a, b] = await Promise.all([request(server, "slow"), request(server, "fast")]);
  console.log(a, "|", b);                        // each got its OWN answer
  console.log(await request(silent, "ignored"));  // times out -> null
})();
```

This is the pattern in `lsp.js`: a `Map` of waiting requests, a timeout so a stuck server cannot freeze the editor, and the id to pair answers with questions.

::: why Why a timeout on every request?
A language server can crash, hang or be busy indexing. Without a timeout, the code that asked "what can I type here?" would wait forever, and a stuck completion popup would be the least of your problems. NOVA gives every request eight seconds (thirty for the first `initialize`), then simply returns "no answer".
:::

### The LSP conversation in order

1. **`initialize`** (request): NOVA says "my project folder is here, and I understand these features". The server answers with *its* capabilities.
2. **`initialized`** (notification): "Great, let's go."
3. **`textDocument/didOpen`** (notification): "I opened this file; here is its text."
4. **`textDocument/didChange`** (notification): "The text changed." (NOVA sends the whole text, debounced by 120 ms.)
5. **Requests** such as `completion`, `hover`, `definition`, `references`, `rename`, `formatting`, `codeAction`.
6. **`textDocument/publishDiagnostics`** (notification, from the server): "Here are the errors in this file." NOVA turns them into wavy lines.

::: whatif What if NOVA forgot to send didChange?
The server would keep analysing the *old* text. Completion would suggest names that no longer exist, hover would point at wrong lines, and error underlines would sit in the wrong places. The editor and server must always agree about the file's contents.
:::

### The off-by-one trap

LSP positions are **0-based** (line 0 is the first line). Monaco's are **1-based**. NOVA converts at the border:

```js
const toPos = (p) => ({ line: p.lineNumber - 1, character: p.column - 1 });
const fromRange = (r) => new monaco.Range(r.start.line + 1, r.start.character + 1, r.end.line + 1, r.end.character + 1);
```

Forget the conversion and every underline lands one line too high.

## GDB/MI: talking to the debugger

gdb has a human interface (type `print x`). It also has a **machine interface** called **GDB/MI** designed for programs like NOVA. You start gdb with `--interpreter=mi3` and talk to it in lines of text.

{{fig:mi-anatomy|The first character of every MI line tells you what kind of message it is.}}

| First char | Kind | Example |
|---|---|---|
| `^` | **Result** of a command | `^done,bkpt={number="1"}` |
| `*` | **Async** event (program state changed) | `*stopped,reason="breakpoint-hit"` |
| `=` | Notification (breakpoint added, library loaded) | `=breakpoint-modified,...` |
| `~` | Console output text | `~"Hello\n"` |
| `@` | The program's own output | `@"Total: 5\n"` |
| `&` | gdb's log | `&"print x\n"` |

A command may begin with a **token** (a number) so you can match the result: you send `7-exec-next` and gdb answers `7^running`.

After the class name, data is a list of `key="value"` pairs, where a value can be a string, a `{tuple}` or a `[list]`. Here is a working parser that is the same logic as NOVA's `debug.js`:

```js run
function miCString(p) {
  if (p.s[p.i] !== '"') return "";
  p.i++;
  let out = "";
  while (p.i < p.s.length) {
    const c = p.s[p.i++];
    if (c === '"') break;
    if (c === "\\") {
      const n = p.s[p.i++];
      if (n === "n") out += "\n"; else if (n === "t") out += "\t"; else out += n;
    } else out += c;
  }
  return out;
}
function miValue(p) {
  const c = p.s[p.i];
  if (c === '"') return miCString(p);
  if (c === "{") { p.i++; return miResults(p, "}"); }
  if (c === "[") {
    p.i++;
    const arr = [];
    while (p.i < p.s.length && p.s[p.i] !== "]") {
      if (/^[\w-]+=/.test(p.s.slice(p.i))) {          // a named item such as frame={...}
        const eq = p.s.indexOf("=", p.i);
        const k = p.s.slice(p.i, eq);
        p.i = eq + 1;
        arr.push({ [k]: miValue(p) });
      } else arr.push(miValue(p));
      if (p.s[p.i] === ",") p.i++;
    }
    p.i++;
    return arr;
  }
  return "";
}
function miResults(p, close) {
  const o = {};
  while (p.i < p.s.length && p.s[p.i] !== close) {
    const eq = p.s.indexOf("=", p.i);
    if (eq < 0) break;
    const k = p.s.slice(p.i, eq);
    p.i = eq + 1;
    o[k] = miValue(p);
    if (p.s[p.i] === ",") p.i++;
  }
  if (close && p.s[p.i] === close) p.i++;
  return o;
}
function miParseLine(line) {
  const m = /^(\d*)([\^*=~@&])(.*)$/.exec(line);
  if (!m) return { t: "raw", text: line };
  const [, tok, ch, rest] = m;
  if ("~@&".includes(ch)) return { t: ch, text: miCString({ s: rest, i: 0 }) };
  const ci = rest.indexOf(",");
  const cls = ci < 0 ? rest : rest.slice(0, ci);
  const data = ci < 0 ? {} : miResults({ s: rest.slice(ci + 1), i: 0 });
  return { t: ch, tok: tok ? +tok : null, cls, data };
}

const samples = [
  '*stopped,reason="breakpoint-hit",frame={func="main",line="9"}',
  '7^done,stack=[frame={level="0",func="square",line="4"},frame={level="1",func="main",line="11"}]',
  '~"Hello, \\"NOVA\\"\\n"',
  "^running",
  "just some program output",
];
for (const s of samples) console.log(JSON.stringify(miParseLine(s)));
```

Study the second output: a nested structure with a list of frames turned into plain JavaScript data. NOVA reads `data.stack`, maps each `frame` to `{func, line, file}` and draws the Frames panel.

::: why Why write a parser instead of using a regular expression?
MI values **nest**: a tuple contains a list that contains tuples. Regular expressions cannot match arbitrarily deep nesting. A parser that calls itself for each nested level (`miValue` calls `miResults`, which calls `miValue`...) handles any depth. It is the same *recursive descent* idea as the calculator in Chapter 8.
:::

## The terminal: bytes and escape codes

A terminal is a program that shows a grid of characters and sends your key presses to a shell. A shell program cannot draw colours or move the cursor with function calls, so it sends special byte sequences hidden inside its text, beginning with the character **ESC** (code 27, written `\x1b`). These are **ANSI escape sequences**.

```js run
const ESC = "\x1b";
const coloured = `${ESC}[31mred${ESC}[0m normal ${ESC}[1;32mbold green${ESC}[0m`;
console.log(JSON.stringify(coloured));

// What NOVA's Run panel does: strip them, because it draws its own colours
const plain = coloured.replace(/\x1b\[[0-9;]*[A-Za-z]/g, "");
console.log(plain);
```

Common ones: `ESC[31m` red text, `ESC[0m` reset all, `ESC[2J` clear screen, `ESC[10;5H` move the cursor to row 10, column 5. The **xterm.js** library inside NOVA understands them all and draws the result.

::: why Why does NOVA use a PTY for the terminal instead of simple pipes?
Programs behave differently when they know a human is watching. Many check "is my output a **terminal**?" and, if not, turn off colours, buffer their output and refuse to ask questions. A **PTY** (pseudo-terminal; on Windows, **ConPTY**) pretends to be a real terminal, so shells, git, Python and text-mode programs behave normally. With plain pipes, PowerShell would print no prompt and interactive tools would not work.
:::

::: story The buffered printf
The Run panel uses pipes, not a PTY, so a C program's `printf("Enter a number: ")` may stay hidden in the program's own buffer until it ends. C's standard library buffers output fully when it is not connected to a terminal. NOVA fixes this by quietly adding a tiny header to the build, `-include nova_unbuf.h`, which calls `setvbuf(stdout, NULL, _IONBF, 0)` to turn the buffering off. This is why prompts appear instantly in NOVA. A small trick, but it needed a good understanding of how protocols between a program and its viewer really work.
:::

## Designing your own protocol: lessons

1. **Frame your messages.** Never assume a read equals a message.
2. **Give requests ids.** Answers arrive out of order.
3. **Add timeouts.** The other side can hang or crash.
4. **Separate requests from notifications.** Not everything needs an answer.
5. **Be strict when sending, tolerant when receiving.** NOVA ignores unknown messages instead of crashing.
6. **Make states explicit.** The debugger moves between *not debugging, running, paused, ended*:

{{fig:debug-states|The debugger is a small state machine. Buttons are enabled or disabled depending on the current state.}}

::: why Why model the debugger as states?
You can only step when the program is *paused*. Pressing Step while it is *running* makes no sense. Writing the states down means every button simply asks "what state am I in?" instead of a tangle of true/false variables. NOVA's `dbgUi` function sets all the toolbar buttons from the two flags `D.on` and `D.running`.
:::

## Mini assignments

::: assign Assignment 13.1: Settings round trip
Write a Node script that loads `settings.json` (create defaults if missing), changes the font size, and saves it back with 2-space indent. If the file contains invalid JSON, print a message and use defaults.
:::

::: assign Assignment 13.2: Strip and count
Take a string with ANSI colour codes in it. Strip the codes and count how many red (`31`) and green (`32`) sections there were.
:::

::: assign Assignment 13.3: Frame stress test
Using `makeReader`, send 100 random messages split into random-sized chunks (1 to 20 bytes). Check that all 100 arrive intact and in order.
:::

::: assign Assignment 13.4: Parse more MI
Extend the MI parser to decode `^done,variables=[{name="x",value="1"},{name="y",value="2"}]` into `{ x: "1", y: "2" }`. Add a function `framesFrom(record)` that returns an array of `{ func, line }`.
:::

## Big assignment: A fake language server

::: big "EchoLS": your own tiny LSP server
**Goal.** Write a real, working language server in Node that speaks LSP over stdin/stdout, then connect NOVA-style client code to it.

**Requirements.**

1. The server reads framed JSON-RPC messages from `process.stdin` using a reader like `makeReader`, and writes framed replies to `process.stdout`.
2. It implements `initialize` (reply with capabilities `{ hoverProvider: true, completionProvider: {} }`), `initialized`, `shutdown`, `exit`.
3. It keeps a map of open documents from `didOpen` / `didChange` / `didClose`.
4. `textDocument/hover` returns the **word under the cursor** and how many times it appears in the file.
5. `textDocument/completion` returns every distinct word in the document that starts with the word being typed.
6. After every `didOpen` and `didChange` it sends `publishDiagnostics` with a **warning on every line containing the text `TODO`** and an **error on any line longer than 100 characters**.
7. Write a second script, the **client**, that starts the server with `spawn`, performs the handshake, opens a document, asks for hover and completion at known positions, prints the answers, and shuts down cleanly. Use a `pending` map with ids and timeouts.
8. Handle unknown methods by replying with an error (`code -32601`) for requests and ignoring notifications.
9. Remember positions are **0-based**.

**Stretch goals.** Add `textDocument/definition` that jumps to the first place a word appears. Plug your server into a tiny Monaco page. Better still: change `lsp.js` in NOVA to start your server for `.txt` files and watch your own warnings appear as wavy lines.

**How to check yourself.** Pipe a hand-written framed message into your server in a terminal and read the framed answer. The headers' lengths must match the byte counts exactly.
:::

## Quiz

::: quiz
1. Name the six kinds of JSON value.
2. Why can a pipe not simply carry "one message per read"?
3. How does LSP frame a message?
4. Why must the header count bytes, not characters?
5. What are the three kinds of JSON-RPC message?
6. What is the `id` for?
7. What does the first character of a GDB/MI line tell you? Give two examples.
8. Why does a parser for MI need recursion?
9. What is an ANSI escape sequence?
10. Why does the terminal need a PTY, while the Run panel gets by with pipes?
11. Why must LSP positions be converted for Monaco?
:::

::: answers
1. String, number, boolean, null, array, object.
2. Pipes are plain byte streams; a read can contain half a message or several. You need a rule (framing) to split them.
3. A `Content-Length: N` header, a blank line, then exactly N bytes of JSON.
4. Because the reader counts bytes on the wire; multi-byte characters would otherwise make it cut in the wrong place.
5. Request, response and notification.
6. To match an answer to its question, since many requests may be pending at once and answers can arrive in any order.
7. The message kind, e.g. `^` result of a command, `*` async event such as `*stopped`, `~` console text.
8. Values nest to any depth (tuples inside lists inside tuples) and only a self-calling function can follow that.
9. A hidden byte sequence starting with ESC that tells a terminal to colour text, move the cursor, clear the screen, etc.
10. Interactive programs behave properly only if they think they talk to a terminal; the Run panel only shows output from your own program.
11. LSP is 0-based and Monaco is 1-based; without conversion every position is off by one.
:::
