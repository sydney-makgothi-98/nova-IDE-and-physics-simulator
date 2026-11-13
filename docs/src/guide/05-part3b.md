# The Artist: render.hpp

<code>render.hpp</code> turns the state into a picture. It is the largest file, but it is simple in spirit: every function takes the <code>Graphics</code> canvas, looks at the state in <code>A</code>, and draws. It **never changes the state**.

{{fig:gl-layers|The order of drawing. Each layer is painted over the one before it (the painter's algorithm).}}

## The header

@@src render.hpp | ^// render.hpp | ^using namespace Gdiplus;

* <code>#include &lt;cstdarg&gt;</code> brings in <code>va_list</code>, needed by <code>textf</code> below.
* <code>using namespace Gdiplus;</code> lets us write <code>Pen</code> and <code>Graphics</code> instead of <code>Gdiplus::Pen</code> and <code>Gdiplus::Graphics</code>.

## Fonts, colours and the first helpers

@@src render.hpp | ^static FontFamily\* gMono | +6

* **<code>gMono, fSmall, fNorm, fBig</code>** are the program's fonts, created once in <code>main()</code> (Consolas, a clean monospaced font, in three sizes). They are pointers because they must be built *after* GDI+ has started.
* **<code>C(r, g, b, a)</code>** is a shorthand for building a colour: <code>C(53, 116, 240)</code> is a blue. The alpha <code>a</code> (opacity) defaults to 255, fully solid.
* **<code>MASS_COL[8]</code>** is the palette used for masses: blue, red, green, amber, violet, cyan, pink, grey. The *k*-th mass always gets the same colour in the block, the plot lines and the phase portrait.

@@src render.hpp | ^static Color massColor | ^\}$
@@src render.hpp | ^static int massOrdinal | ^\}$

* **A subtle point:** a node's *index* counts anchors too. If the nodes are [anchor, mass, mass], the masses have indices 1 and 2 but are called **m0** and **m1**, and take colours 0 and 1. Both functions count how many **masses** come before the node. <code>massOrdinal</code> returns that count (the m-number); <code>massColor</code> uses it to pick a palette entry, wrapping around after eight (<code>k % 8</code>).

@@src render.hpp | ^static void text\(Graphics | ^\}$

* **<code>MultiByteToWideChar</code>** converts our UTF-8 text to the wide characters (UTF-16) that Windows drawing wants. <code>CP_UTF8</code> names the encoding; the buffer is 256 wide characters.
* **<code>SolidBrush b(c);</code>** A brush in the requested colour.
* **<code>StringFormat sf; sf.SetAlignment(al);</code>** Controls whether the point we give is the left end of the text (<code>StringAlignmentNear</code>), its middle (<code>Center</code>) or its right end (<code>Far</code>). We use this to centre labels on masses and right-align slider values.
* **<code>StringFormatFlagsNoWrap</code>** keeps each string on one line.
* <code>g.DrawString(w, -1, f, PointF(x, y), &amp;sf, &amp;b);</code> Draws it. The <code>-1</code> means "the string is null-terminated, work out its length".

@@src render.hpp | ^static void textf | ^\}$

* A <code>printf</code>-style text function: <code>textf(g, x, y, font, colour, "t = %8.3f s", value)</code>. The <code>...</code> in the parameter list accepts any number of extra arguments; <code>va_start</code>, <code>vsnprintf</code> and <code>va_end</code> are the standard C way to forward them to a formatting function. <code>vsnprintf</code> writes at most 200 characters, so a long message can never overflow the buffer.

## Springs and dashpots, drawn flat

@@src render.hpp | ^// A zig-zag spring | ^\}$

* The spring is a **polyline**: a list of points joined by straight segments. It starts at <code>x1</code>, runs a short straight **lead** (at most 10 pixels, at most 15 % of the length) and then zig-zags up and down by <code>amp</code> pixels <code>coils * 2</code> times, and ends with another straight lead to <code>x2</code>.
* <code>span = len - 2*lead</code> is the length available for zig-zags. Each zig-zag point is placed at <code>x1 + lead + span * (i + 0.5) / (coils*2)</code>: evenly spaced, with half a gap at each end. The <code>(i % 2 ? amp : -amp)</code> alternates the height: up, down, up, down.
* The spring is drawn **horizontal**, along the x axis at height <code>y</code>. Turning it to any angle is the next function's trick, shown below.

@@src render.hpp | ^// A dashpot | ^\}$

* A dashpot (shock-absorber symbol) has three parts: a **cylinder** drawn as an open box (<code>cyl[4]</code>: four points forming a "[" shape), a **rod** from the far end into the cylinder, and a **piston** (a short vertical bar on the rod).
* <code>cl = fmax(14, len*0.5)</code> is the cylinder length: half the link, at least 14 pixels. <code>c0, c1</code> are its two ends.
* <code>p = clampd(x2 - 14, c0 + 6, c1 - 2)</code> is the piston's position, **clamped** to stay inside the cylinder. As the link compresses or stretches, the piston slides in and out but cannot leave the cylinder. A visual cue that the damper is working.

@@src render.hpp | ^static void panelFrame | ^\}$

* Draws a dark rectangle with a thin border, and (if a title is given) the title just above it. The time plot passes an empty title because its tabs already say what it shows.

## drawNodeBody: masses and anchors

@@src render.hpp | ^static void drawNodeBody | ^\}$

* **Setup.** <code>wx, wy</code> are the node's world position (design or live, via <code>nodeWorld</code>); <code>x, y</code> are its pixel position. <code>selected</code> is true if this node is the current selection.
* **A ground anchor** is drawn as a horizontal bar with hatch marks beneath (the standard engineering symbol for a fixed support) and a round pin on top. It turns **cyan** when selected. Then <code>return</code>: anchors have no more to draw.
* **A mass.** <code>R = nodeRadius(i)</code> is half its width. <code>rail = (int)std::lround(n.rail);</code> reads the rail setting.
* **The rail guide.** In edit mode, a mass on a rail gets a faint dashed line through it (horizontal for an X rail, vertical for a Y rail), 90 pixels each way, to show where it can go.
* **The ghost.** While a simulation exists, a faint dotted square marks where the mass **started**. It makes it easy to see how far the mass has moved, and is how the sag in example 4 is visible.
* <code>LinearGradientBrush gb(r, lighter, base, LinearGradientModeVertical);</code> Fills the block with a gradient from a lighter shade at the top to the base colour at the bottom, which gives a gentle 3-D look. The lighter shade is the base colour with 55 added to each channel, limited to 255 (<code>std::min(255, ...)</code>).
* The outline is **white and thick** when selected, dark and thin otherwise.
* The two labels: <code>"m%d"</code> (the mass number) above the middle and <code>"%.3gkg"</code> (the mass in kg, with at most 3 significant figures) below it.

## drawLink: a spring or damper at any angle

{{fig:gl-linkdraw|The trick for drawing a spring at any angle: draw it flat, then move and turn the whole canvas.}}

@@src render.hpp | ^static void drawLink | ^\}$

* **Geometry.** <code>pax, pay, pbx, pby</code> are the pixel positions of the two ends. <code>dx, dy, len</code> give the on-screen vector and its length; if the link is shorter than 4 pixels we skip it (<code>return</code>), because nothing sensible can be drawn. <code>ux, uy</code> is the unit vector along it.
* **Fanning out parallel links.** The loop counts how many links join the same two nodes (<code>same</code>) and how many of those come before this one (<code>before</code>). The sideways offset is <code>off = (before - (same - 1)/2.0) * 16.0</code> pixels: for two links it is −8 and +8, so they appear as two parallel lines; for a single link it is 0. That is how a spring and damper between the same two nodes are drawn side by side instead of on top of each other.
* **Colour.** The default is near-white for springs and grey for dampers. **While running**, a spring is colored by how hard it pulls: its force <code>in.force</code> is divided by a reference force (<code>forceRef</code>) and limited to −1..+1. Positive (stretched, pulling) turns it **orange**; negative (compressed, pushing) turns it **blue**. You can *see* the forces. A selected link is cyan.
* **The rotation trick.** <code>GraphicsState state = g.Save();</code> remembers the canvas's current transform. <code>g.TranslateTransform(pax - uy*off, pay + ux*off);</code> moves the canvas's origin to the start of the link, shifted sideways by the offset (the perpendicular to **u** is (−u<sub>y</sub>, u<sub>x</sub>)). <code>g.RotateTransform(atan2(dy, dx) * 180/π);</code> turns the canvas so its x axis points along the link. Now we can draw the spring **flat**, along x, using the simple <code>coil</code> function, and it appears at the right angle on screen. <code>g.Restore(state);</code> puts the canvas back so the next drawing is unaffected.
* **Shortening.** <code>s0 = fmin(ra, len*0.4)</code> and <code>s1 = len - fmin(rb, len*0.4)</code> shrink the drawn part so the spring starts at the edge of the mass blocks, not their centres. The <code>0.4 * len</code> limit prevents the ends crossing if two nodes are very close.
* **What to draw.** If the visible part is under 12 pixels, a plain line; for a spring, <code>coil</code> with a coil count of <code>round((s1 - s0)/16)</code>, limited to 3..22 (longer springs get more coils, like a real spring); for a damper, <code>dashpot</code>.

::: why
**Why rotate the canvas instead of computing every zig-zag point at an angle?** The rotation takes two lines and works for every shape, now and in the future. Computing the rotated points by hand for the spring and again for the dashpot would be more code, more arithmetic, and more chances for a sign error.
:::

## drawScene: assembling the canvas

@@src render.hpp | ^static void drawScene | ^\}$

* <code>g.SetClip(sc);</code> Restrict all drawing to the scene rectangle; <code>g.ResetClip();</code> at the end lifts the restriction. A **clip region** stops a stretched spring or a zoomed-in view from painting over the plots below.
* **The grid.** One vertical and one horizontal line per metre, from <code>floor(x0)</code> to <code>ceil(x1)</code> (the world limits of the visible area). The line through zero is a slightly brighter pen (<code>axis</code>). The grid scrolls and scales with the view, so you always have a ruler.
* **The empty-canvas hint.** If there are no nodes in edit mode, two lines of instructions are drawn in the middle.
* **Spring colour reference.** <code>forceRef</code> starts at 5 N and grows to 1.5 times the biggest spring force in the latest telemetry row. This is how the colour scale adapts: a gentle system and a violent one both show the full range of colours.
* **Drawing order.** First every link (<code>drawLink</code>), then every node (<code>drawNodeBody</code>), so masses cover the ends of springs.
* **Tool previews.** In edit mode with the Mass or Ground tool, over empty space, a dashed cyan outline shows where the new node will land, at the **snapped** position. With a half-drawn link (<code>linkStart &gt;= 0</code>) a dotted cyan rubber band is drawn from the first node to the cursor, with a prompt.
* **The mouse spring.** In run mode while dragging a mass, a dotted red line and a red dot show the rubber band.

## drawHud: the telemetry box

@@src render.hpp | ^static void drawHud | ^\}$

* The box is a semi-transparent black rectangle (<code>C(8, 10, 14, 215)</code>: alpha 215 of 255) with a white border, in the top-left of the scene.
* The **title line** and the **mode word** (DESIGN in amber, RUNNING in green, PAUSED in red).
* **Global numbers.** Time, speed and FPS. While a simulation exists: T, V, E = T + V and the dissipated energy, and the **energy-law error** <code>std::fabs(A.meter.residual())</code> with the count of masses and links. In design mode: a count of masses and springs, and the gravity.
* **The selection section.** What it shows depends on what is selected:
  * **A mass:** its name and colour, then (if a simulation exists) its displacement from the start (<code>q - design position</code>), its speed |**v**|, its acceleration |**a**| (computed by calling the physics function <code>lag::accel</code> once more, just for the readout) and the dominant frequency.
  * **A ground anchor:** its position and the note that it never moves.
  * **A spring or damper:** length and stretch (spring) or rate (damper), force, and for a damper the **power lost** <code>c ṡ²</code> in watts, which is how much heat it is making at this instant.
  * **Nothing selected:** a hint.
* **The REC badge.** While recording: a red dot and the elapsed time <code>mm:ss</code>, computed from the frame count at 60 frames per second.
* **The toast** appears centred near the bottom of the scene until its expiry time.

::: check
Run example 4, select the damper, and read "power lost". It is zero when the damper is not moving (rate = 0) and greatest when the suspension is moving fastest: a damper makes heat only when it moves.
:::

## Plots

@@src render.hpp | ^struct Series | +1

* A <code>Series</code> is one curve: a name for the legend, a colour, and its list of values. All curves of a plot share one list of times.

@@src render.hpp | ^// Draw several curves against time | ^\}$

The generic time plot. It draws any set of curves, so the Position, Velocity, Energy and Link-force tabs all use it.

* **Finding the range.** It scans every value for the smallest and largest. If <code>symmetric</code> is true (position, velocity, force) the range is made symmetric about zero, so zero sits in the middle and positive and negative look equal. Energy is not symmetric. A tiny range is widened so a flat line does not crash the scaling (<code>hi - lo &lt; 1e-6</code>).
* **Padding.** The range is widened 10 % so curves do not touch the frame.
* **Smoothing.** <code>sm</code> remembers the previous range for each tab (<code>A.plotScale[modeIndex]</code>) and moves 20 % of the way to the new one each frame (<code>sm += (target - sm) * 0.2</code>). Without this, the whole plot would jump whenever a new peak appeared. With it the axis glides.
* **The grid and labels.** Five horizontal lines with numeric labels formatted <code>%.3g</code> (three significant figures); a zero line if zero is in range.
* **The curves.** For each series, every sample becomes a point: x from its time (<code>(times[i] - t0) / PLOT_SECONDS * r.Width</code>, where <code>t0 = t1 - 10</code>, so the plot is a 10-second sliding window) and y from its value scaled to the rectangle. <code>g.DrawLines</code> joins them.
* **The legend.** A small coloured square and the series name for each curve, moving right by an amount based on the name length.

@@src render.hpp | ^static void drawPlots | ^\}$

* **Collecting the frames.** It walks the telemetry log **backwards** from the newest row while the rows are within the last 10 seconds, then reverses the list (<code>std::reverse</code>) so it is oldest first. It builds the list of times.
* **Tabs 0 and 1 (Position and Velocity).** For each mass it makes up to **two** series, x and y. A component is *skipped* if the mass's rail forbids it (an X-rail mass has no y curve), so a rail mass shows a single clean curve. The y curve is drawn in a darker shade of the same colour (<code>c * 0.6</code>). Position is plotted **relative to the design position** so every curve starts near zero.
* **Tab 2 (Energy).** Four series: kinetic (red), potential (blue), total E = T + V (white), and the running total of dissipated energy (amber). You can watch energy trade between T and V and drain into heat.
* **Tab 3 (Link force).** If a spring or damper is selected: its force (cyan) and its stretch or rate (amber). If not, an instruction is shown on the empty plot.
* **The phase portrait.** It picks the selected mass, or the first mass if none is selected, and plots **velocity against position** for the last 700 rows. For a mass on a Y rail it uses y; otherwise x. The oldest points are drawn **fainter** (alpha grows from 30 to 255 with age), leaving a fading trail, and the newest point gets a white dot.

::: def
**Phase portrait.** A plot of velocity (vertical) against position (horizontal). A frictionless oscillator traces a closed loop (an ellipse); a damped one spirals inward toward the middle; a driven one settles onto a loop. Shape tells you the *kind* of motion at a glance.
:::

## The right-hand panel

@@src render.hpp | ^static void drawPanel | ^\}$

* **The background and headings.** A dark strip the full height of the window, a divider line, and the small section titles TOOLS, SIMULATION, DESIGN FILE and EXAMPLES at the same fixed positions as the buttons.
* **Drawing the buttons.** One loop over <code>layoutButtons()</code>. For each: is the mouse over it (<code>hover</code>)? Is the Record button active (<code>rec</code>)? The fill colour is red for an active Record, blue for an active button, lighter grey for a hover, plain grey otherwise. Then the rectangle, border and centred label.
* **The properties section.** A title built from the selection ("mass m0", "spring", "damper", "ground anchor", or "nothing selected (world settings)"), then one slider per entry of <code>buildProps()</code>: the name on the left, the value on the right (a word if the slider has labels, else <code>"%.3g unit"</code>), a grey **track**, a blue **fill** up to the value, and a round white **knob**.
* **The footer note.** "sliders edit the physics LIVE" in green while a simulation exists; in design mode a reminder that starting velocity and push apply at Run.

## render: putting the layers together

@@src render.hpp | ^static void render | ^\}$

* <code>W = w; H = h;</code> Keeps the global size in step with the canvas.
* <code>SetSmoothingMode(SmoothingModeAntiAlias)</code> turns on smooth edges; <code>SetTextRenderingHint(TextRenderingHintAntiAlias)</code> does the same for text.
* Fill the background, then <code>drawScene</code>, <code>drawPlots</code>, <code>drawHud</code>, <code>drawPanel</code> in that order (the painter's algorithm of Figure 11.1).

This eight-line function is *the entire picture*. Screen and video both come from calling it.

## Chapter checks

::: quiz
1. What does <code>g.Save()</code> / <code>g.Restore()</code> let <code>drawLink</code> do?
2. How are a spring and a damper between the same two nodes kept from overlapping?
3. Why does the plot's vertical range move 20 % at a time?
4. Why is a Y-rail mass shown with only one curve in the Position plot?
5. How does the program colour a spring by its force?
6. What would go wrong if <code>drawScene</code> did not set a clip region?
:::

::: answers
1. Rotate and shift the canvas to draw flat shapes at an angle, then put the canvas back exactly as it was.
2. Links with the same endpoints are offset sideways by (before − (same−1)/2) × 16 pixels.
3. Smoothing: without it the axis would jump every time a new peak appears; with it the axis glides.
4. The rail forbids x motion, so only the y curve is meaningful (the x series is skipped).
5. The spring's force divided by a reference force, limited to −1..+1: positive blends toward orange, negative toward blue.
6. A long spring or a zoomed-in view could paint over the plots and panel.
:::

# The Receptionist: live.cpp

<code>live.cpp</code> is the only file the compiler builds directly. It connects everything to Windows. This chapter covers every part except recording (Chapter 13) and the self-test (Chapter 14).

## The header and includes

@@src live.cpp | ^// live.cpp | ^static HWND gWnd

* The big comment is the **map of the program**: one line per file, plus the command-line modes. The first thing a new reader sees explains the whole layout.
* <code>namespace Gdiplus { using std::min; using std::max; }</code> is a workaround. The GDI+ headers use the names <code>min</code> and <code>max</code> without saying where they come from; this line tells the compiler "inside <code>Gdiplus</code>, <code>min</code> means <code>std::min</code>". It must come **before** <code>#include &lt;gdiplus.h&gt;</code>.
* The **order of the includes matters**: <code>&lt;windows.h&gt;</code> first, then <code>&lt;gdiplus.h&gt;</code>, then our headers, because our headers use Windows types such as <code>LARGE_INTEGER</code> and GDI+ types such as <code>RectF</code>.
* <code>static HWND gWnd = nullptr;</code> The handle of our window. <code>HWND</code> is "handle to a window", a number Windows uses to identify it. <code>nullptr</code> means "no window yet". The headless modes never create a window, and <code>present()</code> checks for that.

## The Canvas: our hidden picture

@@src live.cpp | ^struct Canvas \{ | ^\} canvas;

* A **struct with its own methods**. It holds a device context <code>dc</code>, a bitmap <code>bmp</code>, the previous bitmap <code>old</code>, a pointer <code>bits</code> to the **pixel memory**, and the size.
* **<code>make(nw, nh)</code>**: first <code>destroy()</code> any old one. Then fill a <code>BITMAPINFO</code> describing a 32-bit-per-pixel image: <code>biWidth = nw</code> and **<code>biHeight = -nh</code>**. The *negative* height is a Windows convention meaning "rows run **top to bottom**", which is what we want and what the recorder expects.
* <code>GetDC(nullptr)</code> gets the screen's device context, used only as a model for colour format. <code>CreateCompatibleDC</code> makes a memory device context that can hold a picture. <code>CreateDIBSection(...)</code> makes the actual bitmap and returns a pointer to its pixels in <code>bits</code>. <code>SelectObject(dc, bmp)</code> puts the bitmap into the device context so drawing goes into it. <code>ReleaseDC</code> gives the screen DC back.
* **<code>destroy()</code>** undoes it in the reverse order: select the old bitmap back, delete the new one, delete the DC. Windows resources must be released by hand; forgetting leaks memory every time the window is resized.

@@src live.cpp | ^static void drawFrameToCanvas | ^\}$
@@src live.cpp | ^static void present | ^\}$

* <code>drawFrameToCanvas</code> makes sure the canvas has the right size (a resize makes a new one), creates a <code>Graphics</code> object that draws **into the canvas's device context**, and calls <code>render</code>. This is the moment a picture exists in memory.
* <code>present</code> returns immediately if there is no window or the window is tiny. Otherwise it draws the frame, then <code>BitBlt</code>s the finished canvas to the window in one go: <code>SRCCOPY</code> means "copy the pixels exactly". Double buffering in six lines.

## Files and folders

@@src live.cpp | ^static std::string stamp | ^\}$
@@src live.cpp | ^static std::string recordingsDir | ^\}$

* <code>stamp()</code> returns a timestamp such as <code>20261112_221530</code> (year, month, day, hour, minute, second), made with <code>strftime</code>. File names built from it sort in time order and never collide.
* <code>recordingsDir()</code> builds <code>LAB_DIR/recordings</code> and calls <code>CreateDirectoryA</code>, which creates the folder or quietly does nothing if it already exists. Videos and CSV files all go there.

## Exporting the telemetry to CSV

@@src live.cpp | ^void exportCsv | ^\}$

**CSV** ("comma-separated values") is a plain text table that Excel, Python, MATLAB and others can all open.

* If the log is empty, tell the user and stop.
* Build a path with <code>stamp()</code> and open an <code>std::ofstream</code>; if that fails, say so.
* **The header row.** <code>t,T,V,E,dissipated,work_in</code> then, for every **mass**, four columns: <code>m0_x,m0_y,m0_vx,m0_vy</code>, and for every link three columns named after its kind and index: <code>spring3_stretch, spring3_force, spring3_length</code> (or <code>damper2_rate, ...</code>).
* <code>f.precision(7);</code> Seven significant digits.
* **One row per telemetry frame**, in the same column order: the times and energies, then the four numbers of each mass, then the three of each link. The guards <code>4*i+3 &lt; fr.nd.size()</code> protect against a frame that was recorded with fewer nodes.
* Finally a toast with the file name.

::: try
Run an example for ten seconds, press **Export CSV**, open the file from the <code>recordings</code> folder in a spreadsheet, and make a chart of <code>m0_x</code> against <code>t</code>. It should look like the plot on screen.
:::

## Reading the dominant frequency

@@src live.cpp | ^static void updateDominantFrequency | ^\}$

* It works only in run mode, with a **mass** selected, and once at least 64 rows exist (about one second): otherwise <code>domFreq = 0</code> and the display shows "--".
* <code>comp</code> chooses which coordinate to analyse: y for a Y-rail mass, x otherwise.
* It copies the **last 360 rows** (about six seconds) of that coordinate into a vector.
* <code>span = last time - first time</code> and the sample rate is <code>(n - 1) / span</code>: the number of intervals divided by the time they took. We *measure* the sample rate instead of assuming 60, because the frames are not perfectly evenly spaced.
* It calls <code>lag::dominantFrequency</code> (Chapter 7) only if the span is over half a second.

The main loop calls this about every 0.3 seconds. The calculation is not free (a Fourier test of several hundred samples at over a thousand frequencies), so it is *rationed*.

## The keyboard

@@src live.cpp | ^static void onKey | ^\}$

* <code>GetKeyState(VK_CONTROL) &amp; 0x8000</code> asks whether the Ctrl key is held down. The <code>0x8000</code> picks the "is down" bit.
* **Ctrl+S** saves the design, **Ctrl+O** loads it.
* A <code>switch</code> maps each key to a function you have already met: <code>VK_SPACE</code> toggles run; <code>VK_ESCAPE</code> cancels a half-drawn link, or else stops the run, or else returns to the Select tool (three different meanings, chosen by what is going on); letters S, M, G, K, D pick the tools; Delete or Backspace deletes; F9 toggles recording; Tab cycles the plot tabs; Home resets the view; comma and period halve and double the speed (clamped to 0.125..4); the digits 1 to 4 (and the numeric keypad) load the examples.
* <code>VK_...</code> are *virtual-key codes*: names Windows gives to keys. Letters and digits use their ASCII codes ('S', '1').

## The window procedure

@@src live.cpp | ^static LRESULT CALLBACK WndProc | ^\}$

The function Windows calls for every message. Read the cases in turn:

* **<code>WM_SIZE</code>**: the window changed size. <code>LOWORD(lp)</code> and <code>HIWORD(lp)</code> are the new width and height, packed into one number; we store them in <code>W</code> and <code>H</code>. Next frame, every rectangle is recomputed from them.
* **<code>WM_GETMINMAXINFO</code>**: Windows asks "what is the smallest size you allow?" We say 1100 × 700, so the panel and plots never get squashed beyond use.
* **<code>WM_ERASEBKGND</code>**: returns 1 ("I did it") without erasing. If Windows cleared the window to white before every repaint, it would flash. We paint every pixel ourselves.
* **<code>WM_PAINT</code>**: Windows says the window needs repainting. <code>BeginPaint/EndPaint</code> tell it we noticed; then <code>present()</code> draws.
* **<code>WM_LBUTTONDOWN / MOUSEMOVE / LBUTTONUP</code>**: the left button. <code>SetCapture(h)</code> on press makes Windows keep sending mouse messages to our window *even if the pointer leaves it*, so a drag continues past the edge; <code>ReleaseCapture()</code> on release lets go. The position is packed in <code>lp</code>, extracted with <code>LOWORD</code>/<code>HIWORD</code> and cast to <code>(short)</code> so that positions left of or above the window come out **negative** instead of huge positive numbers. The handlers are the ones from <code>app.hpp</code>.
* **<code>WM_RBUTTONDOWN / MBUTTONDOWN</code>** start a **pan**: set <code>A.panning</code> and remember the start. The matching UP messages end it.
* **<code>WM_MOUSEWHEEL</code>**: the wheel position arrives in **screen** coordinates, so <code>ScreenToClient</code> converts it to window coordinates. <code>GET_WHEEL_DELTA_WPARAM(wp) / 120</code> is the number of notches (one notch = 120 units).
* **<code>WM_KEYDOWN</code>**: a key was pressed; hand it to <code>onKey</code>.
* **<code>WM_DESTROY</code>**: the window is closing. We **stop any recording** (so the .mp4 is finished properly), **save the design** to <code>design.vlab</code> (so it is waiting for you next time), and <code>PostQuitMessage(0)</code> which puts <code>WM_QUIT</code> into the queue and ends the main loop.
* **<code>DefWindowProc</code>** handles every message we did not mention, with Windows' default behaviour (moving the window, minimising, and so on). Forgetting it makes a window that cannot be dragged.

## Reading the command line

@@src live.cpp | ^struct CliOptions | +1
@@src live.cpp | ^static CliOptions parseOptions | ^\}$
@@src live.cpp | ^static void applyOptions | ^\}$
@@src live.cpp | ^static void stepOffscreen | ^\}$

* These functions support the headless modes (<code>--shot</code> and <code>--demo</code>) used for testing and for taking this guide's screenshots.
* <code>parseOptions</code> scans the arguments in pairs: <code>--preset 3</code>, <code>--run 4</code> (seconds), <code>--select node:1</code> or <code>link:2</code>, <code>--tab 2</code>, <code>--empty 1</code>.
* <code>applyOptions</code> loads the chosen example and selection into the app state.
* <code>stepOffscreen</code> advances the simulation by whole seconds in steps of 1/60 s, exactly like the recorder does, refreshing the dominant frequency every 18 frames.

## main: the front door

@@src live.cpp | ^int main\( | ^    int rc = 0;

* <code>--selftest</code> is handled first, before anything else starts, because the tests need no window and no GDI+.
* <code>GdiplusStartup(&amp;gsi, &amp;tok, nullptr)</code> starts the drawing library and returns a token; <code>GdiplusShutdown(tok)</code> at the end closes it. The fonts are created *after* the start.
* <code>lag::loadModel(designPath(), A.model)</code> loads last session's design; if there is none (the first run), <code>presetTwoDof()</code> is used so the user always sees something.
* <code>resetView()</code> centres the view.

@@src live.cpp | ^    if \(argc > 2 && \(std::strcmp\(argv\[1\], "--shot"\) | ^    \} else \{$

* The **headless** branch. <code>--shot</code> renders **one picture** to a PNG: set the size (1320 × 800), apply the options, optionally run N seconds, draw once, save. <code>--demo</code> **records a video** without any window: it starts the encoder, runs the simulation frame by frame at exactly 1/60 s, draws each frame into the canvas, and writes it. These modes are why this guide's screenshots are real, and why we could test the recorder with no one at the keyboard.

@@src live.cpp | ^        std::printf\("Vibration Lab: a window | ^    \}$

The **interactive** branch:

* <code>WNDCLASSA wc{};</code> describes a *window class*: a template for windows. We set the window procedure (<code>lpfnWndProc = WndProc</code>), the program instance, a class name, and the arrow cursor. <code>RegisterClassA(&amp;wc)</code> registers it with Windows.
* <code>AdjustWindowRect</code> converts the *client* size we want (1320 × 800 for the drawing area) into the *outer* window size, which includes the title bar and borders.
* <code>CreateWindowA(...)</code> makes the window. <code>WS_OVERLAPPEDWINDOW | WS_VISIBLE</code> asks for a normal resizable window that is shown at once.
* <code>SetForegroundWindow</code> brings it to the front.
* **The loop** (Figure 9.1): drain the message queue with <code>PeekMessage</code>; read the clock and compute <code>dt = clampd(now - last, 0, 0.05)</code> (never more than 50 ms, so a stall cannot produce a giant jump); <code>advanceSim(A.recording ? 1.0/REC_FPS : dt)</code>; every 0.3 s refresh the frequency; <code>present()</code>; <code>captureFrame()</code>; count frames for the FPS readout every half second.
* **The pacing.** While recording, the loop sleeps until a full 1/60 s has passed, so dragging a mass during a recording feels natural and the video's time matches the real time you spent. Otherwise <code>Sleep(1)</code> yields the processor briefly so the program does not use 100 % of a CPU core for nothing.

@@src live.cpp | ^    canvas.destroy | ^\}$

* Release the canvas and shut down GDI+, then return the exit code (0 for success).

::: whatif
**What if <code>dt</code> were not clamped?** Drag the window by its title bar: Windows halts the main loop while you hold it, perhaps for several seconds. When you let go, <code>dt</code> would be several seconds and the physics would try to simulate all of that at once. The clamp turns a long pause into a tiny hiccup instead.
:::

## Chapter checks

::: quiz
1. Why do we handle <code>WM_ERASEBKGND</code> by returning 1?
2. What does <code>SetCapture</code> do for a drag?
3. Why cast the mouse coordinates to <code>(short)</code>?
4. What happens in <code>WM_DESTROY</code>, and why in that order?
5. Why is <code>dt</code> clamped to 50 ms?
6. How does the program decide between the interactive and headless modes?
:::

::: answers
1. To stop Windows erasing to white before each repaint, which would flicker; we draw every pixel ourselves.
2. It makes Windows send mouse messages to our window even when the pointer leaves it, so drags continue outside.
3. The values are packed 16-bit numbers; casting to short gives the right negative values for positions left of or above the window.
4. Recording is stopped (finishing the file), the design is saved, then the quit message ends the loop.
5. After a long stall (for example while dragging the window) one frame would otherwise demand a huge amount of simulation.
6. By the command-line arguments: <code>--shot</code> and <code>--demo</code> take the headless branch, no arguments opens the window.
:::
