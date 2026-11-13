@@part Part V — Using It and Growing It
# The User Manual, with Experiments

This chapter is for the person at the keyboard. It teaches the program through six experiments. Each one has a prediction you can calculate by hand (using Chapter 3) and a result you can check against the telemetry.

## Running the program inside NOVA

1. Open the folder <code>examples/vibration-lab</code> in NOVA (**File, Open Folder**).
2. The run-target box at the top should say <code>vibration_live</code>. If it does not, click it and choose it.
3. Press the green Run triangle. NOVA builds with CMake and g++ and then starts the program, which opens its own window.

::: note
If the window seems not to appear, look in the Windows taskbar: it can open behind NOVA. NOVA's Run button hides the window of programs that are *building*, but shows the window of the program you *run*.
:::

## The screen at a glance

{{img:vl-empty.png|The empty canvas, ready for you to design something.}}

| Area | What it is for |
|------|----------------|
| Canvas (top left) | Draw the system. Grid lines are one metre apart. Mouse wheel zooms, right-button drag pans, Home resets the view. |
| Telemetry box (top left of the canvas) | Time, energies, the energy-law error, and the numbers of the selected item. |
| Plot tabs and time plot | Position, Velocity, Energy, Link force: the last 10 seconds. |
| Phase portrait | Velocity against position of the selected mass. |
| Panel: TOOLS | Select, Mass, Ground, Spring, Damper, Delete. |
| Panel: SIMULATION | Run/Pause/Resume, Stop, Record, Export CSV. |
| Panel: DESIGN FILE | Save, Load, Clear. |
| Panel: EXAMPLES | Buttons 1 to 4 load the ready-made examples. |
| Panel: PROPERTIES | Sliders for whatever is selected, plus gravity. |

## Experiment 1: build a mass on a spring from scratch

**Goal:** see that the frequency you measure agrees with ω<sub>n</sub> = √(k/m).

1. Press **Clear**. The canvas is empty.
2. Pick **Ground** and click once at the left of the canvas (a pin appears).
3. Pick **Mass** and click about three metres to the right of it (a blue block).
4. Pick **Spring**. Click the pin, then click the block. A spring appears. Pick **Damper** and do the same: a damper appears beside it.
5. Pick **Select** and click the block. In the panel set **motion** to **X rail**, and **start vx** to 3 m/s.
6. Press **Run** (or the space bar).

::: check
The block slides back and forth, slowing down. The Position tab shows a decaying wave. The telemetry shows the **dominant frequency**. For the default spring (k = 20 N/m), the default mass (m = 1 kg) and the default damper (c = 0.8 N s/m):

* ω<sub>n</sub> = √(20/1) = 4.47 rad/s, so f<sub>n</sub> = 4.47 / 2π = **0.712 Hz**;
* ζ = 0.8 / (2 √20) = 0.089, lightly damped;
* the damped frequency is almost the same, 0.709 Hz.

The dominant-frequency readout should settle near **0.71 Hz**.
:::

Now change something and predict first: select the block and raise its mass to 4 kg. The frequency should **halve** (ω<sub>n</sub> ∝ 1/√m). Change the spring's stiffness to 80 N/m with the original mass: it should **double**.

## Experiment 2: resonance

**Goal:** see a small push build a large motion.

Use the system from Experiment 1 (k = 20, m = 1, c = 0.8). Select the block and set **push force** to 5 N, **push angle** to 0° (to the right) and **push freq** to **4.5 rad/s**, which is the natural frequency.

::: check
After about 10 seconds the motion settles to a steady swing of about **1.4 metres** each side. The theory: at resonance the steady amplitude is F / (c ω) = 5 / (0.8 × 4.47) = **1.40 m**. A push of only 5 N, applied at the right rhythm, moves the block 1.4 m.
:::

Now change the push frequency to 9 rad/s (twice as fast). The amplitude shrinks to under **0.1 m**: the same push at the wrong rhythm does almost nothing. Open the **Energy** tab to see the story: at resonance the total energy climbs and then flattens when the energy fed in each cycle equals the heat burned in the damper. That is the energy law (3.13) in action.

::: whatif
**What if the damper were removed (c = 0)?** At resonance the amplitude would grow without limit: the push feeds energy every cycle and nothing takes it out. Real bridges and buildings have been damaged by exactly this. The damper is what keeps resonance from being destructive.
:::

## Experiment 3: a car suspension

{{fig:gl-quartercar|The quarter-car model of example 4: a tyre spring, a wheel, a suspension spring and damper, and the car body. Both masses move only up and down.}}

Press **4** (the quarter-car). Press Run and watch the wheel hit a bump. The body rides up and then settles.

* Select the **damper** and move **damping c** while it runs (sliders work *live*): c = 1 makes the car bounce many times; c = 5 (the default) settles in about a second; c = 25 makes it stiff and slow.
* Open the **Link force** tab with the damper selected. The force curve is the number a shock-absorber designer cares about.
* Raise the **body mass** (select the top block): a heavier car sags more. The static sag is m g / k, formula (3.18).

{{img:vl-quarter-energy.png|The quarter-car's Energy tab. Kinetic and potential energy trade places while the dampers steadily burn the total away. Gravity has shifted the potential energy to negative values (the car has sagged below where it was drawn).}}

## Experiment 4: a hardening spring

**Goal:** see that for a non-linear spring the frequency depends on how far it swings.

Use Experiment 1's system with the damper's c set to a small number such as 0.05, and set the spring's **cubic k3** to 200. Make the spring start stretched by changing its **rest length** to a little less than the drawn length.

::: check
With a small start (stretch 0.2 m) the frequency is close to the linear value, about **0.79 Hz**. With a large start (stretch 1 m) it rises to about **1.9 Hz**, more than double. The stiffer the spring gets as it stretches, the faster it vibrates. This amplitude-dependent frequency is the signature of the **Duffing oscillator**. A linear spring (k3 = 0) always gives 0.71 Hz, whatever the amplitude.
:::

## Experiment 5: a spring pendulum

Press **3**. The mass swings and bounces at once. Select the mass and open the Phase portrait: the curve never repeats exactly.

{{img:vl-pendulum.png|The spring pendulum (example 3) after four seconds. The mass has swung and bounced, the plot shows both x and y, and the phase portrait has not closed on itself.}}

Try this: note the picture, press Stop, nudge the mass by one grid step (0.25 m) with the Select tool, and run again. The two runs look the same at first and then separate. That sensitivity to tiny changes is a hallmark of **chaotic** motion, and it is the best reason this engine is two-dimensional.

## Experiment 6: record a video

1. Design or load a system and press **Run**.
2. Press **Record** (or F9). A red dot and "REC 00:03" appear in the corner of the canvas.
3. Do something interesting: drag a mass, change a slider.
4. Press Record again. A message says where the file went.

The video is in the folder <code>examples/vibration-lab/recordings</code>, named <code>vibration_YYYYMMDD_HHMMSS.mp4</code>. Any player can open it (Windows Media Player, Movies &amp; TV, a browser, PowerPoint). It contains the canvas and the plots, but not the control panel.

::: tip
For a cleaner video, press Run first and then Record, so the video starts when the motion starts. To make a video of a pure replay with nothing but the physics, press Stop, press Record, then press Run.
:::

## What every number in the telemetry means

| Number | Meaning |
|--------|---------|
| t | Simulated time, in seconds |
| T | Kinetic energy: motion |
| V | Potential energy: springs, and height if gravity is on |
| E = T + V | Total energy. It falls as dampers work and rises when you push |
| dissipated | Total heat made by dampers so far |
| energy-law error | How far E is from (start + work in − heat). **Should be tiny** (about 10⁻⁵ J or less) |
| x, y | Displacement of the selected mass from where it started |
| &#124;v&#124;, &#124;a&#124; | Speed and acceleration of the selected mass |
| dominant freq | The strongest vibration frequency, in hertz |
| length, stretch | For a spring: how long, and how far from its rest length |
| force | For a link: the tension. Positive means pulling its ends together |
| power lost | For a damper: the heat it is making this instant (c ṡ²) |

## Keys and mouse

| Input | Action |
|-------|--------|
| Space | Run / pause / resume |
| Esc | Cancel a half-drawn link, or stop the run, or go back to the Select tool |
| S, M, G, K, D | Tools: Select, Mass, Ground, sprinK, Damper |
| Delete or Backspace | Delete the selected item |
| 1, 2, 3, 4 | Load an example |
| F9 | Start or finish recording |
| Tab | Next plot tab |
| , and . | Slower and faster (0.125× to 4×) |
| Home | Reset the view |
| Ctrl+S, Ctrl+O | Save / load <code>design.vlab</code> |
| Wheel | Zoom about the cursor |
| Right-button drag | Pan |
| Left-drag a mass (running) | Pull it with the mouse spring |
| Left-drag an item (designing, Select tool) | Move it, snapping to the 0.25 m grid |

## When something goes wrong

| What you see | What it means and what to do |
|--------------|------------------------------|
| "Numbers blew up (too stiff or too light)" | A mass is very light with a very stiff spring, or a huge push. The program stopped itself. Raise the mass or lower the stiffness. |
| "Press Stop to edit the layout" | You tried to add or delete something while running. Press Stop first. Sliders and dragging masses still work. |
| "Cannot connect those two" | You tried to join a node to itself, two grounds, or repeat the same link. |
| The energy-law error is large | You changed a slider while running (it rebases) or something is wrong. Press Stop and Run again; if it still grows, report it. |
| Nothing happens when you press Run | There is no mass on the canvas. |
| The video looks wrong or will not open | The window may have been resized during recording (the program stops recording if it became smaller than the video). Record again. |
| The window never appears when started from NOVA | It may be behind NOVA, or you are running an old NOVA. Make sure the build you run is current. |

## Chapter checks

::: quiz
1. You double the mass on a spring. What happens to the natural frequency?
2. At resonance, what limits the amplitude?
3. In Experiment 4, why does the frequency rise with the starting stretch?
4. Why is the video width smaller than the window width?
5. What is the energy-law error and what size should it be?
:::

::: answers
1. It falls by a factor √2: f<sub>n</sub> ∝ 1/√m, so about 0.707 of before.
2. The damper: at steady state the energy fed in per cycle equals the heat burned per cycle.
3. The cubic term k₃ s³ makes the spring stiffer as it stretches, and a stiffer spring vibrates faster.
4. The control panel (300 pixels) is left out of the video; only the canvas and plots are recorded.
5. The difference between the measured energy and (start + work in − heat); it should be tiny, around 10⁻⁵ J or less.
:::

# Where to Go Next

The engine was built so that each new feature is a **small addition**, not a rewrite. This chapter lists projects in increasing difficulty. For each, the basic steps are given so you can start.

## Level 1: small and safe

**1. Undo and redo.** *Idea:* store a copy of the <code>Model</code> before every edit. *Steps:* (a) add <code>std::vector&lt;lag::Model&gt; undoStack</code> to <code>App</code>; (b) at the start of each function that changes the layout (<code>addMass</code> calls, <code>deleteSelection</code>, drags), push <code>A.model</code>; (c) Ctrl+Z pops the last model back; (d) Ctrl+Y for redo with a second stack. The design is only a few hundred bytes, so copying it is cheap: this is why <code>Model</code> is plain data with no pointers.

**2. Type numbers into the sliders.** *Steps:* add a small text box state (the property being edited and a string); on double-click, capture keyboard characters in <code>WM_CHAR</code>; on Enter parse the number with <code>std::stod</code> and write it through the same <code>Prop::p</code> pointer.

**3. A parameter-sweep plot.** *Idea:* the *frequency response*: amplitude against drive frequency. *Steps:* a loop that runs the engine headless (no window) for each frequency from 0.5 to 15 rad/s, records the steady amplitude, and plots it. The peak lands at ω<sub>n</sub>. This uses only <code>lag::advance</code>, which is why the engine knows nothing about windows.

## Level 2: new physics

**4. More elements.** Each new part is a new term in T, V, D or Q:

| Element | Term | New force |
|---------|------|-----------|
| Inerter (mass-like link) | T = ½ b ṡ² | b s̈ along the link |
| Dry friction (Coulomb) | D-like, force c·sign(ṡ) | constant magnitude, opposite to motion |
| Base motion | position of a ground anchor moves: r(t) | spring and damper see a moving end |
| Bump stop | V = ½ k (s − s₀)² only when s &gt; s₀ | one-sided spring |

**5. Rigid rods (pendulums, linkages).** A rod fixes the distance between two masses: the constraint |r<sub>b</sub> − r<sub>a</sub>| = ℓ. The proper Lagrangian way uses **Lagrange multipliers**. A simple first version replaces the rod by a *very stiff spring*, which works if you accept the stiffness limits of Chapter 4 (a smaller step).

**6. Angular coordinates and rigid bodies.** Give a body an angle θ and a moment of inertia I: add T = ½ I θ̇². The generalized coordinates grow from (x, y) to (x, y, θ), and a link attached off-centre needs its attachment point rotated, as in the rotation trick of <code>drawLink</code>.

## Level 3: better numerics

**7. Energy-friendly integrators.** RK4 slowly loses or gains energy over very long runs. A **velocity-Verlet** (symplectic) integrator keeps the energy bounded for conservative systems; swap it into <code>rk4()</code>'s place and compare with the Meter.

**8. Adaptive steps (RK45).** Choose the step from an error estimate: small steps when the motion is violent, large steps when it is calm. The accumulator in <code>advanceSim</code> already separates the step size from the frame rate.

**9. Modal analysis.** Linearise the system about rest, build the mass matrix M and the stiffness matrix K, and compute the eigenvalues of M⁻¹K. The square roots are the natural frequencies. This turns the "dominant frequency" guess into the full list for any design.

## Level 4: the interface and the recorder

**10. A live FFT plot.** The frequency finder already computes the whole spectrum; add a fifth tab that plots it.

**11. Audio.** Turn the motion of a mass into a tone (the displacement is a waveform). Windows offers <code>waveOut</code> or WASAPI.

**12. Audio inside the video.** Add a second stream to the Sink Writer: <code>AddStream</code> with an AAC output type and a PCM input type. Time-stamp the audio samples with the same fixed clock.

**13. A cross-platform version.** Replace Win32 + GDI+ with SDL2 for the window and Cairo for the drawing, and ffmpeg's libraries for the video. The engine, <code>model_io.hpp</code> and <code>app.hpp</code> do not change: that is what separation of concerns buys.

**14. A web version.** Port <code>engine.hpp</code> to JavaScript (it is a few hundred lines of plain arithmetic), draw on a <code>&lt;canvas&gt;</code>, and record with the browser's <code>MediaRecorder</code>.

## Level 5: larger projects

**15. Finite elements.** A beam is a long chain of masses and springs. Add a "chain" tool that places *n* masses and links between two points, then watch waves travel along it.

**16. Optimisation.** Write a program that varies a suspension's spring and damper and searches for the setting that gives the smoothest ride (smallest peak body acceleration over a bump). It would call <code>lag::advance</code> thousands of times, headless.

## A habit for every project

Whatever you build, add a check to <code>--selftest</code> *first*. A new element is only finished when it passes a test against a formula or a conservation law.

## Chapter checks

::: quiz
1. Why is undo easy in this program?
2. What new physics term would you add for a one-sided bump stop?
3. Why might a velocity-Verlet integrator be preferred for very long runs?
4. Which files would not change in a cross-platform port?
:::

::: answers
1. <code>Model</code> is plain data with no pointers, so copying it before each edit is cheap and safe.
2. A spring-energy term that only exists when the stretch exceeds the gap: V = ½ k (s − s₀)² for s &gt; s₀, otherwise 0.
3. It keeps the energy bounded instead of drifting slowly.
4. <code>engine.hpp</code>, <code>model_io.hpp</code> and <code>app.hpp</code> (the drawing, window and recorder files are the ones that change).
:::

# Appendix A: Glossary

| Word | Meaning |
|------|---------|
| **Action (S)** | The integral of the Lagrangian over time. The real path makes it stationary. |
| **Accumulator** | A bank of leftover real time, spent in fixed 1 ms physics steps (<code>simAcc</code>). |
| **Anti-aliasing** | Smoothing the edges of drawn shapes so they do not look jagged. |
| **Bitrate** | Bits of compressed video per second. |
| **BitBlt** | "Bit-block transfer": copy a rectangle of pixels from one place to another. |
| **Canvas** | In the program: the hidden bitmap everything is drawn into. In the window: the drawing area. |
| **Codec** | A compression method (H.264). |
| **COM** | Component Object Model: Microsoft's system of objects with reference counts (Media Foundation uses it). |
| **Container** | The file format that holds compressed video plus an index (MP4). |
| **Critical damping** | ζ = 1: the fastest return to rest without overshoot. |
| **CSV** | Comma-separated values: a plain text table that spreadsheets open. |
| **Damper (dashpot)** | A part whose force opposes speed: F = c ṡ. |
| **Damping ratio (ζ)** | How strong the damping is compared with critical. |
| **Degrees of freedom** | The number of independent numbers describing the system's position. |
| **Dissipation (D)** | Rayleigh function ½ c ṡ²; heat power is 2D. |
| **DIB section** | A bitmap whose pixel memory we can access directly. |
| **Double buffering** | Drawing to a hidden picture then copying it to the window in one move. |
| **Duffing oscillator** | A mass on a spring with a cubic term; its frequency depends on amplitude. |
| **Euler–Lagrange equation** | d/dt(∂L/∂q̇) − ∂L/∂q = 0 (plus ∂D/∂q̇ = Q with dissipation and forces). |
| **Frame** | One still picture of a video, or one row of telemetry. |
| **Generalized coordinates** | The list of numbers q describing where everything is. |
| **Generalized force (Q)** | Outside forces written for the chosen coordinates. |
| **GDI+** | Windows' built-in 2-D drawing library. |
| **H.264** | The most widely used video codec. |
| **Hann window** | A fade-in/fade-out weighting used before a Fourier test. |
| **HRESULT** | A COM result code: negative means failure. |
| **Kinetic energy (T)** | Energy of motion, ½ m v². |
| **Lagrangian (L)** | T − V. |
| **Media Foundation** | Windows' multimedia framework, including the H.264 encoder we use. |
| **Message** | A note from Windows about an event (mouse, key, resize). |
| **Namespace** | A labelled box of names, used to prevent clashes. |
| **Natural frequency (ωn)** | √(k/m): how fast it would vibrate without damping. |
| **Nyquist limit** | The highest frequency you can see in samples: half the sample rate. |
| **Phase portrait** | A plot of velocity against position. |
| **Potential energy (V)** | Energy stored by position (springs, height). |
| **Rail** | A constraint letting a mass move along only x or only y. |
| **RAII** | Resource Acquisition Is Initialisation: objects clean up after themselves. |
| **Resonance** | Large motion when a push matches the natural frequency. |
| **RK4** | Fourth-order Runge–Kutta, a method for stepping differential equations. |
| **Sink Writer** | The Media Foundation object that encodes and writes a media file. |
| **State** | The positions and velocities at one instant. |
| **Stretch (s)** | Length minus rest length of a spring. |
| **Telemetry** | The measured numbers (and plots) shown while running. |
| **Toast** | A short message that appears and fades. |
| **Trapezoid rule** | Integrating by averaging the two ends of each step. |
| **Unit vector** | A direction arrow of length exactly 1. |
| **Window procedure** | The function that receives a window's messages. |

# Appendix B: Maths to Code

Every symbol of the derivation, and the name it has in the program.

| Symbol | Meaning | Code |
|--------|---------|------|
| q, v | positions, velocities | <code>State::q</code>, <code>State::v</code> (flat arrays, 2 per node) |
| m<sub>i</sub> | mass | <code>Node::m</code> |
| r<sub>a</sub>, r<sub>b</sub> | positions of the two link ends | <code>q[2*L.a]</code>, <code>q[2*L.b]</code> (and +1 for y) |
| d | vector from a to b | <code>dx, dy</code> in <code>linkInfo</code> |
| ℓ | link length | <code>LinkInfo::len</code> |
| u | unit vector | <code>LinkInfo::ux, uy</code> |
| L₀ | rest length | <code>Link::L0</code> |
| s | stretch | <code>LinkInfo::stretch</code> |
| ṡ | rate of change of length | <code>LinkInfo::rate</code> |
| k, k₃ | linear and cubic stiffness | <code>Link::k</code>, <code>Link::k3</code> |
| c | damping coefficient | <code>Link::c</code> |
| f | tension in a link | <code>LinkInfo::force</code> |
| g | gravity | <code>Model::gravity</code> |
| F₀, θ, ω | drive amplitude, angle, frequency | <code>driveF</code>, <code>driveAngle</code>, <code>driveW</code> |
| Q | outside forces | <code>driveForce(...)</code> + <code>ext</code> |
| T | kinetic energy | <code>kinetic()</code> |
| V | potential energy | <code>potential()</code> |
| D | dissipation | <code>rayleigh()</code> |
| a | accelerations | <code>accel()</code> output |
| h | time step | <code>SIM_STEP</code> (0.001 s) |
| k₁…k₄ | RK4 slopes | <code>a1..a4</code>, <code>v, v2, v3, v4</code> |
| E, E₀ | total energy, start energy | <code>Meter::E</code>, <code>Meter::E0</code> |
| P<sub>in</sub> | input power Σ Q·v | <code>inputPower()</code>, <code>Meter::workIn</code> |
| 2D | heat power | <code>2.0 * rayleigh()</code>, <code>Meter::dissipated</code> |
| ω<sub>n</sub>, ζ, ω<sub>d</sub> | natural, damping ratio, damped | computed in <code>selfTest</code> and the telemetry |

# Appendix C: Files and Folders

```text
examples/vibration-lab/
  CMakeLists.txt     one target: vibration_live (live.cpp)
  engine.hpp         the Lagrangian physics
  model_io.hpp       save / load / the four examples
  app.hpp            state, clicks, keys, sliders
  render.hpp         all drawing
  encoder.hpp        H.264 video encoder (Media Foundation)
  live.cpp           window, main loop, recording, CSV, self-test
  design.vlab        your last design (written when you close the window)
  recordings/        videos (.mp4) and telemetry (.csv)
  cmake-build-debug/ created by NOVA when it builds
```

| Command | What it does |
|---------|--------------|
| <code>vibration_live</code> | Opens the window |
| <code>vibration_live --selftest</code> | Runs the nine checks; exit code 0 means all passed |
| <code>vibration_live --shot out.png --preset 2 --run 5 --select node:1 --tab 0</code> | Draws one picture without a window |
| <code>vibration_live --demo out.mp4 --preset 3 --run 6</code> | Records a video without a window |
