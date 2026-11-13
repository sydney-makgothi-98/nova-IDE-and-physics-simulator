# CSS: The Skin

## What CSS does

If HTML is the skeleton, **CSS** is the skin, clothes and make-up. It decides colours, sizes, spacing, fonts and where things sit on the screen.

::: def CSS
**Cascading Style Sheets.** A list of **rules**. Each rule says: *for elements that match this selector, set these properties*. "Cascading" means that when rules clash, there is a fair order for deciding which one wins.
:::

## Your first rule

```css
h1 {
  color: black;
  font-size: 28pt;
}
```

Parts of a rule:

* `h1` is the **selector**: which elements to style.
* Between the braces `{ }` is the **declaration block**.
* `color: black;` is one **declaration**: a **property** (`color`), a colon, a **value** (`black`) and a semicolon.

Three ways to add CSS to HTML:

1. **External file** (best): `<link rel="stylesheet" href="styles.css" />`. NOVA does this.
2. **A `<style>` block** inside the page head.
3. **Inline** on one tag: `<p style="color:red">`. Quick but messy.

::: why Why keep CSS in its own file?
Because one file can style many pages, the browser remembers (caches) it, and you change the look of the whole program in one place. Mixing CSS into HTML is like writing instructions on every single brick of a house.
:::

## Selectors: choosing what to style

| Selector | Picks | Example |
|---|---|---|
| `p` | every `<p>` | `p { margin: 0; }` |
| `.tab` | every element with class `tab` | `.tab { padding: 0 12px; }` |
| `#sidebar` | the one element with id `sidebar` | `#sidebar { width: 280px; }` |
| `.tab.active` | has *both* classes | `.tab.active { color: black; }` |
| `.node:hover` | when the mouse is over it | `.node:hover { background: grey; }` |
| `#tabs .tab` | a `.tab` *inside* `#tabs` | |
| `button:disabled` | disabled buttons | `opacity: .35;` |
| `li:nth-child(even)` | every second item | zebra stripes |

::: try Try it
Make a page with six `<li>` items. Write a rule that gives every even item a grey background using `:nth-child(even)`, and a rule that makes an item bold when the mouse is over it.
:::

## Units and colours

**Sizes**: `px` (pixels, fixed), `%` (percentage of the parent), `em`/`rem` (relative to font size), `vh`/`vw` (percent of the window height/width), `pt` (points; used in this book for text).

**Colours** can be names (`black`), hex codes (`#3574f0`), or `rgb(53, 116, 240)`. A hex code is red, green and blue written in base 16: `#RRGGBB`. An 8-digit code adds transparency: `#ffffff1c` is white at about 11 percent opacity (NOVA uses it for hover highlights).

## The box model: everything is a box

This is the single most important idea in CSS. **Every element is a rectangular box** made of four layers.

{{fig:box-model|The CSS box model. Total width = content + padding + border + margin.}}

```css
.node {
  padding: 4px 8px;        /* space INSIDE the border */
  border: 1px solid grey;  /* the line */
  margin: 2px 0;           /* space OUTSIDE the border */
}
```

::: why Why does NOVA say box-sizing: border-box?
By default, `width: 100px` sets only the **content** width, and padding and border are added on top, so the box ends up wider than 100px. That surprises everyone. One of the first rules in NOVA's CSS is `* { box-sizing: border-box; }`, which makes `width` include padding and border. A 100px box stays 100px. Almost every modern site starts with this line.
:::

::: whatif What if you skip it?
Imagine a panel set to `width: 100%` with `padding: 10px`. Without `border-box` it is 100% **plus** 20px wide, so it sticks out past the window and a horizontal scrollbar appears. This is one of the most common layout bugs in the world.
:::

## Variables: change the whole look in one place

CSS has **custom properties** (variables). You define them once and use them everywhere with `var()`:

```css
:root {
  --bg: #1e1f22;      /* main background */
  --text: #dfe1e5;    /* text colour */
  --accent: #3574f0;  /* blue highlights */
}
body  { background: var(--bg); color: var(--text); }
.btn.primary { background: var(--accent); }
```

`:root` means "the top of the page", so the variables are available everywhere. Now watch the trick that gives NOVA its themes:

```css
:root[data-theme="light"] {
  --bg: #ffffff;
  --text: #1e1f22;
}
```

When JavaScript sets `document.documentElement.dataset.theme = 'light'`, the second block takes over, every `var(--bg)` changes, and the entire IDE is light. No other code is needed.

::: why Why variables instead of typing the colours everywhere?
NOVA uses its background colour in dozens of places. With variables, changing it is a one-line edit. Without them you would hunt through the file, miss three spots, and end up with an ugly mismatch. Variables are also what make themes (dark, light, high-contrast) cheap to add.
:::

## Layout, part 1: Flexbox

**Flexbox** arranges items in a **row or a column** and spreads out the space for you. It is how almost every toolbar is made.

```css
.toolbar {
  display: flex;                 /* turn flexbox on for the children */
  align-items: center;           /* line them up vertically */
  justify-content: space-between;/* push first and last to the edges */
  gap: 6px;                      /* space between items */
}
```

{{fig:flexbox|Three flexbox settings. justify-content moves items along the main axis; flex-direction turns the axis.}}

Key flexbox properties:

| Property | What it controls |
|---|---|
| `display: flex` | Starts flexbox on the parent |
| `flex-direction` | `row` (sideways) or `column` (downwards) |
| `justify-content` | Spacing along the main axis |
| `align-items` | Alignment across the main axis |
| `gap` | Space between items |
| `flex: 1` | "Take all the leftover space" |

NOVA's tab bar, status bar, toolbar, bottom-panel header and the Debug panel's button row are all flexbox. The central column (`#center`) uses `display: flex; flex-direction: column` so the tabs sit on top of the editor, which has `flex: 1` and grows to fill the rest.

## Layout, part 2: Grid

**Grid** places things in **rows and columns at once**. It is perfect for whole-window layouts.

```css
#app {
  display: grid;
  grid-template-rows: 40px 1fr auto auto 24px;  /* title, workspace, splitter, bottom, status */
  height: 100vh;
}
#workspace {
  display: grid;
  grid-template-columns: 40px 280px 4px 1fr;    /* stripe, sidebar, splitter, editor */
}
```

The magic unit is `fr` (a *fraction* of the free space). `1fr` means "whatever is left after the fixed sizes".

{{fig:grid-layout|NOVA's window is a grid. Fixed rows and columns surround one flexible area, the editor.}}

::: why Why 1fr for the editor?
When you resize the window, the title bar must stay 40px tall and the stripe 40px wide, while the editor absorbs all the change. `1fr` expresses exactly that. Doing this with percentages would need maths and would break at odd sizes.
:::

### CSS variables in the grid: resizable panels

NOVA lets you drag the sidebar edge. The grid width is written as `var(--side-w, 280px)`. While you drag, JavaScript simply changes the variable:

```js
document.documentElement.style.setProperty('--side-w', newWidth + 'px');
```

The grid recalculates by itself. No loops, no manual resizing of children.

## Position, overflow and layers

* `position: absolute; inset: 0;` stretches an element to fill its nearest positioned parent. NOVA's editor and welcome screen both use it so they can sit on top of each other.
* `position: fixed` pins an element to the window (used for popup menus and toasts).
* `overflow: auto` adds scrollbars only when needed.
* `z-index` decides which overlapping element is on top. NOVA's modal overlay uses a high number so it covers the app.

::: whatif What if the sidebar had no min-height: 0?
This is a famous flexbox and grid trap. Flex and grid children refuse to shrink below their content size unless you say `min-height: 0`. Without it, a long file tree would make the sidebar taller than the window and the whole page would scroll instead of the tree. NOVA's CSS sets `min-height: 0` on the workspace and panels for this reason.
:::

## Making it feel alive: transitions and animation

```css
.chip { transition: background .12s; }
.chip:hover { background: var(--hover); }

@keyframes spin { to { transform: rotate(360deg); } }
#stBuild.busy::before { animation: spin .7s linear infinite; }
```

* `transition` smooths a change over time (0.12 seconds is quick but visible).
* `@keyframes` describes an animation. NOVA's little spinner in the status bar is a circle with one transparent side, rotated forever.
* `::before` and `::after` are **pseudo-elements**: extra decoration that CSS can add without touching HTML.

::: why Why are NOVA's animations so short?
Tools are used hundreds of times a day. A half-second fade is charming the first time and annoying the hundredth. Professional tools keep motion under about 150 milliseconds: enough to feel smooth, too short to wait for.
:::

## The cascade: who wins when rules clash?

If two rules set the same property on the same element, the browser picks the winner in this order:

1. `!important` beats everything (use very rarely).
2. **More specific** selectors beat less specific. An id (`#sidebar`) beats a class (`.tab`), which beats a tag (`p`).
3. If still tied, the rule that comes **later** in the file wins.

::: warn Watch out
Do not fight specificity with `!important` everywhere. NOVA uses it in only a handful of places, each for a good reason: forcing `[hidden]` to hide, letting the debugger line highlight and breakpoint dots beat Monaco's own styles, and keeping the terminal background transparent. If you need it twice in a row, rethink the selectors.
:::

## Media: dark and light, and print

CSS can read the user's preferences. For example, a page can follow the system theme:

```css
@media (prefers-color-scheme: dark) {
  :root { --bg: #1e1f22; }
}
```

And it can prepare a page for printing. This very book is made from HTML and CSS: the black-and-white boxes, the 1.5 line spacing and the Times New Roman font come from a style sheet. For example:

```css
body { font-family: 'Times New Roman', serif; font-size: 12pt; line-height: 1.5; }
.co-why { border-left: 7px solid #000; background: #f7f7f7; }
```

## Mini assignments

::: assign Assignment 7.1: Style your card
Take your `card.html` from Chapter 6. Add a style sheet that gives the card a 1px border, 20px padding, 12px rounded corners, a maximum width of 400px, and centres it on the page (`margin: 40px auto`).
:::

::: assign Assignment 7.2: Two themes
Define `--bg`, `--text` and `--accent` variables. Add a button that toggles `data-theme` between `dark` and `light` (you will need one line of JavaScript, shown in Chapter 8). Check that every colour on the page comes from a variable.
:::

::: assign Assignment 7.3: A toolbar
Build a row of five buttons using flexbox. The first button sits at the far left, the last at the far right, and the middle three are centred together. Hint: `margin-left: auto` on the last item.
:::

## Big assignment: Make MiniIDE beautiful

::: big Style the MiniIDE from Chapter 6
**Goal.** Turn your plain `miniide.html` into something that looks like a real IDE, using only CSS.

**Requirements.**

1. Use CSS variables for **at least eight** colours. Provide a dark theme (default) and a light theme (`:root[data-theme="light"]`).
2. Make the whole page a **grid**: title bar (40px), middle area (1fr), bottom panel (200px), status bar (24px). The page must exactly fill the window with no page scrollbar.
3. The middle area is a second **grid** with columns: icon stripe (40px), sidebar (260px), editor (1fr).
4. The project tree shows indentation for nested folders and a hover highlight. The selected file has a blue background.
5. The editor tabs are a flex row. The active tab has a coloured bottom border and brighter text.
6. The code block has a monospace font, line height 1.5, and a subtle border.
7. Buttons have rounded corners, hover and pressed states, and a disabled look. The Run button is green, Stop is red.
8. The status bar has a small spinning circle (CSS animation) next to "Building…".
9. Add `[hidden] { display: none !important; }` and prove why you need it by temporarily removing it.
10. Everything must still work when the window is narrowed to 700px (no overlapping text).

**Stretch goals.** Add a draggable splitter by changing a CSS variable from 5 lines of JavaScript. Add a `@media print` rule that hides the toolbar and stripe. Recreate NOVA's rounded "chip" buttons exactly.

**How to check yourself.** Put your page next to the screenshot of NOVA in Chapter 1. The layout and proportions should look related.
:::

## Quiz

::: quiz
1. What are the three parts of a CSS declaration?
2. Which wins: `#sidebar` or `.tab`? Why?
3. Write the box model layers from inside to outside.
4. What does `box-sizing: border-box` change?
5. What does `1fr` mean in a grid?
6. How does NOVA switch between dark and light themes?
7. What does `[hidden] { display: none !important; }` fix?
8. Why do flex and grid children sometimes need `min-height: 0`?
9. What is a pseudo-element? Give one example.
10. Why are NOVA's transitions about 0.12 seconds long?
:::

::: answers
1. A property, a colon with a value, and a semicolon (e.g. `color: black;`).
2. `#sidebar`: an id is more specific than a class.
3. Content, padding, border, margin.
4. `width` and `height` then include padding and border, so boxes do not grow unexpectedly.
5. One fraction of the free space left after fixed sizes are removed.
6. JavaScript sets the `data-theme` attribute on the root element; a second rule block redefines the colour variables.
7. A CSS `display` rule overriding the browser's `hidden` behaviour: it forces hidden elements to stay hidden.
8. They refuse to shrink below their content size by default; `min-height: 0` lets them shrink so the content can scroll.
9. Extra generated content or styling attached to an element, such as `::before` (NOVA's spinner uses it).
10. Short motion feels smooth but never makes the user wait, which matters for tools used hundreds of times a day.
:::
