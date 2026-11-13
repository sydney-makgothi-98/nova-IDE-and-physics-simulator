# Analysis Tools

## Spectrum

Choose a channel and press **Compute spectrum**. The program shows how strongly each frequency is present in that signal and marks the strongest. It removes the average first (so a constant offset is not mistaken for a very slow wave). For a damped oscillation the peak sits at the *damped* frequency.

{{img:suite-spectrum.png|The spectrum of the main mass in the tuned-mass-damper run: the response is concentrated near the system's natural frequencies.}}

## Modal analysis

A system with several moving parts can vibrate in several **modes**. Each has its own natural frequency and a shape (who moves with whom). Press **Compute natural frequencies and mode shapes** and click a mode: in design mode it is animated on the canvas.

{{img:suite-modal.png|Two coupled masses: mode 1 (1.007 Hz) moves them together, mode 2 (1.510 Hz) moves them against each other. These match the closed-form answers.}}

The analysis linearises the springs and dampers about the drawn pose; contacts and joints are ignored. A mode with 0 Hz means part of the system is not held by any spring and can drift.

## Frequency response

Choose where to push (the *input*), where to measure (the *output*), a range of frequencies, and press **Run frequency sweep**. For each frequency the program runs a full steady-state simulation and reports the amplitude and the phase lag of the response. The result is the resonance curve of the system. Each frequency is a real simulation, so a sweep takes a few seconds to a minute.

# How Accurate Is It?

Everything the program claims is tested. The validation suite (**Tools\vlab_tests.exe**, its output in **Documents\validation_report.txt**) runs 51 checks against closed-form answers and conservation laws. A selection:

| Check | Result |
|-------|--------|
| Mass-spring-damper against its exact solution (10 s) | worst difference 8 x 10^-13 m |
| Frictionless spring pendulum: energy conserved over 30 s | wandered 3 x 10^-12 J of 23.5 J |
| Pendulum period on a rigid rod | 2.8388 s measured, 2.8388 s theory |
| Rubber ball rebounds to e^2 x height | 1.281 m measured, 1.280 m theory |
| Energy lost in an impact, m g h (1 - e^2) | 3.534 J measured, 3.532 J theory |
| Equal-mass elastic collision | velocities swapped exactly; momentum and energy conserved |
| Block sliding to a stop on a flat floor | distance 5.242 m (theory 5.242 m); friction heat 36.004 J (theory 36.000 J) |
| Block on a ramp: a = g (sin - mu cos) | 1.574 m/s^2 measured and theory |
| Rolling disc: a = (2/3) g sin(angle) | 2.237 m/s^2 measured and theory |
| Rubber and steel stay put when static friction exceeds tan(angle) | slip 0.0000 m |
| Natural frequencies of two coupled masses | 1.0066 and 1.5099 Hz, as theory |
| Frequency response of a mass on a spring | 0.169 m/N measured, 0.170 theory (steady-state accuracy 0.6 %) |
| Deterministic | two runs give bit-identical results |
| Hinged beam | period 2.318 s measured, 2.316 s theory |
| Simply supported beam | the two reactions sum to the weight (490.5 N) and are equal |
| Cantilever | support force = total weight, support moment = sum of weight x distance |
| Every example's energy ledger | numerical error under 4 % of the energy that moved |

## Honest limits

* It is a **2-D** simulator. Bodies move in a plane and have a depth of 1 m.
* The material values are generic textbook numbers. Real friction depends on the surface, the speed and the temperature: measure yours for engineering decisions.
* Contacts are solved with a sequential-impulse method with a very small time step. Very high stacks, very fast thin objects and very stiff, very light parts can still misbehave; if the numbers blow up the program stops and tells you.
* It is validated against theory, but it is **not a certified engineering tool**.

# Troubleshooting and Questions

| Problem | What to do |
|---------|------------|
| "The numbers blew up" message | A very light body on a very stiff spring, or a huge force. Raise the mass, soften the spring, or reduce the physics step (Environment, Advanced). |
| "Press Stop first" | You tried to add, delete or move something while a run exists. Press **Stop**; sliders and fields still work during a run. |
| Nothing happens when I press Run | There is no moving body. Place a ball, block or polygon. |
| A body falls through the floor | Make sure both have "Collides with other bodies" ticked, collisions are on in the Environment, and the floor line is long enough to reach the body. |
| The ball never stops bouncing | You are in Ideal mode, or the restitution is 1. Press Realistic. |
| The block slides although I expect it to stick | Static friction is below tan(angle). The combined friction is the geometric mean of the two surfaces. |
| "Export failed" | The file is probably open in Excel. Close it or choose another name. |
| The video does not appear | It is in <code>Documents\Vibration Lab Suite\Recordings</code>. If the window was made smaller during recording, the program stops it safely. |
| The layout is a mess | View, Reset window layout. |
| I want to run without the window | Use <code>Tools\vlab_cli.exe --file my.vlab --duration 60 --out result.xlsx</code>. |

# Reference

## Keys and mouse

| Input | Action |
|-------|--------|
| Space | Run / pause |
| Esc | Cancel, back to Select, or stop the run |
| F9 | Start or finish recording a video |
| Ctrl+N, Ctrl+O, Ctrl+S | New, open, save a design |
| Ctrl+E | Export to Excel |
| Ctrl+Z, Ctrl+Y | Undo, redo |
| Delete | Delete the selected body or connection |
| V, C, B, P, W, T, F, A, S, D, R, J | Tools: Select, Ball, Block, Polygon, Wedge, Beam, Floor line, Anchor, Spring, Damper, Rod, Pin |
| 1, 2, 3 | Pin support, Fixed support, Roller support |
| Q, E | Turn the object being placed by 15 degrees (also Ctrl + mouse wheel) |
| Shift + click | Keep the placement tool after placing |
| Home | Fit the design in the canvas |
| Mouse wheel | Zoom about the cursor |
| Right or middle drag | Pan |
| Alt | Ignore the grid while placing or moving |

## Conventions

* Units: metres, kilograms, seconds, newtons, joules, radians per second.
* **y points down**, like the screen. Gravity pulls toward positive y. An angle of 0 points right; positive angles turn clockwise on the screen.
* A body's **dx, dy** are its displacement from where it started; **x, y** are absolute.

## The Excel channels

| Channel | Meaning |
|---------|---------|
| x, y | Position of the body's centre (m) |
| dx, dy | Displacement from the start (m) |
| angle | Rotation (degrees) |
| vx, vy, speed | Velocity components and speed (m/s) |
| omega | Spin (rad/s) |
| KE | Kinetic energy of that body (J) |
| length, stretch, force | Spring length, stretch beyond rest length, and tension (positive = pulling) |
| rate, power | Damper stretching rate, and the heat power it makes (W) |
| T kinetic, V potential, E total | Energies of the whole system (J) |
| work in, lost viscous / drag / friction / impact / total | The cumulative energy ledger (J) |
| numerical error | How far the books are from balancing (should be near zero) |

## Command line

<code>vlab_cli --list</code> lists the examples. <code>vlab_cli --example 7 --duration 8 --out ball.xlsx</code> runs example 7 and exports. <code>vlab_cli --file my.vlab --ideal --duration 30 --sample-hz 50 --out run.xlsx</code> runs your design in ideal mode. Add <code>--spectrum</code> or <code>--modal</code> to include those sheets.

## The Unreal Engine plugin

The folder <code>UnrealPlugin</code> contains a plugin that runs the same engine inside Unreal Engine 5, so a design made here can drive meshes in a game level. It is a beta scaffold; see its README.
