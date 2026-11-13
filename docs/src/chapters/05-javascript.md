# JavaScript: The Brain

## What JavaScript is

**JavaScript** (JS) is the language that makes things *happen*. HTML says what is on the page and CSS says how it looks, but only JavaScript can say *"when the user clicks Run, do this"*. Most of NOVA is JavaScript: about two thousand lines.

JavaScript began in 1995 as a quick scripting tool for web pages. Today it runs in browsers, on servers (Node.js), in desktop apps (Electron), on phones and even on tiny devices. It is one of the most used languages in the world.

::: def Script
A **script** is a program that is run directly from its text by an interpreter or engine, without you producing a separate program file first.
:::

## Running JavaScript

There are two easy places to run it.

1. **The browser console.** Press F12 in any browser, click *Console*, and type. Great for quick tests.
2. **Node.js.** Save code in a file such as `test.js` and run `node test.js` in a terminal. All the runnable examples in this chapter work this way, and the outputs shown were produced by really running them while this book was made.

Your first program:

```js run
console.log("Hello, NOVA!");
console.log(2 + 3);
```

`console.log` prints a value. It is your best friend for finding out what your program is doing.

## Variables and types

A **variable** is a labelled box that holds a value. Use `const` for values that never change and `let` for values that will.

```js run
const name = "NOVA";        // text (a "string")
let version = 1;            // a number
const isFree = true;        // true or false (a "boolean")
const nothing = null;       // "on purpose, there is nothing"
let notSet;                 // undefined: "nobody gave me a value"

version = version + 1;      // allowed: let can change
console.log(name, version, isFree, nothing, notSet);
console.log(typeof name, typeof version, typeof isFree);
```

::: why Why prefer const?
When you read `const total = 5`, you know instantly that `total` never changes, so you can stop worrying about it. Fewer things that can change means fewer places for a bug to hide. NOVA's code uses `const` almost everywhere and `let` only for things that must change, such as a counter.
:::

::: warn Watch out
Older JavaScript used `var`. It has confusing scope rules. You will see it in old code but should not write it in new code.
:::

### Strings and template strings

```js run
const lang = "C++";
const n = 3;
console.log("I like " + lang + " " + n + " times");   // the old, clumsy way
console.log(`I like ${lang} ${n} times`);              // template string: much nicer
console.log(`Two plus two is ${2 + 2}`);              // any expression fits in ${ }
console.log("hello".toUpperCase(), "a,b,c".split(","), "  trim me ".trim());
console.log("NOVA IDE".includes("IDE"), "NOVA".length, "abc".slice(1));
```

Template strings use backticks (the key left of `1`). NOVA uses them constantly to build HTML and messages: `` `${rel(f)}  (${rs.length})` `` makes the file headings in the Find panel.

### Numbers

```js run
console.log(10 / 3);              // 3.3333333333333335
console.log(Math.floor(10 / 3));  // 3
console.log(10 % 3);              // 1 (the remainder)
console.log(0.1 + 0.2);           // 0.30000000000000004  (!)
console.log((0.1 + 0.2).toFixed(2));
console.log(Number("42") + 1, parseInt("12px"), Math.max(3, 9, 4));
```

::: why Why is 0.1 + 0.2 not 0.3?
Computers store numbers in binary. Just as one third cannot be written exactly in decimal (0.3333...), a tenth cannot be written exactly in binary. The tiny error is tolerated in most work. Never compare decimals with `===`. The same behaviour exists in C and C++ (`double`), so you will meet it again.
:::

## Comparing and deciding

```js run
console.log(5 === 5, 5 === "5", 5 == "5");   // three equals: strict. Two: sloppy.
console.log(5 !== 6, 3 < 4, 3 >= 3);
console.log(true && false, true || false, !true);

const age = 17;
if (age >= 18) {
  console.log("adult");
} else if (age >= 13) {
  console.log("teen");
} else {
  console.log("child");
}
console.log(age >= 18 ? "can vote" : "cannot vote yet");   // the "ternary" shortcut
```

::: why Why always ===?
`==` quietly converts types before comparing, so `5 == "5"` is true and `0 == ""` is true, which causes strange bugs. `===` compares both value *and* type and does what you expect. Use it always. (This is a good habit that C and C++ also reward: be exact about types.)
:::

## Loops

```js run
for (let i = 1; i <= 3; i++) {
  console.log("lap", i);
}

let n = 3;
while (n > 0) {
  console.log("countdown", n);
  n--;
}

for (const word of ["editor", "compiler", "debugger"]) {
  console.log("part:", word);
}
```

## Functions

A **function** is a named recipe you can use again and again.

```js run
function add(a, b) {
  return a + b;
}
const multiply = (a, b) => a * b;          // arrow function: shorter
const greet = (name = "friend") => `Hello, ${name}!`;   // default value

console.log(add(2, 3), multiply(4, 5));
console.log(greet(), greet("Team NOVA"));
```

**Why functions?** They stop you repeating yourself and give names to ideas. NOVA's very first helper is a function:

```js
const $ = (s, r = document) => r.querySelector(s);
```

It is a shortcut: instead of writing `document.querySelector('#tabs')` forty times, NOVA writes `$('#tabs')`. The `r = document` is a **default value**: if you do not pass a second argument it searches the whole page.

## Arrays: lists of things

```js run
const parts = ["editor", "tree", "terminal"];
parts.push("debugger");                       // add to the end
console.log(parts.length, parts[0], parts[parts.length - 1]);

const lengths = parts.map(p => p.length);               // transform every item
const longOnes = parts.filter(p => p.length > 5);       // keep some items
const total = lengths.reduce((sum, n) => sum + n, 0);   // boil down to one value
console.log(lengths, longOnes, total);

console.log(parts.find(p => p.startsWith("t")));        // first match
console.log(parts.some(p => p === "tree"), parts.every(p => p.length > 3));
console.log([...parts].sort(), parts.includes("tree"), parts.indexOf("terminal"));
console.log(parts.slice(1, 3), parts.join(" | "));
```

`map`, `filter` and `reduce` are the three most useful tools in JavaScript. Learn them well.

::: why Why map and filter instead of a for loop?
You *can* do the same with a loop. But `map` says *"make a new list by changing each item"* in a single readable line, and it cannot accidentally change the original. NOVA's file search is one line of this style: `files.filter(f => /\.(c|cc|cpp|cxx)$/i.test(f))` picks the C and C++ files out of a project.
:::

## Objects: things with named parts

```js run
const tab = { name: "main.cpp", dirty: false, line: 12 };
tab.dirty = true;                       // change
tab.language = "C++";                   // add
console.log(tab.name, tab["line"], tab);

const { name, line } = tab;             // destructuring: pull parts out
console.log(name, line);

const copy = { ...tab, line: 99 };      // spread: copy and override
console.log(copy.line, tab.line);

console.log(Object.keys(tab));
console.log(JSON.stringify(tab));
console.log(JSON.parse('{"a":1,"b":[2,3]}').b[1]);
```

NOVA's whole state is one object, `S`, with fields such as `S.root`, `S.tabs`, `S.active` and `S.settings`. Having **one place** where the state lives makes it easy to find any fact about the IDE.

::: why Why one big state object?
When the window shows the wrong thing, the first question is "what does the program *believe*?". If every fact lives in one object you can type `S` in the developer tools and read the answer. Scattering state over many variables makes bugs much harder to find. Many professional frameworks (Redux, Vuex) are built on this exact idea.
:::

## Collections: Map and Set

```js run
const seen = new Set([1, 2, 2, 3, 3, 3]);       // each value only once
console.log(seen.size, seen.has(2));

const cache = new Map();                         // key to value, any type of key
cache.set("main.cpp", 120);
cache.set("util.cpp", 45);
console.log(cache.get("main.cpp"), cache.has("x"), [...cache.keys()]);
```

NOVA uses a `Set` for the folders that are open in the tree (`S.expanded`) and a `Map` to remember which breakpoint number gdb gave each line.

## Strings, patterns and regular expressions

A **regular expression** ("regex") is a pattern for matching text. It looks scary, but it is a small tool you learn in parts.

```js run
console.log(/\d+/.exec("line 42 here"));            // finds the digits
console.log("a1b22c333".match(/\d+/g));             // all matches
console.log("2026-11-12".replace(/-/g, "/"));
console.log(/^[A-Z]/.test("NOVA"), /^[A-Z]/.test("nova"));
```

The most useful pieces: `\d` a digit, `\w` a letter/digit/underscore, `\s` a space, `.` any character, `+` one or more, `*` zero or more, `?` optional, `^` start, `$` end, `( )` capture a part, `[abc]` any of these.

### A real NOVA regex: reading compiler errors

When g++ finds a mistake it prints a line like `src/main.cpp:14:9: error: 'x' was not declared`. NOVA must pull that line apart to draw a wavy underline. Here is the real pattern from `app.js`:

```js run
const DIAG_RE = /^((?:[A-Za-z]:)?[^:\n]+?):(\d+):(?:(\d+):)? (fatal error|error|warning|note): (.*)$/;

const lines = [
  "src/main.cpp:14:9: error: 'x' was not declared in this scope",
  "C:/proj/util.cpp:7:1: warning: unused variable 'n'",
  "Building CXX object main.o",
];
for (const line of lines) {
  const m = line.match(DIAG_RE);
  if (m) {
    const [, file, row, col, kind, msg] = m;
    console.log({ file, row: +row, col: +col, kind, msg });
  } else {
    console.log("(not an error line)", line);
  }
}
```

::: why Why the odd bit (?:[A-Za-z]:)?
On Windows a path starts with a drive letter and a colon, like `C:/proj`. But the colon is also the separator between file, line and column! The optional group `(?:[A-Za-z]:)?` says "a drive letter and colon may come first, and that colon belongs to the file name". Without it, `C` would be treated as the file and `/proj/util.cpp` as the line number, and every error on a Windows path would be lost. This is a bug the team met on the very first test.
:::

## Scope and closures

A variable lives in the **scope** where it was created: inside `{ }`, inside a function, or global. An inner function can see the variables of every box around it. A function that **remembers** variables from where it was created is called a **closure**.

{{fig:scope-chain|An inner function can see outwards through every enclosing box. That is a closure.}}

```js run
function makeCounter() {
  let count = 0;                 // private to makeCounter
  return () => ++count;          // the inner function remembers count
}
const a = makeCounter();
const b = makeCounter();
console.log(a(), a(), a(), b());   // each counter has its own count
```

::: why Why do closures matter?
They let you keep private data and build tools that "remember". NOVA's `splitter` function creates one dragging behaviour for each divider, and each one remembers *which* divider it belongs to. Closures are also why event handlers work: the handler remembers the tab it was made for.
:::

## Classes (a short look)

```js run
class Tab {
  constructor(name) { this.name = name; this.dirty = false; }
  edit() { this.dirty = true; return this; }
  label() { return this.dirty ? `${this.name} ●` : this.name; }
}
const t = new Tab("main.cpp");
console.log(t.label());
console.log(t.edit().label());
```

NOVA mostly uses plain objects and functions, which is a style many JavaScript programmers prefer for small programs. You will see classes in the C++ chapter, where they are central.

## When things go wrong: errors

```js run
function parsePort(text) {
  const n = Number(text);
  if (!Number.isInteger(n) || n < 1 || n > 65535) throw new Error("Bad port: " + text);
  return n;
}
try {
  console.log(parsePort("8080"));
  console.log(parsePort("hello"));
} catch (e) {
  console.log("Caught:", e.message);
} finally {
  console.log("done");
}
```

`throw` raises an error; `try/catch` handles it so the program does not crash. NOVA wraps file opening in `try/catch` so that opening a broken file shows a friendly message instead of freezing the IDE.

## Time: the event loop, callbacks, promises and async/await

This is the hardest and most important part of JavaScript. Take it slowly.

### One thing at a time

JavaScript can do **only one thing at a time**. Imagine a single waiter in a restaurant. If the waiter stood at the kitchen for ten minutes waiting for one soup, nobody else would be served. Instead the waiter drops the order at the kitchen and serves other tables. When the soup is ready, a bell rings.

In JavaScript the slow jobs (reading a file, waiting for a timer, a network request) are handed away. When they finish, their **callback** waits in a queue. The **event loop** checks over and over: *"is the waiter free? Then serve the next callback."*

{{fig:event-loop|The event loop. Slow jobs run elsewhere. Finished jobs queue up, and the loop feeds them one at a time to the call stack.}}

```js run
console.log("1 start");
setTimeout(() => console.log("4 timer (even with 0 delay)"), 0);
Promise.resolve().then(() => console.log("3 promise"));
console.log("2 end");
```

Study the output. Even a timer with zero delay waits until the current code has finished. Promises jump the queue and run before timers.

::: why Why does this matter for an IDE?
If NOVA read a big file with a method that *blocked* the waiter, the whole window would freeze until the disk finished, and you could not click, type or scroll. Because every disk, process and message operation is **asynchronous**, the window always stays alive. This is the single biggest reason an Electron IDE feels smooth.
:::

### Callbacks

The oldest style: pass a function to be called later.

```js run
function fakeRead(file, done) {
  setTimeout(() => done(null, "contents of " + file), 100);
}
fakeRead("main.cpp", (err, text) => {
  console.log(err, text);
});
console.log("waiting...");
```

The trouble comes when jobs depend on each other: callback inside callback inside callback (nicknamed "callback hell").

### Promises

A **promise** is an IOU: "I do not have the value yet, but I will either give you one or tell you why not."

{{fig:promise-states|A promise is pending, then becomes fulfilled with a value or rejected with an error.}}

```js run
const wait = (ms) => new Promise(resolve => setTimeout(resolve, ms));

wait(100)
  .then(() => { console.log("step 1"); return 42; })
  .then(n => { console.log("step 2 got", n); throw new Error("oops"); })
  .catch(e => console.log("caught", e.message));
```

### async and await

`async/await` is a nicer way to write promises so that asynchronous code reads like normal top-to-bottom code.

```js run
const wait = (ms, v) => new Promise(r => setTimeout(() => r(v), ms));

async function build() {
  console.log("configuring...");
  const cfg = await wait(100, "config ok");     // pause THIS function, not the program
  console.log(cfg);
  const exe = await wait(100, "hello.exe");
  console.log("built", exe);
  return exe;
}
build().then(r => console.log("finished:", r));
console.log("(other work keeps running)");
```

This is exactly the shape of NOVA's `runPipeline`: configure, `await` it, then build, `await` that, then run. Here is a piece of the real code:

```js
async function openFile(path, pos) {
  let tab = S.tabs.find(t => key(t.path) === key(path));
  if (!tab) {
    let r; try { r = await nova.read(path); } catch (e) { return toast('Cannot open: ' + e.message); }
    ...
```

::: whatif What if you forget await?
You get a **promise object** instead of the value. `const r = nova.read(path)` would make `r` a promise, and `r.text` would be `undefined`. The code runs without an error and does the wrong thing. If a value looks like `Promise { <pending> }` you forgot an `await`.
:::

### Running many things at once

```js run
const wait = (ms, v) => new Promise(r => setTimeout(() => r(v), ms));
async function main() {
  const t0 = Date.now();
  const results = await Promise.all([wait(200, "A"), wait(200, "B"), wait(200, "C")]);
  console.log(results, Date.now() - t0 < 400 ? "ran together" : "ran one by one");
}
main();
```

## Useful tools for a real program

### Debouncing: wait until the user stops

Every key you press changes the file, but you do not want to bother the language server after each letter. A **debounce** waits until typing pauses.

```js run
function debounce(fn, ms) {
  let timer;
  return (...args) => {
    clearTimeout(timer);                       // cancel the earlier plan
    timer = setTimeout(() => fn(...args), ms); // make a new plan
  };
}
const send = debounce((text) => console.log("sent:", text), 120);
send("s"); send("st"); send("std"); send("std::");   // four quick "keystrokes"
```

Only one message is sent. This is the idea behind `lspDidChange` in NOVA, which waits 120 milliseconds.

::: why Why does debounce use a closure?
The variable `timer` must survive between calls so that the next call can cancel the previous timer. The closure keeps it alive and private. This is a perfect, tiny example of why closures exist.
:::

### Optional chaining and defaults

```js run
const config = { editor: { font: "Consolas" } };
console.log(config.editor?.font, config.terminal?.font, config.terminal?.font ?? "default");
```

`?.` stops safely if something is missing; `??` supplies a default only if the value is `null` or `undefined`. You will see both through NOVA's code.

## JavaScript in the browser: the DOM

Everything above works in Node. In a browser window you also have the **DOM** (Chapter 6) to read and change the page. These examples need a page, so you cannot run them with Node. Type them into the console of any web page.

```js
const btn = document.querySelector('#btnRun');          // find one element
const all = document.querySelectorAll('.tab');          // find many

const div = document.createElement('div');              // make a new element
div.className = 'tab active';
div.textContent = 'main.cpp';
document.querySelector('#tabs').appendChild(div);       // put it in the page

btn.addEventListener('click', () => console.log('Run!'));   // react to a click
document.documentElement.dataset.theme = 'light';           // set an attribute
```

An **event** is something the user (or the system) does: click, key press, mouse move, resize. An **event listener** is a function that waits for it.

### Event delegation

Imagine a list with a thousand tabs. Giving each its own listener is wasteful. Instead put **one** listener on the parent and check which child was clicked:

```js
document.querySelector('#tabs').addEventListener('click', (e) => {
  const tab = e.target.closest('.tab');
  if (tab) console.log('clicked', tab.textContent);
});
```

NOVA's Run console uses this: one listener on the whole output area catches clicks on any file link in a compiler message and opens that file at that line.

## Modules (a short look)

Large programs are split into files. In Node you can write:

```js
const fs = require('fs');                    // load a built-in module
const { join } = require('path');            // load just one function
module.exports = { myFunction };             // share from your own file
```

NOVA's renderer files do not use modules. They are plain `<script>` tags that share one global scope, which is the simplest approach for a small tool. Chapter 9 shows Node's modules in action.

## Worked project: a tiny interpreter

It is time to put the pieces together and build what Chapter 3 described: a **lexer**, a **parser** and an **evaluator** for sums such as `1 + 2 * 3`. This runs in Node.

```js run
// 1. LEXER: text -> tokens
function tokenize(src) {
  const tokens = [];
  let i = 0;
  while (i < src.length) {
    const c = src[i];
    if (/\s/.test(c)) { i++; continue; }
    if (/[0-9.]/.test(c)) {
      let n = "";
      while (i < src.length && /[0-9.]/.test(src[i])) n += src[i++];
      tokens.push({ type: "num", value: parseFloat(n) });
      continue;
    }
    if ("+-*/()".includes(c)) { tokens.push({ type: c }); i++; continue; }
    throw new Error("Unexpected character: " + c);
  }
  return tokens;
}

// 2. PARSER: tokens -> tree
function parse(tokens) {
  let pos = 0;
  const peek = () => tokens[pos];
  const next = () => tokens[pos++];
  function expression() {                       // handles + and -
    let left = term();
    while (peek() && (peek().type === "+" || peek().type === "-")) {
      const op = next().type;
      left = { op, left, right: term() };
    }
    return left;
  }
  function term() {                             // handles * and /  (binds tighter)
    let left = factor();
    while (peek() && (peek().type === "*" || peek().type === "/")) {
      const op = next().type;
      left = { op, left, right: factor() };
    }
    return left;
  }
  function factor() {                           // numbers, (brackets), negatives
    const t = next();
    if (!t) throw new Error("Unexpected end");
    if (t.type === "num") return { num: t.value };
    if (t.type === "-") return { op: "neg", left: factor() };
    if (t.type === "(") {
      const e = expression();
      const close = next();
      if (!close || close.type !== ")") throw new Error("Missing )");
      return e;
    }
    throw new Error("Unexpected token " + t.type);
  }
  const tree = expression();
  if (pos < tokens.length) throw new Error("Extra input");
  return tree;
}

// 3. EVALUATOR: walk the tree
function evaluate(n) {
  if ("num" in n) return n.num;
  if (n.op === "neg") return -evaluate(n.left);
  const a = evaluate(n.left), b = evaluate(n.right);
  switch (n.op) {
    case "+": return a + b;
    case "-": return a - b;
    case "*": return a * b;
    case "/": return a / b;
  }
}

const run = (src) => evaluate(parse(tokenize(src)));
console.log(run("1 + 2 * 3"));
console.log(run("(1 + 2) * 3"));
console.log(run("10 / 4 - -2"));
console.log(JSON.stringify(parse(tokenize("1 + 2 * 3"))));
try { run("2 * (3 + "); } catch (e) { console.log("error:", e.message); }
```

::: why Why are there two functions, expression and term?
That is how the parser knows `*` happens before `+`. `expression` handles the **low-priority** operators and calls `term` for each piece. `term` handles the **high-priority** operators and calls `factor`. So `2 * 3` is gathered into one node *before* `expression` sees the plus sign. Each level of priority gets its own function. This technique is called **recursive descent**, and it is how many real compilers read code.
:::

## Mini assignments

::: assign Assignment 8.1: FizzBuzz
Print the numbers 1 to 30. For multiples of 3 print `Fizz`, for multiples of 5 print `Buzz`, and for both print `FizzBuzz`.
:::

::: assign Assignment 8.2: Word counter
Write `countWords(text)` that returns an object such as `{ the: 3, cat: 1 }`. Make it ignore upper and lower case and punctuation. *Hint: `text.toLowerCase().match(/\w+/g)` and a loop or `reduce`.*
:::

::: assign Assignment 8.3: Parse a compile error
Extend the `DIAG_RE` example: write `summarise(lines)` that takes an array of g++ output lines and returns `{ errors: N, warnings: N, files: [..unique names..] }`.
:::

::: assign Assignment 8.4: Retry with promises
Write `retry(fn, times)` that calls an async function and, if it fails, tries again up to `times` times, waiting longer each time (100ms, 200ms, 400ms). Test it with a function that fails twice and then succeeds.
:::

::: assign Assignment 8.5: Add variables to the interpreter
Teach the interpreter to understand `x = 5` and then `x * 2`. You will need a new token type for names, a place to store values (a `Map`) and a new tree node.
:::

## Big assignment: Bring MiniIDE to life

::: big MiniIDE: behaviour in JavaScript
**Goal.** Take your styled `miniide.html` from Chapter 7 and make it work. No libraries. About 250 lines of JavaScript.

**Requirements.**

1. **A fake project.** Store a project in an object: `{ "src/main.cpp": "…code…", "src/util.cpp": "…", "include/util.h": "…", "CMakeLists.txt": "…" }`. Draw the tree from it dynamically (create elements from the object, do not type the list by hand).
2. **Tabs.** Clicking a file opens it in a tab (or switches to its tab if already open). Tabs can be closed with a button. The editor area shows the file's text in a `<textarea>`.
3. **Dirty marker.** Editing a file shows a dot on its tab. Ctrl+S "saves" it back into the project object and removes the dot.
4. **Run button.** Pressing it appends fake build output line by line using `async/await` and a `wait()` helper. Show a spinner in the status bar while it runs. A Stop button cancels the run (you will need a flag that the loop checks).
5. **Find in files.** A search box scans every file's text and lists matches as *file:line text*. Clicking a result opens that file at that line.
6. **Problems.** Scan the code for the text `TODO` or the word `error` and list those lines in a Problems panel with a count badge.
7. **Theme and settings.** A toggle switches the theme by setting `data-theme`. Save the theme and font size to `localStorage` and restore them on load.
8. **Keyboard shortcuts.** Ctrl+S (save), Shift+F10 (run), Ctrl+W (close tab).
9. **One state object** named `S` holds everything. Typing `S` in the console shows the whole IDE.
10. **Debounce** the find-in-files search so it runs 200ms after typing stops.

**Stretch goals.** Add a command palette that opens with Ctrl+Shift+A and fuzzy-matches actions. Add `contenteditable` syntax colouring using a regular expression for keywords. Save the project object to `localStorage` so it survives a refresh.

**How to check yourself.** Open the browser developer console. There should be **no red errors** during any of the actions. Compare how NOVA's `openFile`, `renderTabs` and `doFind` (in `app.js`) do the same jobs.
:::

## Quiz

::: quiz
1. What is the difference between `let` and `const`?
2. Why is `===` preferred over `==`?
3. What do `map`, `filter` and `reduce` each do?
4. What is a closure? Give one use.
5. In what order does the output appear: a `setTimeout` of 0, a `Promise.then`, and a normal `console.log`?
6. What does `await` do, and what happens if you forget it?
7. Why does NOVA never block while reading a file?
8. What is event delegation and why use it?
9. Why does the NOVA error-line regex contain `(?:[A-Za-z]:)?`
10. What does a debounce do and why does it need a closure?
:::

::: answers
1. `let` can be reassigned; `const` cannot. Prefer `const` so fewer things can change.
2. `===` compares value and type with no hidden conversion; `==` converts types and gives surprising results.
3. `map` changes every item into a new list; `filter` keeps some items; `reduce` combines all items into one value.
4. A function that remembers variables from where it was made. Uses: private data (a counter), debounce, event handlers.
5. Normal `console.log` first, then the promise, then the timeout.
6. It pauses the async function until the promise finishes and gives the value. Without it you get a pending promise object instead of the value.
7. File operations are asynchronous: the work happens elsewhere, and the program is free until the result arrives, so the window stays responsive.
8. Putting one listener on a parent and checking `e.target` instead of one on each child. It is cheaper and also works for elements added later.
9. On Windows a path can start with a drive letter and a colon (`C:`), and without this optional group the colon would be mistaken for the separator between file and line number.
10. A debounce waits until calls stop for a short time and then runs once. It needs a closure so the timer variable survives between calls and can be cancelled by the next call.
:::
