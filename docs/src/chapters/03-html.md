@@part Part II — The Languages

# HTML: The Skeleton

## What HTML is, in one minute

Every web page you have ever seen, and the window of every Electron program, starts as an **HTML** file. HTML tells the computer **what things are on the page**: "this is a heading", "this is a button", "this is a list".

::: def HTML
**HyperText Markup Language.** *Markup* means you wrap pieces of text in **tags** that label them. HTML does not calculate or decide. It only describes.
:::

Think of a school report. The title is big and centred. Below are paragraphs. There may be a table and a picture. HTML is the way you tell the computer "this part is the title, this part is a paragraph, this part is a table".

## Your first page

Create a file called `hello.html`. Type this exactly:

```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>My First Page</title>
</head>
<body>
  <h1>Hello, NOVA!</h1>
  <p>This is my first web page.</p>
</body>
</html>
```

Double-click the file. Your browser opens it. You just made a page.

Let's read it line by line.

| Line | Meaning |
|------|---------|
| `<!doctype html>` | "This page uses modern HTML." Always the first line. |
| `<html lang="en">` | The root. Everything lives inside it. `lang` says the page is in English. |
| `<head>` | Information *about* the page that is not shown in the window. |
| `<meta charset="utf-8" />` | Tells the browser how to read letters, so that symbols and other languages show correctly. |
| `<title>` | The name shown on the browser tab. |
| `<body>` | Everything that you actually see. |
| `<h1>` | A top-level heading. |
| `<p>` | A paragraph. |

::: why Why do tags come in pairs?
A tag like `<p>` is an **opening tag** and `</p>` is the **closing tag**. The pair says where the paragraph starts and where it ends. Without the closing tag the browser would have to guess, and it would often guess wrong, making the whole page strange. The text between is the **content**. The whole thing together is an **element**.
:::

::: whatif What if you forget `<!doctype html>`?
The browser switches to **quirks mode**, an old compatibility mode from the 1990s. Sizes and spacing behave differently, and your page looks subtly wrong. This is a good example of a tiny line that prevents a big headache. NOVA's `index.html` starts with it.
:::

## The tree behind the page

When the browser reads your HTML it builds a **tree** in memory called the **DOM** (Document Object Model). Every element is a branch, every piece of text a leaf.

{{fig:dom-tree|The DOM tree. JavaScript can walk this tree and change any branch, which is how NOVA updates the screen.}}

::: why Why does the tree matter?
Because JavaScript does not edit the text of your HTML file. It edits the **tree**. When NOVA opens a tab, JavaScript adds a new branch to the tree, and the browser redraws. Understanding the tree is the key to understanding every web program.
:::

## The elements you will use every day

### Headings and text

```html
<h1>Biggest heading</h1>
<h2>Second level</h2>
<h3>Third level</h3>
<p>A paragraph of text with <b>bold</b>, <i>italic</i> and <code>code</code>.</p>
<br />   <!-- a line break -->
<hr />   <!-- a horizontal line -->
```

Headings go from `h1` (largest, use once) to `h6`. Use them for structure, not just for size. People who use screen readers jump from heading to heading to find their way.

### Lists

```html
<ul>            <!-- unordered: bullets -->
  <li>Editor</li>
  <li>Compiler</li>
  <li>Debugger</li>
</ul>

<ol>            <!-- ordered: numbers -->
  <li>Write</li>
  <li>Build</li>
  <li>Run</li>
</ol>
```

### Links and images

```html
<a href="https://example.com">Click me</a>
<img src="logo.png" alt="The NOVA logo" width="120" />
```

The **attribute** `href` says where a link goes. The attribute `alt` is text shown (or read aloud) if the picture cannot be seen. Never leave `alt` out.

### Buttons and inputs

```html
<button id="btnRun" title="Run (Shift+F10)">Run</button>
<input type="text" id="findQ" placeholder="Find in files…" />
<label><input type="checkbox" id="findCase" /> Match case</label>
<select id="theme">
  <option value="dark">Dark</option>
  <option value="light">Light</option>
</select>
```

This is very close to what NOVA's toolbar and search box are made of.

### Tables

```html
<table>
  <tr><th>Key</th><th>Action</th></tr>
  <tr><td>Shift+F10</td><td>Run</td></tr>
  <tr><td>Ctrl+F9</td><td>Build</td></tr>
</table>
```

Use tables for **data**, never for page layout. (CSS does layout, Chapter 7.)

::: try Try it
Make a page called `about-me.html` with a heading, two paragraphs, a list of three hobbies and a link to a favourite website. Open it in the browser. Then change one tag to a different one (for example `ul` to `ol`) and refresh.
:::

## Attributes, ids and classes

An **attribute** gives extra information to a tag: `<a href="...">`. Two attributes are the most important in all of web programming:

* `id` gives an element a **unique name**: `<div id="sidebar">`. Only one element on the page may have a given id. JavaScript uses ids to find elements: `document.querySelector('#sidebar')`.
* `class` gives an element a **group name**: `<button class="ib sm">`. Many elements may share a class. CSS uses classes to style whole groups at once.

::: why Why both id and class?
An `id` is like a **passport number**: unique to one person. A `class` is like a **school uniform**: many people wear it. When you need to reach exactly one element (the Run button) use an id. When you want to style many (all small icon buttons) use a class. Mixing them up is a very common beginner error.
:::

### Data attributes

You can invent your own attributes starting with `data-`. NOVA uses them to remember which panel a button opens:

```html
<button class="sb" data-panel="project" title="Project (Alt+1)">…</button>
<button class="sb" data-bottom="problems" title="Problems (Alt+6)">…</button>
```

JavaScript reads them back with `button.dataset.panel`. This lets one small piece of code serve many buttons, instead of writing a separate function for each.

## Page layout with semantic tags

Old pages used `<div>` for everything. Modern HTML has tags that **say what they are**:

```html
<header>  top bar or title area  </header>
<nav>     a group of navigation buttons  </nav>
<aside>   a sidebar  </aside>
<main>    the main content  </main>
<section> a themed group of content  </section>
<footer>  bottom strip  </footer>
```

NOVA's skeleton uses exactly these: a `<header id="titlebar">`, a `<nav class="stripe">`, an `<aside id="sidebar">`, a `<main id="center">`, a `<section id="bottom">` and a `<footer id="status">`. Look back at the anatomy figure in Chapter 2: those seven regions are these tags.

::: why Why use semantic tags instead of div?
For **meaning**. A screen reader can say "navigation" or "main content" and let the user jump there. Search engines understand the page better. And *you* will understand your own file better when you return to it in six months. Meaning costs nothing and helps everyone.
:::

## The hidden attribute, and a real NOVA bug

HTML has a built-in attribute called `hidden`. Any element with `hidden` is not shown:

```html
<div id="bottom" hidden>…the bottom panel…</div>
```

NOVA turns panels on and off by setting `element.hidden = true` or `false` from JavaScript. But there is a trap that cost the team an hour.

::: whatif What if you also give that element a CSS `display` rule?
If your CSS says `#bottom { display: flex; }`, that rule **beats** the browser's built-in "hidden means display none" rule, and the panel stays visible even though it has the `hidden` attribute! The fix is one line of CSS that makes `hidden` always win:

`[hidden] { display: none !important; }`

This line is the very first line of NOVA's `styles.css`. Now you know why.
:::

## Scripts and style sheets

An HTML file loads other files with two tags:

```html
<link rel="stylesheet" href="styles.css" />   <!-- the CSS (looks) -->
<script src="app.js"></script>                <!-- the JavaScript (behaviour) -->
```

Where you put the `<script>` matters. NOVA's scripts sit at the **bottom** of the body. Why? Because the browser reads the page top to bottom. If a script runs before the elements it needs exist, `document.querySelector` finds nothing. At the bottom, everything above already exists.

::: why Why does the order of NOVA's scripts matter?
In NOVA's `index.html`, the xterm terminal library is loaded **before** Monaco's loader. The reason is subtle: Monaco's loader creates a global function called `define`. The xterm library checks for `define` and, if it finds one, registers itself in a different way that does not create the global `Terminal` that NOVA needs. Loading xterm first avoids the clash. This was a real bug found during development, and it is a good lesson: **order of scripts can change behaviour**.
:::

## SVG: drawing with text

**SVG** (Scalable Vector Graphics) lets you draw shapes with HTML-like tags. Every icon in NOVA and every diagram in this book is SVG. It is just text, it stays sharp at any size, and you can colour it from CSS.

```html
<svg viewBox="0 0 120 60" width="240">
  <rect x="5" y="5" width="110" height="50" rx="8" fill="white" stroke="black" stroke-width="3"/>
  <circle cx="30" cy="30" r="12" fill="black"/>
  <text x="50" y="36" font-size="16">NOVA</text>
  <path d="M95 20 l10 10 l-10 10" fill="none" stroke="black" stroke-width="3"/>
</svg>
```

NOVA's icons are described by a single `path` string such as `M4.5 3l8 5-8 5z` (a triangle: *move to* a point, *line to* the next two, *close*). The whole icon set in `icons.js` is thirty lines.

::: why Why SVG icons and not picture files?
A picture file (PNG) has a fixed number of pixels: it gets blurry when enlarged and cannot change colour. SVG is maths: it stays crisp at any size, and by writing `stroke="currentColor"` the icon automatically takes the colour of the text around it, so a single icon works in both dark and light themes.
:::

## Accessibility in one page

About one person in six has some disability. Good HTML helps them for free:

* Use real `<button>` tags for buttons (not a styled `<div>`). Keyboard users can reach and press them.
* Always give images `alt` text and buttons a `title` or label.
* Use headings in order.
* Make sure text has strong contrast against its background.
* Make everything usable with the keyboard alone. (A programming tool should! NOVA has shortcuts for almost everything.)

## Mini assignments

::: assign Assignment 6.1: Business card
Build `card.html` for yourself: name as `h1`, job title as `h2`, a photo with `alt` text, a list of skills, and a link to an email address (`href="mailto:you@example.com"`).
:::

::: assign Assignment 6.2: A table of shortcuts
Make a table of five keyboard shortcuts from Appendix B with columns *Shortcut*, *Action*, *Where it works*. Use `<th>` for the header row.
:::

::: assign Assignment 6.3: Your own icon
Draw a simple icon (a house, a gear or a folder) in SVG inside a 16 by 16 `viewBox`. Use only `path`, `rect` and `circle`. Colour it with `stroke="currentColor"` and then wrap it in a `<button>`.
:::

## Big assignment: The skeleton of a mini IDE

::: big Build "MiniIDE" in pure HTML
**Goal.** Create `miniide.html`, a page that has the *structure* of an IDE but no colours or behaviour yet. You will style it in Chapter 7 and bring it to life in Chapter 8.

**Requirements.**

1. A `<header>` with a logo (inline SVG), a project name and three `<button>` elements: Run, Build and Stop.
2. A `<nav>` with four icon buttons (Project, Search, Problems, Terminal). Each carries a `data-panel` attribute.
3. An `<aside>` containing a nested list (`ul` inside `ul`) that looks like a project tree: a `src` folder with `main.cpp` and `util.cpp`, an `include` folder with `util.h`, and a `CMakeLists.txt`.
4. A `<main>` with a row of three tab elements and a `<pre><code>` block holding a small C++ program.
5. A `<section>` at the bottom with four tab buttons and four panels (Run, Problems, Find, Terminal). Only the Run panel has no `hidden` attribute.
6. A `<footer>` showing line, column, and the word "UTF-8".
7. Every interactive element has an `id` or a `data-` attribute that JavaScript will be able to find later.

**Stretch goals.** Add a `<dialog>` element for a Settings window that contains a `<select>` for theme and a `<input type="number">` for font size. Add `aria-label` attributes to every icon-only button.

**How to check yourself.** Open the file with no CSS. It will look plain, like a 1995 web page, but a person should be able to read it from top to bottom and understand the layout. Compare it with `renderer/index.html` in the NOVA project.
:::

## Quiz

::: quiz
1. What is the difference between an opening tag and a closing tag?
2. What is the DOM?
3. When would you use `id` and when `class`?
4. Why is `<!doctype html>` the first line?
5. Why should you use `<button>` rather than a styled `<div>`?
6. What is a `data-` attribute used for?
7. Give two benefits of SVG icons over PNG files.
8. A panel has the `hidden` attribute but is still visible. What is the most likely cause?
9. Why does NOVA load xterm.js before Monaco's loader?
10. What does `alt` do on an image?
:::

::: answers
1. The opening tag (`<p>`) starts an element and the closing tag (`</p>`) ends it.
2. The Document Object Model: the tree the browser builds in memory from your HTML. JavaScript changes the tree.
3. Use `id` for one unique element; use `class` for a group of elements that share a style or behaviour.
4. It makes the browser use modern mode instead of the old "quirks mode", which changes sizes and spacing.
5. A real button works with the keyboard and screen readers automatically.
6. To store your own small pieces of information on an element so JavaScript can read them (`element.dataset`).
7. They stay sharp at any size and can take on the text colour (`currentColor`), so one icon works in all themes. They are also tiny.
8. A CSS `display` rule on that element overrides the browser's default for `hidden`. Fix with `[hidden] { display: none !important; }`.
9. Monaco's loader defines a global `define`, which makes xterm register as a module instead of creating the global `Terminal`.
10. It gives replacement text for people who cannot see the image (or if the image fails to load).
:::
