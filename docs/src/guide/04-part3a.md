@@part Part III — The Interface, Line by Line
# How a Windows Program Works

Before reading the interface code you need a mental model of what a Windows program *is*. This chapter gives it, in plain words, so that Chapters 10 to 13 read easily.

## A program that waits for events

A command-line program runs from the first line to the last and stops. A **window** program is different: it opens a window and then *waits*. Whenever something happens (you move the mouse, press a key, resize the window, ask it to close) Windows puts a **message** into the program's **message queue**. The program's job is a loop: take a message, deal with it, repeat, forever, until the window is closed.

::: def
**Message.** A small note from Windows to your program: "the mouse moved to (310, 225)", "the key W was pressed", "the window is now 1320 by 800".

**Window procedure (WndProc).** The function *you* write that receives every message for your window. It has a big <code>switch</code> that says what to do for each kind of message.

**Event loop.** The never-ending loop that fetches messages and passes them to the window procedure.
:::

{{fig:gl-msgloop|The heart of the program (the main loop at the bottom of live.cpp). Each pass handles pending messages, moves the physics forward, draws, and optionally captures a video frame.}}

## Two kinds of loops: waiting and polling

The classic loop uses <code>GetMessage</code>, which **sleeps** until a message arrives. That is perfect for a word processor that does nothing between keystrokes. Our program is more like a game: the physics and the picture must keep changing **even when you touch nothing**. So we use <code>PeekMessage</code>, which says "is there a message? If not, carry on" and returns at once. The loop therefore alternates *handle any messages* and *do one frame of work*.

::: whatif
**What if we used <code>GetMessage</code>?** The window would freeze whenever you stopped moving the mouse, because the loop would be asleep waiting for the next message. The simulation would only advance when you wiggled the mouse.
:::

## Drawing: GDI+, brushes and pens

Windows can draw in several ways. We use **GDI+**, a drawing library built into Windows. It gives us:

* a <code>Graphics</code> object: a canvas you draw on;
* <code>Pen</code>s for lines (a colour and a width) and <code>SolidBrush</code>es for filled shapes;
* <code>DrawLine</code>, <code>DrawLines</code>, <code>FillRectangle</code>, <code>DrawString</code>, and so on;
* **anti-aliasing**: the smooth, un-jagged edges you see on the springs and circles;
* **transformations**: <code>TranslateTransform</code> and <code>RotateTransform</code> shift and turn the whole canvas, which is how a spring is drawn at any angle (Chapter 11).

## Double buffering: never draw straight onto the screen

If you draw the background, then the springs, then the masses straight onto the visible window, the user sees each step happen: a flicker. The cure is **double buffering**: draw everything onto a hidden picture in memory, and only when it is complete, copy the whole thing to the window in one quick move (<code>BitBlt</code>, "bit-block transfer").

Our hidden picture is a **DIB section**: a bitmap whose pixels we can reach directly, as a plain block of memory. In <code>live.cpp</code> it is the struct <code>Canvas</code>.

::: why
**This one choice made recording almost free.** Because we always draw into a block of memory we own, the video recorder can take the *same pixels* and hand them to the encoder. If we had drawn straight to the window, we would need a slow and fragile screen-capture step. One drawing, two uses: show it on the screen, and write it to the file.
:::

## The coordinate systems in this program

Three different "worlds" meet in this program, and mixing them up is the commonest source of bugs.

{{fig:gl-view|The world is in metres with y pointing down; the screen is in pixels with y pointing down. Two numbers (the origin and the scale) convert between them.}}

| System | Unit | Origin | Used by |
|--------|------|--------|---------|
| **World** | metres | wherever the user's view puts (0, 0) | the physics, the saved file |
| **Screen / canvas** | pixels | top-left of the window | all drawing, the mouse |
| **Local (while drawing a link)** | pixels | at one end of the link, x axis along the link | the spring and damper drawing routines |

The conversion is a single line each way: <code>sx = ox + x * scale</code> and <code>x = (sx - ox) / scale</code>. The *view* is the three numbers <code>ox</code>, <code>oy</code>, <code>scale</code>. Panning changes <code>ox, oy</code>; zooming changes <code>scale</code>. The physics never sees them, so zooming never changes the simulation.

## How the files fit together

The three interface files divide the work like a small company:

| File | Role | Analogy |
|------|------|---------|
| <code>app.hpp</code> | Decides what *happens*: what is selected, what a click does, which mode it is in | the manager |
| <code>render.hpp</code> | Decides what it *looks like*: it only reads state, never changes it | the artist |
| <code>live.cpp</code> | Talks to Windows: window, messages, loop, files, recording | the receptionist |

::: why
**Why can <code>render.hpp</code> never change the state?** If drawing code could change state, then drawing twice (once for the screen, once for the video) could give different results, and a bug in a drawing routine could corrupt the simulation. Keeping drawing *read-only* makes the picture a pure function of the state.
:::

## Chapter checks

::: quiz
1. What is a message, and who sends it?
2. Why does the program use <code>PeekMessage</code> and not <code>GetMessage</code>?
3. What is double buffering and what problem does it cure?
4. In which coordinate system does the physics work?
5. Why can zooming not change the physics?
:::

::: answers
1. A note from Windows about an event (mouse, key, resize, close).
2. <code>PeekMessage</code> returns immediately so the simulation and drawing keep going when no messages arrive.
3. Drawing to a hidden bitmap and copying the finished picture to the window; it cures flicker (and lets the recorder reuse the picture).
4. In the world system: metres.
5. Zoom changes only the view (<code>scale</code>), which is used only for drawing and mouse conversion.
:::

# The Brain: app.hpp

<code>app.hpp</code> is the file that decides *what happens* when you do something. It holds all the program's memory (the **application state**) and a family of small functions, each triggered by a click or a key.

## The header and the vocabulary

@@src app.hpp | ^// app.hpp | ^#endif

* The comment states the file's job and, importantly, what it does **not** do: no drawing and no Windows plumbing.
* **<code>#pragma once</code>** and the standard includes you already know.
* **<code>#include "engine.hpp"</code> and <code>"model_io.hpp"</code>** bring in the physics and the file code. (A name in quotes means "look next to this file first"; angle brackets mean "look in the compiler's own folders".)
* **<code>#ifndef LAB_DIR / #define LAB_DIR "."</code>** gives <code>LAB_DIR</code> a fallback. The real value is supplied by CMake (<code>target_compile_definitions(... LAB_DIR="...")</code>) and is the folder containing the source, so the program finds <code>design.vlab</code> and writes <code>recordings/</code> in the same place no matter which folder the IDE starts it from.

@@src app.hpp | ^static const int    PANEL_W | ^enum Mode

* **<code>PANEL_W = 300</code>** The right-hand panel is 300 pixels wide. Almost every layout number in the program derives from this and the window size.
* **<code>SCENE_FRAC = 0.56</code>** The drawing area takes 56 % of the window height; the plots take the rest.
* **<code>PLOT_SECONDS = 10.0</code>** The time plot always shows the last 10 seconds.
* **<code>SIM_STEP = 0.001</code>** The physics step, 1 ms (Chapter 4).
* **<code>LOG_LIMIT = 36000</code>** At most 36 000 telemetry rows are kept: ten minutes at 60 per second. Old rows are dropped. Without a limit, a simulation left running overnight would eat all the memory.
* **The three enums** (<code>Tool</code>, <code>SelKind</code>, <code>Mode</code>) name the choices the program juggles: which tool is picked, whether a node or a link (or nothing) is selected, and whether we are designing, running or paused. Enums make code read like English (<code>if (A.mode == MODE_RUN)</code>) and let the compiler catch typos.

## The Frame: one row of telemetry

@@src app.hpp | ^// One row of telemetry | ^\};

A <code>Frame</code> is everything measured at one instant: time, the energies, and two lists. <code>nd</code> has **four numbers per node** (x, y, vx, vy) and <code>lk</code> has **three per link** (stretch or rate, force, length). They are <code>float</code>s, not <code>double</code>s, to halve the memory: 36 000 frames × (many numbers) adds up, and plots do not need 15 digits.

## The App struct: all the memory of the program

@@src app.hpp | ^struct App \{ | ^\} A;

All of the program's changing information lives in one struct, and exactly one instance of it, named <code>A</code>, exists. Read the fields in groups:

* **The model and the physics.** <code>model</code> (the design), <code>st</code> (the current state), <code>meter</code> (energy books), <code>mode</code>, <code>simAcc</code> (the accumulator from Chapter 4), <code>speed</code> (the speed knob: 0.125× to 4×).
* **The selection.** <code>tool</code>; <code>selKind</code> and <code>sel</code> (what is selected, and its index); <code>linkStart</code> (the first node clicked while drawing a link, or −1).
* **The view.** <code>ox, oy, scale</code>: the three numbers from Chapter 9. <code>mx, my</code> is the mouse position. <code>panning, panX, panY</code> track a right-button drag.
* **Dragging.** <code>dragNode</code> (the node under the mouse, or −1), <code>dragSlider</code> (the slider being dragged, or −1), <code>ext</code> (a force for every coordinate: the mouse spring writes into it).
* **Telemetry.** <code>log</code> (a <code>std::deque</code>, a list good at adding to one end and removing from the other), <code>plotMode</code>, <code>plotScale</code> (smoothed plot ranges), <code>domFreq</code>.
* **Housekeeping.** <code>fps</code>, <code>stepsLastFrame</code>, the <code>toast</code> message.
* **Recording.** <code>recording</code>, <code>recFrames</code>, <code>recPath</code>.

::: why
**Why one global struct instead of many separate variables?** Because every part of the interface needs to see the same state. A single struct named <code>A</code> makes it obvious where state lives (<code>A.mode</code>, <code>A.sel</code>), makes it easy to print or save the whole thing, and prevents two parts of the program from keeping their own, slightly different, copies. (In larger programs one would pass the struct around explicitly; for a single-window program a global is clear and short.)
:::

## Small helpers: time, clamping, toast

@@src app.hpp | ^inline double nowSeconds | ^inline void toast

* **<code>nowSeconds()</code>** A high-resolution clock. <code>QueryPerformanceFrequency</code> asks Windows "how many ticks per second?" once (the <code>static</code> variable keeps the answer); <code>QueryPerformanceCounter</code> asks "what tick is it now?" The division converts ticks to seconds. It is accurate to well under a microsecond, which matters for smooth animation.
* **<code>clampd(x, lo, hi)</code>** Forces a number into a range. Used everywhere to keep values sensible (slider fractions, zoom limits).
* **<code>toast(s)</code>** Stores a message and a time **3.5 seconds in the future** at which it should vanish. The renderer shows it while <code>nowSeconds() &lt; toastUntil</code>. Notice that <code>toast</code> never draws anything: it only changes state, and the artist does the rest.

## Screen layout

@@src app.hpp | ^inline Gdiplus::RectF sceneRect | ^inline void resetView

{{fig:gl-layout|The window is divided into the scene, two plots, and the panel. These four functions compute the rectangles.}}

* **<code>sceneRect()</code>** Left of the panel and above the plots: width <code>W - PANEL_W</code>, height <code>H * SCENE_FRAC</code>.
* **<code>timeRect()</code>** and **<code>phaseRect()</code>** split the lower-left area: the time plot takes 58 % of the width, the phase portrait 42 %. The <code>top</code> is 44 pixels below the scene so there is room for the plot tabs.
* **<code>inScene(x, y)</code>** Is a point inside the scene? Used to decide whether a click belongs to the drawing.
* **<code>resetView()</code>** Puts the world origin in the middle of the scene and sets <code>scale = 80</code> pixels per metre.

All layout is computed from <code>W</code> and <code>H</code> *every time*, never stored. That is why the window can be resized freely: the next frame simply recomputes every rectangle.

## Converting between world and screen

@@src app.hpp | ^// world \(metres\) | ^inline double snap

* The four <code>toScreen</code> / <code>toWorld</code> functions are the equations <code>sx = ox + x*scale</code> and <code>x = (sx - ox)/scale</code>, one per axis.
* **<code>snap(v)</code>** rounds to the nearest 0.25 m: <code>std::round(v / 0.25) * 0.25</code>. When you place or drag something, it lands on a **0.25 m grid**. That keeps designs tidy and makes it easy to build symmetric systems.

@@src app.hpp | ^inline void nodeWorld | ^\}$

* **Where a node is drawn.** While designing (<code>MODE_EDIT</code>) it is drawn at its **design** position. While a simulation exists (run or pause) it is drawn at its **live** position from the state. The guard <code>2*i+1 &lt; (int)A.st.q.size()</code> protects against asking for the live position of a node that was added after the simulation started.

@@src app.hpp | ^inline double nodeRadius | ^\}$

* The on-screen size of a node in pixels: <code>12</code> for an anchor, and <code>13 + 7 * cbrt(m)</code> for a mass. The **cube root** makes the block's *volume* (not its width) grow with mass: a 8 kg block is twice as wide as a 1 kg block, a natural look. The same number is the click target size.

## logFrame: recording the telemetry

@@src app.hpp | ^inline void logFrame | ^\}$

* Builds one <code>Frame</code> from the current state and meter.
* <code>f.nd.resize(4 * nodes)</code> and the loop fill x, y, vx, vy for every node (anchors included, so indices match node numbers).
* <code>f.lk.resize(3 * links)</code> and the loop call <code>linkInfo</code> for each link and store the stretch (for a spring) or the rate (for a damper), the force, and the length.
* <code>A.log.push_back(std::move(f));</code> Append it. <code>std::move</code> hands over the frame's internal arrays instead of copying them.
* <code>while (A.log.size() &gt; LOG_LIMIT) A.log.pop_front();</code> The cap from earlier.

## Controlling the simulation

@@src app.hpp | ^inline void startRun | ^\}$

* If there are no masses, there is nothing to simulate: show a toast and return.
* <code>A.model.refreshAutoRest();</code> Make sure automatic springs match the drawing.
* <code>A.st = lag::initialState(A.model);</code> Copy the design into a fresh state, with the starting velocities applied (and the rail rule).
* <code>A.meter.reset(...)</code> start the energy books. <code>A.ext.assign(size, 0.0)</code> zero the mouse forces. <code>A.log.clear()</code> forget the last run's telemetry.
* <code>A.mode = MODE_RUN; logFrame();</code> Enter run mode and log the very first row, so plots start at time zero.

@@src app.hpp | ^inline void stopRun | ^\}$
@@src app.hpp | ^inline void toggleRun | ^\}$

* <code>stopRun</code> returns to <code>MODE_EDIT</code>, cancels any drag, and zeroes the mouse force. The state itself is left alone; because the screen uses the design positions in edit mode, the masses jump back automatically.
* <code>toggleRun</code> is the Run button and the space bar: from edit mode it starts a run; from run it pauses; from pause it resumes.

{{fig:gl-modes|The three modes and what moves between them.}}

@@src app.hpp | ^inline void applyMouseForce | ^\}$

This is the "grab a mass with a rubber band" feature.

* <code>std::fill(A.ext.begin(), A.ext.end(), 0.0);</code> Clear last step's mouse force.
* The early return: do nothing unless we are running, a node is being dragged, the index is valid, and the node is a **mass** (you cannot drag a wall).
* <code>double tx = toWorldX(A.mx), ty = toWorldY(A.my);</code> The cursor in world coordinates: the spring's anchor point.
* <code>if (d &gt; 3.0) { dx *= 3.0/d; dy *= 3.0/d; }</code> Never pull from more than 3 m away. Without this limit, dragging the cursor far from the mass would produce an enormous force (the force is proportional to distance) and fling the system out of control.
* <code>A.ext[2*i] = m * (700.0 * dx - 45.0 * A.st.v[2*i]);</code> The force from Chapter 3 Step 6: a spring (700 per metre of stretch, per kg) and a damper (45 per m/s, per kg). Multiplying by <code>m</code> makes the *acceleration* independent of mass, so every block feels equally easy to move. The y component is the same.

{{fig:gl-mousespring|The mouse spring: a stiff, well-damped rubber band between the cursor and the mass.}}

::: whatif
**What if the gains were not damped (45 set to 0)?** The mass would oscillate around the cursor forever, like a bob on an elastic string, and never settle where you point. With the damping the mass moves to the cursor in about a tenth of a second and stays.
:::

@@src app.hpp | ^// Move the physics forward | ^\}$

The function that moves time forward, called once per frame.

* <code>if (A.mode != MODE_RUN) return;</code> Nothing moves in edit or pause.
* <code>A.simAcc += dt * A.speed;</code> Add the real elapsed time (scaled by the speed knob) to the accumulator.
* <code>while (A.simAcc &gt;= SIM_STEP &amp;&amp; steps &lt; 800)</code> Take 1 ms steps while there is enough time banked, but never more than 800 per frame (the spiral-of-death limit, Chapter 4).
* Inside: <code>applyMouseForce();</code> then <code>lag::advance(A.model, A.st, A.ext, SIM_STEP, A.meter);</code> then subtract one step from the accumulator. The mouse force is recomputed **every step**, because the mass moves between steps.
* <code>if (steps == 800) A.simAcc = 0;</code> Drop the backlog if we hit the limit.
* The <code>for (double v : A.st.q)</code> loop is a **safety net**: if any coordinate is not a finite number, or is more than 10 000 m from the origin, the simulation has blown up. We stop, and show a toast that suggests a remedy. A program that tells you what went wrong beats one that shows a blank screen.
* <code>logFrame();</code> Record one telemetry row for this frame.

## Hit testing: what did the user click?

{{fig:gl-hit|Two tests: a distance to a point for masses and anchors, and a distance to a segment for springs and dampers.}}

@@src app.hpp | ^inline int hitNode | ^\}$

* The loop runs from the **last** node to the first, so that if two nodes overlap, the one drawn on top (the later one) is chosen.
* <code>double dx = sx - toScreenX(x), dy = sy - toScreenY(y), r = nodeRadius(i) + 4;</code> The pixel distance from the click to the node's centre, and the click radius (the size of the node plus 4 pixels of forgiveness).
* <code>if (dx*dx + dy*dy &lt;= r*r) return i;</code> Compare the *squares*, which avoids a square root. Returns the node's index, or −1 if nothing was hit.

@@src app.hpp | ^inline double distToSegment | ^\}$

The distance from a point P to the line **segment** from A to B.

* <code>L2 = dx*dx + dy*dy</code> is the squared length of the segment.
* <code>t = ((px-ax)*dx + (py-ay)*dy) / L2</code> is the "how far along from A to B" of P's shadow on the line (a dot product), as a fraction.
* <code>clampd(t, 0, 1)</code> keeps the shadow *on the segment*, so clicking beyond an end measures to the end point.
* <code>std::hypot(px - (ax + t*dx), py - (ay + t*dy))</code> is the distance from P to that shadow point.
* <code>L2 &gt; 1e-9 ? ... : 0</code> avoids dividing by zero for a zero-length segment.

@@src app.hpp | ^inline int hitLink | ^\}$

* Loops over all links, measures the screen distance from the click to each link's segment, and remembers the nearest one **within 9 pixels** (<code>bd = 9.0</code> is both the starting "best distance" and the threshold).

## Editing

@@src app.hpp | ^inline void clearSelection | ^inline void setTool

* <code>clearSelection</code> sets the selection to "nothing".
* **<code>editingAllowed()</code>** is the gatekeeper for any change to the layout. If we are not in edit mode, it shows a toast ("Press Stop to edit the layout") and returns false. Every editing function begins with <code>if (!editingAllowed()) return;</code>.
* <code>deleteSelection</code> removes the selected node (and, through <code>removeNode</code>, its links) or the selected link, then clears the selection and cancels any half-drawn link.
* <code>setTool</code> changes the tool and cancels any half-drawn link.

::: why
**Why can't you edit the layout while it runs?** Imagine deleting a mass while the integrator is in the middle of a step: the arrays <code>q</code> and <code>v</code> would no longer match the model, and the next step would read the wrong numbers or run past the end of an array. Disallowing edits while running makes that whole class of bug impossible. Live edits that are *safe* (the sliders) are still allowed.
:::

@@src app.hpp | ^inline void loadPreset | ^// -.*the property sliders

* <code>loadPreset(n)</code> replaces the whole model with example *n* (stopping any run first), clears selection and telemetry, and shows a toast.
* <code>designPath()</code> is <code>LAB_DIR + "/design.vlab"</code>: the one design file.
* <code>saveDesign</code> and <code>loadDesign</code> call the engine-layer functions and report success or failure in a toast. <code>loadDesign</code> checks the file loaded into a *temporary* model first.
* <code>clearDesign</code> empties the canvas (edit mode only).

## The property sliders

@@src app.hpp | ^struct Prop \{ | ^static const char\* const RAIL_LABELS

<code>Prop</code> describes one slider:

* <code>name</code> is its label, <code>unit</code> its unit text, <code>lo</code> and <code>hi</code> its range.
* **<code>double* p;</code>** is a **pointer straight into the model**. This is the key idea of the slider system: moving the slider *writes the number directly into the physics*, so there is nothing to "apply" or "update". A slider for a spring's stiffness has <code>p = &amp;link.k</code>.
* <code>step</code>: if above zero, the value snaps to multiples of it. The rail slider uses <code>step = 1</code>, so it can only be 0, 1 or 2.
* <code>labels</code>: if not null, the value is shown as a word from this list (<code>"free (2-D)"</code>, <code>"X rail"</code>, <code>"Y rail"</code>).

@@src app.hpp | ^inline std::vector<Prop> buildProps | ^\}$

* This function builds, **every frame**, the list of sliders appropriate to the current selection. A selected mass gets seven sliders (mass, motion, start vx, start vy, push force, push angle, push frequency); a selected spring gets three (stiffness, rest length, cubic); a damper gets one (damping). Gravity is **always last**, whatever is selected.
* Because the list is rebuilt each frame from the model, it can never go out of date: if you delete the selected item, the next frame's list has no sliders for it.
* The checks like <code>A.sel &gt;= 0 &amp;&amp; A.sel &lt; (int)M.nodes.size()</code> guard against a stale selection index.

::: whatif
**What if <code>Prop</code> stored a <em>copy</em> of the value instead of a pointer?** Every slider would need a "write back" step, and every property would need its own special-case code. With a pointer, one drag handler serves every slider in the program.
:::

@@src app.hpp | ^static const int PROPS_Y | ^\}$

* <code>propTrack(i)</code> gives the rectangle of the *i*-th slider's track: its position depends only on the slider number and the window width, so the renderer and the mouse handler agree about where each slider is.
* <code>setPropFromX</code> is called while you drag. It converts the mouse's x position to a fraction <code>(x - tk.X) / tk.Width</code>, clamped to 0..1, then maps it to the slider's range: <code>val = lo + frac * (hi - lo)</code>. If there is a step, it snaps: <code>std::round(val / step) * step</code>. Then <code>*p.p = val;</code> writes it into the model.
* **<code>if (A.selKind == SEL_LINK &amp;&amp; p.p == &amp;A.model.links[A.sel].L0) ... autoRest = false;</code>** The pointer trick pays off again: we can *ask whether the slider being moved is a spring's rest length* by comparing pointers. If so, the spring stops following the drawing (Chapter 5).
* **<code>if (A.mode != MODE_EDIT) A.meter.rebase(...)</code>** A live edit: restart the energy books (Chapter 7).

## Buttons

@@src app.hpp | ^enum Action | ^struct Btn

* <code>Action</code> is a numbered list of everything a button can do: the five tools, delete, run, stop, record, export, save, load, clear, four example buttons and four plot tabs.
* <code>Btn</code> is a button: its rectangle, label, action, and whether it is "active" (drawn highlighted).

@@src app.hpp | ^inline std::vector<Btn> layoutButtons | ^\}$

* Like <code>buildProps</code>, this builds the **whole list of buttons every frame**. The positions are fixed numbers relative to the panel's left edge <code>px = W - PANEL_W</code>.
* The small lambda <code>add(x, y, w, h, label, id, active)</code> is a shortcut for pushing one button.
* Note <code>A.mode == MODE_RUN ? "Pause" : (A.mode == MODE_PAUSE ? "Resume" : "Run")</code>: the label of the Run button depends on the mode. Nested conditional operators read like "if running say Pause, else if paused say Resume, else say Run".
* <code>A.recording ? "Stop REC" : "Record"</code>: the same idea for Record.
* The four tab buttons are positioned relative to <code>timeRect()</code>, so they sit above the time plot.

**One list, two users.** The renderer loops over this list to *draw* the buttons; the mouse handler loops over the *same list* to find which was clicked. Because both use the same function, the button you see is always exactly the button you click: they cannot drift apart.

@@src app.hpp | ^inline void doAction | ^\}$

* A **switch** that turns a button's <code>Action</code> into a function call. Note that <code>ACT_RECORD</code> and <code>ACT_CSV</code> call <code>toggleRecording()</code> and <code>exportCsv()</code>, which are only *declared* just above (<code>void toggleRecording();</code>) and *defined* later in <code>live.cpp</code>. This is called a **forward declaration**: it promises the compiler the functions exist so <code>app.hpp</code> can call them without including the file that has the video code.

## The mouse

@@src app.hpp | ^inline void onMouseDown | ^\}$

The click handler tries three targets in order, and stops at the first that accepts the click.

1. **Buttons.** Loop through <code>layoutButtons()</code>; if the click is inside one, run its action and <code>return</code>.
2. **Sliders.** Loop through the sliders; if the click is near a track (with 10 pixels of horizontal and −16/+22 pixels of vertical forgiveness, so the thin track is easy to hit), start dragging that slider and set its value.
3. **The drawing area.** First <code>if (!inScene(x, y)) return;</code>. Then <code>hitNode(x, y)</code> and a <code>switch</code> on the tool:
   * **Select:** a node hit selects it and starts dragging it (<code>dragNode</code>). Otherwise try a link. Otherwise clear the selection.
   * **Mass / Ground:** if editing is allowed and the click is on **empty** space (<code>hn &lt; 0</code>), add the node at the snapped position and select it.
   * **Spring / Damper:** clicking empty space cancels; the first click on a node stores it in <code>linkStart</code>; the second click calls <code>addLink</code>. If <code>addLink</code> refuses (returns −1) a toast explains. Either way <code>linkStart</code> is reset.

@@src app.hpp | ^inline void onMouseMove | ^\}$

* If a right-button pan is in progress, move the origin by the cursor's movement, and return.
* If a slider is being dragged, update it.
* If a node is being dragged **in edit mode with the select tool**, move it to the snapped mouse position, clamp it to ±40 m (so it cannot be lost off-screen), and call <code>refreshAutoRest()</code> so automatic springs stay relaxed.
* In *run* mode dragging does not move the node here at all: <code>applyMouseForce</code> reads the mouse position every physics step and the physics moves the node. The same mouse gesture has two meanings depending on the mode.

@@src app.hpp | ^inline void onMouseUp | +1
@@src app.hpp | ^inline void onWheel | ^\}$

* <code>onMouseUp</code> ends any drag (a one-line function).
* <code>onWheel</code> zooms by a factor of 1.12 per wheel notch, limited to 25 .. 260 pixels per metre. The two lines <code>A.ox = x - before * A.scale;</code> keep the world point under the cursor **fixed** while zooming. Without them the picture would zoom toward the world origin and the thing you were looking at would slide away.

{{fig:gl-view|Zooming about the cursor: the world point under the mouse stays where it is.}}

## Chapter checks

::: quiz
1. Why is there exactly one <code>App</code> object?
2. Why does <code>buildProps</code> run every frame instead of once?
3. What is the advantage of <code>Prop::p</code> being a pointer?
4. Why does <code>applyMouseForce</code> limit the pull to 3 m?
5. In <code>hitNode</code>, why compare squared distances?
6. Why is <code>editingAllowed()</code> called before every layout change?
7. How does <code>onWheel</code> keep the point under the cursor fixed?
:::

::: answers
1. All parts of the interface must share the same state; one struct makes that obvious and prevents duplicate, drifting copies.
2. The correct sliders depend on the current selection and on the model, which can change at any moment; rebuilding makes it impossible to show stale sliders.
3. One drag handler can edit any property in the program, and the change goes straight into the physics with nothing to copy back.
4. The force is proportional to distance, so a far cursor would make a huge force and throw the system out of control.
5. It gives the same answer as comparing distances but avoids a slow square root.
6. Changing the layout while the integrator runs would make the model and state arrays disagree, which is a crash or garbage.
7. After changing <code>scale</code> it recomputes <code>ox, oy</code> so that the world point that was under the cursor maps to the same pixel as before.
:::
