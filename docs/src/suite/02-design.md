# Designing a System

## The tools

The **Library** on the left is a navbar. Click **Objects**, **Ground and supports**, **Connections** or **Examples** and a drop-down list opens beside it. Pick an item, then click the canvas to place it. A tool places **one** thing and then returns to Select, so things you already placed stay selectable; hold **Shift** while clicking to keep placing. **Esc** cancels.

| Menu | Tool | Key | What it makes |
|------|------|-----|---------------|
| Objects | Ball | C | A circle. You choose its radius. |
| Objects | Block | B | A rectangle. You choose its width and height. |
| Objects | Polygon | P | A regular polygon with 3 to 12 sides. |
| Objects | Wedge | W | A right-angled triangle. |
| Objects | Beam | T | A rigid beam with a length and a thickness. It **falls** unless it is supported or hinged. |
| Ground and supports | Floor line | F | A hatched line of **zero thickness** with a material (friction, bounce). Rotate it for a ramp or a wall. |
| Ground and supports | Anchor point | A | A fixed point to hang springs, dampers and rods from. |
| Ground and supports | Pin support | 1 | Pins a beam to the ground: it can turn but not move. |
| Ground and supports | Fixed support | 2 | A cantilever: the beam can neither move nor turn. |
| Ground and supports | Roller support | 3 | Holds the beam up but lets it slide along the support's axis. |
| Connections | Spring, Damper | S, D | Click one body, then another. |
| Connections | Rigid rod | R | A bar of fixed length. |
| Connections | Pin joint | J | A hinge between two bodies. It can carry a motor. |
| Connections | Fixed (weld) joint | | Glues two bodies together, angle included. |
| | Select / move | V | Click to select, drag to move. During a run, drag to push. |

{{img:suite-navbar.png|The Library navbar with the Examples drop-down open: Basic, Intermediate and Complex lists.}}

### Turning an object before you place it

While a placement tool is active, a ghost follows the cursor. Press **Q** or **E** (or hold **Ctrl** and turn the mouse wheel) to rotate it in steps before you click, or type an angle in **Rotation** in the Library. The same rotation is used for floors (ramps and walls), beams and the direction of a roller support.

{{img:suite-ghost.png|A block ghost turned 30 degrees with Q / E before it is placed. The simply supported beam below has a pin support (left) and a roller support (right).}}

Before you click, the **Settings for the next one** block lets you choose the size, radius, mass and material. Things snap to a grid (0.1 m by default; hold **Alt** to ignore it).

## Sizes and radii

Everything is measured in metres, kilograms and seconds. After placing something, select it and type exact numbers in **Properties**:

* a **ball** has a radius;
* a **block** has a width and a height;
* a **polygon** has a number of sides and a radius (the distance from its centre to a corner);
* a **wedge** has two leg lengths;
* a **beam** has a length and a thickness, and its mass comes from the material's density;
* a **floor** is a line: it has only a length and an angle.

Mass can be typed directly (kilograms) or worked out from a **density** and the shape's area. The computed mass and moment of inertia are shown underneath.

::: def
**Depth.** This is a 2-D simulator. Every body is treated as 1 metre deep, so "density x area" gives its mass. If you only ever type the mass directly, depth never matters.
:::

## Floors and their friction

A floor is a **line with hatching on the side it supports and no thickness**: it is ground, not an object, so it can never be hinged, bent or knocked over. It has a material, and therefore friction and bounce. Place one, then select it: Properties shows its length, angle and **surface material**. The material sets its friction and bounciness. Give each floor its own material to compare surfaces: for example, put an ice floor and a rubber floor side by side and slide the same block across both.

{{img:suite-incline.png|A wooden ramp tilted 28 degrees with four blocks of different materials. After six seconds, rubber and steel have not moved; wood and ice have slid down to the wall. The red dots are contact points.}}

## Beams and supports

A **beam** is a heavy rigid body. Drop it with nothing under it and it falls. To hold it up, choose a support and click the beam: the support snaps to the beam's ends, the middle of its edges and its centre.

| Support | Holds | Reaction shown |
|---------|-------|----------------|
| Pin | Position. The beam may rotate. | A force (x and y). |
| Roller | One direction only (up, for a flat roller). The beam may slide and rotate. | A force perpendicular to the roller's axis. |
| Fixed (cantilever) | Position **and** angle. | A force and a **moment**. |

While the model runs, click a support symbol and Properties shows the reaction force (and moment, for a fixed support); an orange arrow is drawn at the support. Try the examples **Simply supported beam** (the two reactions share the load) and **Cantilever with a tip load** (moment = weight x distance from the wall). {{img:suite-support.png|A running simply supported beam: the orange arrows are the two support reactions, each carrying half the weight.}}

A pin can also carry a **motor**: a constant torque, a sinusoidal torque and rotary damping, all in Properties. The **Robotic arm** example is built from these.

{{img:suite-gantry.png|The Gantry crane example: a bridge welded to two posts, a trolley on a roller and a hanging load. The orange arrows are the weld reactions.}}

{{img:suite-arm.png|The Robotic arm example: two motorised hinges and a welded payload.}}

## Springs, dampers, rods and pins

Click a connection tool, click one body, then click another. Click near a body's centre to attach to its centre; click elsewhere on it to attach at that spot (this makes it twist as well as move).

* A **spring** is born relaxed at the length you drew it. Changing its **rest length** in Properties pre-stretches or pre-compresses it. A positive **cubic stiffness** makes it harder the more it stretches.
* A **damper** resists speed: force = c x (speed of stretching). It turns motion into heat. It is the only thing that dissipates energy in a pure spring-mass system.
* A **rod** holds two points at a fixed distance.
* A **pin** makes two bodies share one point. A pendulum is an anchor, a rod and a ball.
* A **fixed (weld) joint** makes two bodies share one point and one angle: they move as one piece.

Springs change colour while the model runs: warm when they pull, cool when they push.

## Properties of a moving body

| Property | Meaning |
|----------|---------|
| Mass | Kilograms. |
| Position and angle | Where it starts. Editable in design mode only. |
| Starting velocity and spin | How it is moving at the instant you press Run. |
| Lock x / lock y | A **rail**: the body can only slide up and down, or left and right. |
| Lock rotation | The body cannot spin. |
| Collides with other bodies | Switch off for bodies that should pass through each other. |
| Harmonic push | A force F cos(w t) applied to the body: amplitude, direction and frequency. This is how you drive a system to resonance. |
| Surface material | Static friction, kinetic friction, bounciness, rolling resistance. |

## Materials

The built-in materials give typical textbook values. They are a starting point: type your own numbers for any surface.

| Material | Static friction | Kinetic friction | Bounciness (e) | Rolling resistance |
|----------|-----------------|------------------|----------------|--------------------|
| Ideal | 0 | 0 | 1.00 | 0 |
| Steel | 0.74 | 0.57 | 0.65 | 0.002 |
| Aluminium | 0.61 | 0.47 | 0.60 | 0.003 |
| Rubber | 1.00 | 0.80 | 0.80 | 0.020 |
| Wood | 0.50 | 0.35 | 0.50 | 0.010 |
| Concrete | 0.70 | 0.60 | 0.20 | 0.015 |
| Glass | 0.90 | 0.40 | 0.70 | 0.003 |
| Ice | 0.10 | 0.03 | 0.30 | 0.001 |
| Teflon | 0.04 | 0.04 | 0.50 | 0.002 |
| Clay | 0.60 | 0.50 | 0.05 | 0.030 |

When two bodies touch, their friction is combined from both surfaces (by default, the geometric mean: the square root of the product). Bounciness takes the larger of the two. You can change both rules under **Environment, Advanced**.

::: note
**Static versus kinetic friction.** *Static* friction decides whether a resting body starts to slide: it slides only when the push along the surface exceeds the static friction times the pressing force. Once sliding, the lower *kinetic* friction applies. That is why a block on a ramp can stay put at one angle and run away at a slightly steeper one.
:::

## Undo, save, open

**Ctrl+Z** and **Ctrl+Y** undo and redo every edit (a slider drag counts as one step). **Ctrl+S** saves the design as a <code>.vlab</code> file; **Ctrl+O** opens one. The program also remembers your last design automatically.

# Ideal and Realistic Physics

The **Environment** panel has two big buttons and a list of switches.

| Switch | What it controls |
|--------|------------------|
| **Ideal** | One press turns friction, inelastic impacts, dampers, rolling resistance and drag all off. Nothing can lose energy. |
| **Realistic** | Turns friction, inelastic impacts, dampers and rolling resistance on, using the materials. |
| Collisions between bodies | Off: bodies pass through each other. |
| Friction | Off: every surface is slippery (friction = 0). |
| Inelastic impacts | Off: every collision is perfectly elastic (e = 1). |
| Rolling resistance | Off: a ball rolls forever on a flat floor. |
| Dampers active | Off: dampers are ignored. |
| Air drag | A force against velocity: F = -(c1 + c2 &#124;v&#124;) v. You choose c1 and c2. |
| Gravity | Pull toward the bottom of the screen. Presets for Earth, Moon and zero gravity. |

You can mix them: for example, friction on but bouncing perfectly elastic, to isolate the effect of friction alone.

## What changes when you switch

::: try
Load **Bouncing ball: ideal vs realistic**. Run it in Realistic mode: each bounce is lower than the last and the ball settles on the floor. Now press **Ideal** and run again: the ball returns to its starting height every time, forever. Open **Telemetry, Energy ledger** to see the difference in where the energy goes.
:::

::: try
Load **Friction on an incline: four materials** and run it. Rubber and steel stay put; wood and ice slide. Now press **Ideal** (no friction) and run again: all four slide down together with the same acceleration, g sin(angle).
:::

## The energy ledger

Energy is never created or destroyed; it only changes form. The program keeps a running account and shows it in the **Energy ledger** tab:

{{img:suite-energy.png|The energy ledger of a tuned-mass-damper run: kinetic, potential and total energy (top left), where the energy went (top right), the cumulative ledger (bottom left) and the energy law with its numerical check (bottom right).}}

* **Work in**: energy fed in by pushes and by you dragging with the mouse.
* **Dampers**: heat from dampers.
* **Air drag**: heat from drag.
* **Friction**: heat from sliding and from rolling resistance.
* **Impacts**: kinetic energy lost in collisions.

The law of energy says **E(end) = E(start) + work in - energy lost**. The tab prints both sides and the difference, called the **numerical error**. That number is the program checking itself: it should be tiny (a few thousandths of a joule at most). If it ever grows, something is wrong, and that is useful to know.

::: why
**Why does this matter?** Many simulators just show motion; you cannot tell if the motion is right. A ledger that balances is strong evidence that the forces and the time stepping are correct, and it teaches the central idea of mechanics: the energy goes somewhere.
:::
