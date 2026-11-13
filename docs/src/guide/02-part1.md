@@part Part I — The Idea
# What We Built

## A tour of the finished program

The Vibration Lab is one window. The big area at the top-left is the **canvas**, where you draw. The area below it holds live **plots**. The strip on the right is the **panel**, with tools, buttons and sliders. The box in the corner of the canvas is the **telemetry** readout.

{{img:vl-design.png|The Vibration Lab in DESIGN mode: two masses on a rail between two walls (example 2). The right panel holds the tools and the sliders for whatever is selected.}}

You always do things in the same order:

1. **Design.** Pick the **Ground** tool and click to place a wall or anchor. Pick the **Mass** tool and click to place a block. Pick **Spring** (or **Damper**) and click two things to join them.
2. **Tune.** Pick the **Select** tool, click a mass or a link, and use the sliders to set its mass, stiffness, damping, starting speed and so on.
3. **Run.** Press **Run** (or the space bar). The physics starts. You can still drag any mass with the mouse, as if you were holding it with a rubber band.
4. **Read the telemetry.** Numbers, plots and a phase portrait show what the system is doing. The *energy-law error* shows whether the simulation obeys the laws of physics.
5. **Record.** Press **Record** (or F9). The picture is written to an .mp4 file in the folder <code>recordings</code>. Press **Export CSV** to save every number to a spreadsheet file.

{{img:vl-quarter-force.png|A running simulation (example 4, a quarter-car suspension). The damper is selected, so the telemetry box shows its force and the bottom plot shows that force over time.}}

## The requirements we started from

Before writing code, we wrote down what the program had to do. A list like this is called the **requirements**. Each one later became a feature, and each feature became code you will meet in this guide.

| # | Requirement | Where it lives |
|---|-------------|----------------|
| 1 | Simulate masses, springs, dampers and fixed ground, in two dimensions | <code>engine.hpp</code> |
| 2 | Derive the equations of motion with the Lagrangian method, not with force diagrams | Chapters 2 and 3, <code>accel()</code> |
| 3 | Never "blow up" for sensible inputs, and prove it is right | RK4 in Chapter 4, tests in Chapter 14 |
| 4 | Let the user design a system by clicking, with no code to edit | <code>app.hpp</code> |
| 5 | Show telemetry (numbers and plots) while it runs | <code>render.hpp</code>, <code>Frame</code> log |
| 6 | Record the screen to a standard video file | <code>encoder.hpp</code> |
| 7 | Keep designs between sessions, and export data | <code>model_io.hpp</code>, CSV export |
| 8 | Run well on a plain Windows PC with nothing extra installed | Win32 + GDI+ + Media Foundation |

## The five source files

{{fig:gl-files|The five source files and who includes whom. The physics (engine.hpp) knows nothing about windows or pictures, which is why it can be tested without opening a window.}}

::: why
**Why is the physics kept in its own file that knows nothing about drawing?** Because then the physics can be tested on its own, quickly and exactly (Chapter 14), and later reused in a different program: a web page, a game, a command-line tool. This idea is called **separation of concerns**: each file has one job.
:::

| File | Lines | Its one job |
|------|-------|-------------|
| <code>engine.hpp</code> | about 310 | The model of the system and the laws of motion |
| <code>model_io.hpp</code> | about 130 | Save, load and ready-made example designs |
| <code>app.hpp</code> | about 430 | What the program remembers, and what every click and key does |
| <code>render.hpp</code> | about 490 | Everything painted on screen |
| <code>encoder.hpp</code> | about 150 | Writing frames into an H.264 video file |
| <code>live.cpp</code> | about 460 | The window, the main loop, recording, CSV export, the self-test |

The header files end in <code>.hpp</code> because they contain code that other files include. The program has exactly **one** file that gets compiled on its own: <code>live.cpp</code>. It includes the others. That keeps the build simple: <code>CMakeLists.txt</code> has a single <code>add_executable</code> line.

## Building and running

::: try
1. Open the folder <code>examples/vibration-lab</code> in NOVA IDE. The run-target box at the top should say <code>vibration_live</code>.
2. Press the green Run triangle. CMake configures, g++ compiles, and a window opens.
3. Press **Run** inside the window and drag a block with the mouse.
:::

From a terminal the same thing is:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
build/vibration_live.exe              # opens the window
build/vibration_live.exe --selftest   # runs the nine checks, no window
```

::: check
<code>--selftest</code> should end with the words **all checks passed**. Chapter 14 explains each of the nine checks.
:::

## Chapter checks

::: quiz
1. Name the three big parts of the program, in one word each.
2. Why is <code>engine.hpp</code> separate from <code>render.hpp</code>?
3. What do the buttons Run, Stop and Record each do?
4. Which file is compiled on its own?
:::

::: answers
1. Physics (engine), interface (designer), recorder (video).
2. So the physics can be tested without a window, and reused elsewhere.
3. Run starts or pauses the simulation, Stop returns to design mode, Record starts or finishes the video file.
4. <code>live.cpp</code> (it includes the other files).
:::

# From Newton to Lagrange

## Newton's way, and where it hurts

You probably learned physics the Newton way: draw the object, draw every force on it, add the forces, and say **F = m a**. For one block on one spring this is easy. For a real machine it becomes painful:

* A machine with ten parts needs ten force diagrams.
* Some forces are **constraint forces** (the force a rail pushes back with so the block cannot leave it). They are unknown at the start, and you must solve for them.
* Every time you add a part, you must redraw diagrams and redo the algebra.

A program that lets the *user* add parts by clicking cannot ask the user to redo algebra. We needed a method that works for **any** collection of parts with the same recipe, every time. That method is the **Lagrangian approach**, invented by Joseph-Louis Lagrange in 1788.

## The big idea: use energy, not forces

Forces point in directions, so adding them needs care. **Energy is just a number.** Energy has no direction. Lagrange's idea: describe the system by its **energies**, and let a short, fixed piece of calculus turn those energies into the equations of motion.

You need only three energy-like quantities:

::: def
**Kinetic energy (T).** Energy of motion. For a mass: T = ½ m v².

**Potential energy (V).** Energy stored by position. A stretched spring stores V = ½ k s². A raised mass stores V = m g h.

**Dissipation (D).** A measure of how fast energy is being turned into heat by friction-like parts such as dampers.
:::

## Generalized coordinates: how many numbers describe the system?

To describe where everything is, you need a list of numbers. The list is called the **generalized coordinates**, written **q**. You choose the smallest list that fully fixes the position of every part.

{{fig:gl-coords|Left: two masses on a rail need two numbers. Right: one mass free in the plane also needs two numbers.}}

::: def
**Degrees of freedom (DOF).** The number of independent numbers needed to describe the position of the system. Two masses on a rail: 2 DOF. One free mass in a plane: 2 DOF. A free mass in a plane that is forced onto a rail: only 1 DOF.
:::

Our engine stores *two* numbers for every node (<code>x</code> and <code>y</code>), and then **removes** a degree of freedom for any mass that is on a rail. That is simpler to code than choosing a different coordinate list for every design.

## The Lagrangian and Hamilton's principle

Write the **Lagrangian** as the kinetic energy minus the potential energy:

<div class="eq">L = T − V <span class="tag">(2.1)</span></div>

Why subtract? Nobody can give a one-line reason; it is simply what works. A deeper justification is **Hamilton's principle**: out of all the paths a system could follow between two moments in time, the real path is the one that makes the **action** *stationary*, where

<div class="eq">S = ∫ L dt   (from t₁ to t₂) <span class="tag">(2.2)</span></div>

"Stationary" means that if you nudge the path a tiny bit, the action does not change to first order, in the same way that the bottom of a bowl is flat for a tiny nudge.

{{fig:gl-variation|The real path (thick) and some nearby nudged paths (dashed). All start and end at the same two points. The real path is the one where the action stops changing.}}

## Deriving the Euler–Lagrange equation

This short derivation is the engine of the whole method. Take it slowly; each line is simple.

**Step 1.** Suppose <code>q(t)</code> is the real path. Make a nudged path <code>q(t) + ε η(t)</code>, where ε is a small number and η(t) is any smooth shape that is **zero at both ends** (the nudged path must start and finish at the same points).

**Step 2.** The action of the nudged path is a function of ε. If the real path is stationary, its derivative with respect to ε is zero at ε = 0:

<div class="eq">dS/dε = ∫ ( ∂L/∂q · η  +  ∂L/∂q̇ · η̇ ) dt = 0 <span class="tag">(2.3)</span></div>

(We used the chain rule: L depends on q and on the speed q̇.)

**Step 3.** Integrate the second term by parts. That moves the time-derivative from η onto the other factor and produces a minus sign:

<div class="eq">∫ ∂L/∂q̇ · η̇ dt = [ ∂L/∂q̇ · η ] − ∫ d/dt( ∂L/∂q̇ ) · η dt <span class="tag">(2.4)</span></div>

The bracket term is zero, because η is zero at both ends.

**Step 4.** Put it together:

<div class="eq">∫ ( ∂L/∂q − d/dt( ∂L/∂q̇ ) ) · η dt = 0 <span class="tag">(2.5)</span></div>

This must be true for **every** shape η. The only way that can happen is for the bracket to be zero at every moment:

<div class="eq"><b>d/dt( ∂L/∂q̇ ) − ∂L/∂q = 0</b> <span class="tag">(2.6)</span></div>

That is the **Euler–Lagrange equation**, one equation for each coordinate q. Notice what happened: we never mentioned a force, a rail, or a free-body diagram.

## What Hamilton's principle leaves out: damping and pushes

Hamilton's principle describes systems that keep their energy. Real systems have dampers (which turn energy into heat) and outside pushes (a motor, or your mouse). Both fit into the same equation, with two extra terms:

<div class="eq">d/dt( ∂L/∂q̇ ) − ∂L/∂q + ∂D/∂q̇ = Q <span class="tag">(2.7)</span></div>

::: def
**Rayleigh dissipation function (D).** For a viscous damper (a dashpot whose force is proportional to speed), D = ½ c ṡ², where c is the damping coefficient and ṡ is how fast its two ends are moving apart. It is named after Lord Rayleigh. The term ∂D/∂q̇ gives the damper's force.

**Generalized force (Q).** The outside forces, written in the coordinates we chose. For a push F on a mass that moves along coordinate x, Q = F.
:::

::: why
**Why a damper's force is c·ṡ.** Differentiating D = ½ c ṡ² with respect to the speed gives c ṡ: a force that grows in step with the speed, and which opposes it. This is exactly how a shock absorber behaves, which is why D is the right tool.
:::

## The first worked example: one mass, one spring, one damper

Let the coordinate be <code>x</code> (the displacement from rest). Then:

<div class="eq">T = ½ m ẋ²,    V = ½ k x²,    D = ½ c ẋ²,    Q = F(t) <span class="tag">(2.8)</span></div>

Compute each piece of (2.7):

* ∂L/∂ẋ = ∂T/∂ẋ = m ẋ, so d/dt of it is **m ẍ**.
* ∂L/∂x = −∂V/∂x = **−k x**.
* ∂D/∂ẋ = **c ẋ**.

Put them in (2.7):

<div class="eq"><b>m ẍ + c ẋ + k x = F(t)</b> <span class="tag">(2.9)</span></div>

This is the most famous equation in vibration theory. We obtained it **without drawing a force diagram**. The same three-step recipe (write T, V, D; differentiate; assemble) will give us the equations for a whole network of parts in Chapter 3.

## Rails: constraints made easy

A **rail** forces a mass to move along a line only. A *horizontal* rail (X rail) allows only x; a *vertical* rail (Y rail) allows only y. In the Lagrangian method, a rail simply **deletes** one coordinate: the blocked coordinate is no longer a variable, so the rail's pushing force never appears in the equations.

In the code, we keep both x and y for every node, and we **zero the acceleration and the starting speed in the blocked direction**. Chapter 6 shows the two lines that do it, and test 4 in Chapter 14 proves the blocked direction never drifts.

::: whatif
**What if we used Newton's way for rails?** We would have to compute, at every instant, how hard the rail pushes sideways so that the sideways acceleration is exactly zero. With Lagrange, that push never has to be computed at all, because the rail is built into the coordinates.
:::

## Why Lagrange suits a program that users extend by clicking

{{fig:gl-recipe|The Lagrangian recipe. Steps 1 to 4 are algebra, done once on paper (Chapter 3). Step 5 is done by the computer, thousands of times per second.}}

Adding a new kind of part (say, a *friction* element) means adding **one new term** to T, V, D or Q, then differentiating that one term. Nothing else changes. That is why the engine in Chapter 6 is one short function (<code>accel</code>) with one loop over links and one loop over masses.

## Chapter checks

::: quiz
1. What is the Lagrangian L?
2. What does it mean for an action to be "stationary"?
3. Why must the nudge η be zero at both ends?
4. For T = ½ m ẋ², what is ∂T/∂ẋ? And its time derivative?
5. In the Euler–Lagrange equation with dissipation, which term gives the damper's force?
6. How many degrees of freedom do three masses have if one is on an X rail and two are free in the plane?
7. Why does the rail's pushing force never appear in our equations?
:::

::: answers
1. L = T − V, kinetic minus potential energy.
2. A tiny nudge of the path does not change the action to first order (like the flat bottom of a bowl).
3. The nudged path must start and end at the same two points as the real path; also it makes the boundary term in the by-parts step vanish.
4. ∂T/∂ẋ = m ẋ; its time derivative is m ẍ.
5. ∂D/∂q̇ (it equals c ṡ for a damper).
6. 1 + 2 + 2 = 5.
7. Because the rail removes a coordinate, so the force that holds the mass on the rail does no work and never enters the equations.
:::

# Deriving Our Equations

In Chapter 2 we met the method. Now we use it on the real model: masses and ground anchors in a plane, joined by springs and dampers, with gravity, a harmonic push, and a mouse force. We will derive every formula that appears in <code>engine.hpp</code>.

## Step 1: the coordinates

Every node *i* (a mass or a ground anchor) has a position (x<sub>i</sub>, y<sub>i</sub>) and a velocity (v<sub>x,i</sub>, v<sub>y,i</sub>). The program stores them in two flat arrays:

<div class="eq">q = ( x₀, y₀, x₁, y₁, … )   v = ( vₓ₀, v<sub>y0</sub>, vₓ₁, v<sub>y1</sub>, … ) <span class="tag">(3.1)</span></div>

So node *i* lives at <code>q[2*i]</code> and <code>q[2*i+1]</code>. A ground anchor is just a node that never moves.

## Step 2: the geometry of one link

A spring or damper joins two nodes, *a* and *b*. Three quantities describe it:

{{fig:gl-spring-geom|The geometry of one link. Everything the engine needs comes from the vector d from a to b.}}

<div class="eq">d = r<sub>b</sub> − r<sub>a</sub>,   ℓ = |d| = √(dₓ² + d<sub>y</sub>²),   u = d / ℓ <span class="tag">(3.2)</span></div>

* **d** is the arrow from *a* to *b*.
* **ℓ** (the length) is how long that arrow is.
* **u** is the arrow shrunk to length 1: the **unit vector** that points along the link.

We need one more fact: how the length changes when a node moves. Differentiate ℓ = √(dₓ² + d<sub>y</sub>²) with respect to the x-position of *b*:

<div class="eq">∂ℓ/∂x<sub>b</sub> = dₓ / ℓ = uₓ,   and similarly   ∂ℓ/∂y<sub>b</sub> = u<sub>y</sub> <span class="tag">(3.3)</span></div>

For node *a*, d has a minus sign, so ∂ℓ/∂r<sub>a</sub> = −**u**. In words: **stretching the link by moving b along u makes it longer; moving a along u makes it shorter.**

## Step 3: the spring's potential energy and its force

Let L₀ be the spring's rest length. The **stretch** is s = ℓ − L₀. A spring stores

<div class="eq">V<sub>spring</sub> = ½ k s² + ¼ k₃ s⁴ <span class="tag">(3.4)</span></div>

The first term is the ideal (linear) spring. The second term, with the **cubic stiffness** k₃, makes the spring harder to stretch the further it goes: a "hardening" spring. When k₃ = 0 it vanishes.

We need the *gradient* of V with respect to the coordinates. The chain rule links V to the coordinates through s:

<div class="eq">∂V/∂r<sub>b</sub> = (dV/ds) · (∂s/∂r<sub>b</sub>) = f · u,    ∂V/∂r<sub>a</sub> = − f · u,    where  f = k s + k₃ s³ <span class="tag">(3.5)</span></div>

The force on a node is **minus** the gradient of the potential (a ball rolls *down* the hill). So:

<div class="eq">force of spring on b = − f u,    force of spring on a = + f u <span class="tag">(3.6)</span></div>

Check the signs with a feeling: if the spring is stretched, s > 0 and f > 0. The force on *b* is −f**u**, which points back toward *a*. The force on *a* is +f**u**, pointing toward *b*. A stretched spring pulls its two ends together, exactly as it should.

::: mapx
In the code, **f** is the field <code>force</code> that <code>linkInfo()</code> computes for a spring, and the two signs in (3.6) are the lines <code>F[2*L.a] += in.force * in.ux</code> and <code>F[2*L.b] -= in.force * in.ux</code> inside <code>accel()</code>.
:::

## Step 4: the damper's dissipation and its force

How fast does the link's length change? Differentiate ℓ with respect to time, using (3.3) and the chain rule:

<div class="eq">ṡ = dℓ/dt = ∂ℓ/∂r<sub>b</sub> · v<sub>b</sub> + ∂ℓ/∂r<sub>a</sub> · v<sub>a</sub> = u · ( v<sub>b</sub> − v<sub>a</sub> ) <span class="tag">(3.7)</span></div>

So ṡ is the part of the *relative velocity* that lies along the link. A velocity sideways to the link does not change its length and a damper does not resist it.

A damper with coefficient c has

<div class="eq">D<sub>damper</sub> = ½ c ṡ²,    ∂D/∂v<sub>b</sub> = c ṡ u,    ∂D/∂v<sub>a</sub> = − c ṡ u <span class="tag">(3.8)</span></div>

and the damper force on *b* is −(c ṡ) **u** and on *a* is +(c ṡ) **u**. The very same signs as the spring. That is why, in the code, a spring and a damper differ only in how they compute the number called <code>force</code>; the code that *applies* it is shared.

::: why
**Why this sharing matters.** The Lagrangian method told us that a damper's equations have the same shape as a spring's. We wrote the "apply a force along the link" code **once** and used it for both. If we had derived each by force diagrams, we might never have noticed.
:::

## Step 5: gravity

Gravity pulls toward +y (screen coordinates have y pointing down). A mass at height y has potential energy that **falls** as y grows, measured from its starting y₀:

<div class="eq">V<sub>g</sub> = − m g ( y − y₀ ) <span class="tag">(3.9)</span></div>

Differentiating, ∂V<sub>g</sub>/∂y = −m g, so the force is +m g, pointing down. We subtract y₀ so that the potential energy starts at zero and the readout is easy to read; subtracting a constant changes no forces.

## Step 6: the outside forces

Two kinds of outside force are included:

* **The drive:** a harmonic push on any mass, F(t) = F₀ cos(ω t) in a direction at angle θ: components (F cos θ, F sin θ). With θ = 0 it pushes right; with θ = 90° it pushes down (y is down).
* **The mouse spring:** when you drag a mass, the program adds a force that pulls it toward the cursor, F = m ( 700 (target − q) − 45 v ). It is a critically-damped spring tied to the cursor. The gains 700 and 45 were chosen so the mass follows quickly without ringing. Multiplying by m makes a heavy mass and a light mass feel equally "grabbable".

Both are **generalized forces Q**. Together with the springs and dampers, they enter the right-hand side of (2.7).

## Step 7: assemble the equation of motion

Because T = Σ ½ m |v|² depends only on the velocities, and V and D depend only on positions and velocities respectively, equation (2.7) simplifies for every mass *i*:

<div class="eq"><b>m<sub>i</sub> a<sub>i</sub> = − ∂V/∂r<sub>i</sub> − ∂D/∂v<sub>i</sub> + Q<sub>i</sub></b> <span class="tag">(3.10)</span></div>

Writing out what each term is for our model, summing over all the links that touch mass *i*:

<div class="eq">m<sub>i</sub> a<sub>i</sub> = Σ<sub>links</sub> ( ± ( f<sub>spring</sub> + f<sub>damper</sub> ) u )  +  ( 0 , m<sub>i</sub> g )  +  F<sub>drive</sub>(t)  +  F<sub>mouse</sub> <span class="tag">(3.11)</span></div>

The sign is "+" if mass *i* is the *a* end of the link and "−" if it is the *b* end. This is the equation <code>accel()</code> solves, once per mass, every one-thousandth of a second.

{{fig:gl-accel-flow|The data flow inside accel(): loop 1 gathers link forces onto the nodes, loop 2 turns forces into accelerations.}}

Finally, a mass on a rail has its blocked acceleration component set to zero (Chapter 2).

## Step 8: the law of energy, derived

The program checks itself using a law of energy. Here is the proof.

Let E = T + V. Differentiate it with respect to time. The kinetic part gives Σ m **v · a**; the potential part gives Σ (∂V/∂**r**) · **v**. Substitute **m a** from (3.10):

<div class="eq">dE/dt = Σ v · ( − ∂V/∂r − ∂D/∂v + Q ) + Σ ( ∂V/∂r ) · v = − Σ v · ∂D/∂v + Σ Q · v <span class="tag">(3.12)</span></div>

The potential terms cancel. Now use a fact about functions like D that are **quadratic in the velocities**: *Euler's theorem on homogeneous functions* says Σ v · ∂D/∂v = 2 D. Therefore:

<div class="eq"><b>dE/dt = − 2 D + P<sub>in</sub></b>,    where   P<sub>in</sub> = Σ Q · v <span class="tag">(3.13)</span></div>

{{fig:gl-energy-flow|Energy flows in from drives and the mouse, swaps between kinetic and potential, and leaves as heat through the dampers. Equation (3.13) says exactly this.}}

In words: **the total energy changes only because dampers burn it (−2D) and outside forces feed it (P<sub>in</sub>).** Integrating (3.13) over time gives the law the <code>Meter</code> checks:

<div class="eq">E(t) = E(0) + ∫ P<sub>in</sub> dt − ∫ 2D dt <span class="tag">(3.14)</span></div>

::: why
**Why this is a powerful test.** (3.14) is *not* something the code was written to satisfy. It is a consequence of the physics. If the code has a wrong sign, a missing factor, or a bug in a force, the energy will not balance and the <em>energy-law error</em> on screen will grow. The error is a free, built-in bug detector.
:::

## Step 9: the exact answer for one mass, to test against

For one mass with a linear spring and a damper and no push, equation (2.9) is m ẍ + c ẋ + k x = 0. Try a solution of the form x = e<sup>rt</sup>. Substituting gives m r² + c r + k = 0, whose roots are

<div class="eq">r = − ζ ω<sub>n</sub> ± i ω<sub>d</sub> <span class="tag">(3.15)</span></div>

with the three famous numbers:

<div class="eq">ω<sub>n</sub> = √( k / m ),    ζ = c / ( 2 √( k m ) ),    ω<sub>d</sub> = ω<sub>n</sub> √( 1 − ζ² ) <span class="tag">(3.16)</span></div>

::: def
**Natural frequency (ω<sub>n</sub>).** How fast the system would oscillate with no damping, in radians per second. Divide by 2π for hertz (cycles per second).

**Damping ratio (ζ).** How strong the damping is, compared with the amount that exactly prevents any oscillation. ζ < 1 rings; ζ = 1 is *critical*; ζ > 1 is *overdamped*.

**Damped frequency (ω<sub>d</sub>).** The actual ringing speed when ζ < 1. It is a little lower than ω<sub>n</sub>.
:::

For ζ < 1, with starting position x₀ and starting speed v₀, the exact answer is

<div class="eq">x(t) = e<sup>−ζ ω<sub>n</sub> t</sup> ( x₀ cos ω<sub>d</sub> t + ( v₀ + ζ ω<sub>n</sub> x₀ ) / ω<sub>d</sub> · sin ω<sub>d</sub> t ) <span class="tag">(3.17)</span></div>

{{fig:gl-damping|The same system with four damping ratios. Critical damping (thick line) returns to rest fastest without overshoot; very light damping rings for a long time; heavy damping creeps back slowly.}}

## Step 10: a worked numerical example

Example 1 of the program is: m = 2 kg, k = 30 N/m, c = 1.5 N·s/m, with the spring stretched by 1 m and released from rest. Then:

* ω<sub>n</sub> = √(30 / 2) = √15 = **3.873 rad/s**, so a frequency of 3.873 / 2π = 0.616 Hz.
* ζ = 1.5 / (2 √(30 × 2)) = 1.5 / 15.49 = **0.0968**. Very lightly damped: it will ring many times.
* ω<sub>d</sub> = 3.873 × √(1 − 0.0968²) = **3.855 rad/s**, so the real ringing frequency is 0.6136 Hz and the period is 1.63 s.
* The envelope of the ringing falls as e<sup>−ζ ω<sub>n</sub> t</sup> = e<sup>−0.375 t</sup>. After 10 seconds it has dropped to e<sup>−3.75</sup> = 2.3 % of its start.

{{img:vl-onedof.png|Example 1 running. This is the system of the worked example: the Velocity tab shows a decaying oscillation, and the dominant frequency in the telemetry box matches the damped frequency we calculated.}}

When you run example 1 in the program and select the mass, the telemetry reports a *dominant frequency* near 0.61 Hz, which agrees with ω<sub>d</sub>.

::: try
Load example 1 (the **1** button). Select the mass, press Run, and look at the "dominant freq" line. Then select the spring and raise its stiffness k from 30 to 120. How should the frequency change? (Hint: ω<sub>n</sub> ∝ √k, so four times the stiffness gives about twice the frequency.)
:::

## Step 11: another physical effect, the static sag

In example 4 (the quarter-car), gravity pulls the masses down and the springs hold them up. The system settles where the spring force equals the weight. For a mass m hanging on one spring k, the sag is

<div class="eq">s<sub>static</sub> = m g / k <span class="tag">(3.18)</span></div>

You can check this against the program: heavier cars sag further, and the number in the telemetry box when the plot flattens matches (3.18) for a single spring.

## Chapter checks

::: quiz
1. In (3.2), what is the unit vector u, and what is its length?
2. A spring is compressed (s < 0). Which way is the force on node b? Show it from (3.6).
3. Why does a damper ignore sideways motion?
4. Why do we subtract y₀ in the gravity energy?
5. What does (3.13) tell you if a system has no dampers and no push?
6. Compute ω<sub>n</sub> and ζ for m = 1, k = 100, c = 4.
7. By what factor does the sag m g / k change if the spring is made twice as stiff?
:::

::: answers
1. u = d / ℓ, the arrow from a to b shrunk to length 1. Its length is exactly 1.
2. s < 0 gives f < 0, so the force on b is −f u = +|f| u: it points *away* from a. A compressed spring pushes its ends apart.
3. Because ṡ = u · (v<sub>b</sub> − v<sub>a</sub>) only counts the part of the velocity along the link; sideways motion does not change the length.
4. So that V starts at zero and is easy to read; subtracting a constant does not change any force.
5. dE/dt = 0: energy is conserved. (The program tests this with the spring pendulum, Chapter 14.)
6. ω<sub>n</sub> = √(100/1) = 10 rad/s; ζ = 4 / (2 √100) = 0.2.
7. It halves.
:::

# How a Computer Solves It: RK4

## The problem with exact answers

Formula (3.17) works for **one** mass with a **linear** spring. As soon as you add a second mass, a cubic spring, gravity with a swinging mass, or your mouse, nobody can write the answer as a formula. The computer must find it **step by step**, taking tiny jumps in time. This is called **numerical integration**.

## Turning second-order into first-order

Equation (3.10) has accelerations (second derivatives). Numerical methods like first derivatives, so we write the position and the velocity as two unknowns that each have a first derivative:

<div class="eq">dq/dt = v,    dv/dt = a( t, q, v ) <span class="tag">(4.1)</span></div>

Here <code>a( t, q, v )</code> is exactly what the function <code>accel()</code> computes. The pair (q, v) is called the **state**, which is why the code has a struct named <code>State</code>.

## The simplest method, and why it fails

**Euler's method** says: look at the slope now, and walk a step h in that direction.

<div class="eq">q<sub>new</sub> = q + h v,    v<sub>new</sub> = v + h a <span class="tag">(4.2)</span></div>

For a frictionless oscillator, each Euler step multiplies the "size" of the motion by √(1 + h²ω²), which is always **more than 1**. The oscillation grows with every step: energy appears from nowhere.

{{fig:gl-euler-rk4|A frictionless spring. Simple Euler's answer (dashed) swells until it explodes; RK4 (thick) lies on top of the true answer, with the same step size.}}

::: whatif
**What if the engine used Euler's method?** A frictionless spring would gain energy and explode. A damped system would look fine for small steps and wrong for large ones. Because users can set any stiffness, we cannot allow a method that blows up so easily.
:::

## RK4: four peeks, one weighted guess

The **fourth-order Runge–Kutta method** (RK4) looks at the slope four times inside one step, then combines them with weights 1, 2, 2, 1:

* **k₁**: the slope at the start of the step.
* **k₂**: the slope in the middle, using k₁ to guess where the middle is.
* **k₃**: the slope in the middle again, using k₂ for a better guess.
* **k₄**: the slope at the end, using k₃.

<div class="eq">y<sub>new</sub> = y + h · ( k₁ + 2 k₂ + 2 k₃ + k₄ ) / 6 <span class="tag">(4.3)</span></div>

{{fig:gl-rk4-stages|The four slopes inside one step, and the weighted average that RK4 actually uses (dashed).}}

The weights 1, 2, 2, 1 come from matching the Taylor series of the true solution up to the h⁴ term. The result: the error in one step shrinks like h⁵, and the error accumulated over a whole run shrinks like **h⁴**. Halve the step and the error drops sixteen times.

In our engine, <code>rk4()</code> applies this to both q and v at once. The four "slopes" are the velocities and accelerations at the four sample points; Chapter 7 goes through it line by line.

## How small should a step be? Stability

RK4 stays stable as long as <em>h × ω<sub>max</sub></em> is below about 2.8, where ω<sub>max</sub> is the *fastest* oscillation in the system. Our step is **h = 1 millisecond**. The stiffest, lightest thing the sliders allow is k = 500 N/m with m = 0.1 kg, so ω<sub>max</sub> = √(500 / 0.1) ≈ 71 rad/s, and h ω = 0.071. That is forty times below the limit, so the program is very safe, yet it is cheap: 1000 steps per second is trivial for a modern computer.

::: why
**Why a *fixed* step of exactly 1 ms?** A fixed step gives repeatable results: the same design always produces the same motion, which matters for recording and for tests. A step that changed with the frame rate would make the simulation behave differently on a slow computer.
:::

## The accumulator: real time versus physics time

The screen is redrawn roughly 60 times per second, but the gaps between redraws are uneven. The physics must still advance in steps of exactly 1 ms. The solution is a number called the **accumulator** (<code>simAcc</code> in the code).

{{fig:gl-accumulator|Each frame we add the real elapsed time (times the speed knob) to the accumulator, then take as many 1 ms physics steps as fit. The leftover waits for the next frame.}}

If a frame took 17.3 ms, we take 17 steps and keep 0.3 ms in the accumulator. Nothing is lost, and nothing is invented. A safety limit of 800 steps per frame stops a "spiral of death" if the computer stalls: a long pause would otherwise demand thousands of steps, which would make the next frame even later.

## Keeping the energy books: the trapezoid rule

The <code>Meter</code> integrates the two power terms of (3.14). It uses the **trapezoid rule**: for each step it averages the power at the start and at the end of the step, then multiplies by h. That is accurate to the second order, and it costs only two extra evaluations per step.

## Finding the dominant frequency

The telemetry shows a *dominant frequency* of the selected mass. To find it, the program tries many test frequencies f and, for each, measures how well a sine wave of that frequency matches the recorded motion. This is a **discrete Fourier transform**:

<div class="eq">X( f ) = Σ<sub>n</sub> ( x<sub>n</sub> − x̄ ) · w<sub>n</sub> · e<sup>− i 2π f t<sub>n</sub></sup> <span class="tag">(4.4)</span></div>

The test frequency that gives the largest |X| wins.

{{fig:gl-dft|A decaying 2.5 Hz wiggle (left) and its spectrum (right). The tallest peak sits at 2.5 Hz.}}

Two details improve it. First, we subtract the average x̄ so a constant offset (like sag) does not look like a "0 Hz tone". Second, we multiply by a **Hann window** w<sub>n</sub> = ½ − ½ cos(2π n / (N−1)), which fades the signal in and out so that cutting the recording at arbitrary points does not smear the peak.

## Checking the solver

We trust the numerical method only after testing it against two things nobody can argue with:

1. **The exact answer** (3.17), for one mass. The result: the largest gap over 10 seconds is **6.9 × 10⁻¹² metres**. That is a small fraction of the width of a single atom, far below anything visible.
2. **The law of energy** (3.14). For the driven two-mass system with a wiggling mouse force, the worst error over 20 seconds was **1.0 × 10⁻⁵ joules**.

Chapter 14 shows how these are automated.

## Chapter checks

::: quiz
1. Why do numerical methods turn the acceleration equation into two first-order equations?
2. In Euler's method, does the energy of a frictionless oscillator go up, down, or stay the same?
3. What are the four weights in RK4?
4. If you halve the step size, how much smaller is RK4's total error?
5. Why do we keep the leftover time in the accumulator?
6. Why is the average x̄ subtracted before the Fourier test?
:::

::: answers
1. Because the standard algorithms work on first derivatives: dq/dt = v and dv/dt = a.
2. It goes up (it blows up).
3. 1, 2, 2, 1 (divided by 6).
4. Sixteen times (error ∝ h⁴).
5. So no time is lost or invented, however uneven the frame gaps are.
6. So that a steady offset is not mistaken for a very slow wiggle.
:::
