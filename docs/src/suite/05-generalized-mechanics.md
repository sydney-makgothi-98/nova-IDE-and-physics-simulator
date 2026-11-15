# Creating Generalized Equations and Code for Any System

Every other chapter of this guide tells you what the program does. This chapter tells you **how it was made to work for any system you can draw**: a mass on a rail, a pendulum, a quarter-car, a robotic arm, a block on a bed of springs, a beam with a torsional spring at its hinge, a truck on a bridge. It explains the mathematics first, then walks through the real source code function by function, and at each step says **why** the choice was made, **why not** the obvious alternatives, and **what the alternatives would have cost**.

The listings in this chapter are copied from the source files automatically when the guide is built, so they always match the program. Line numbers are the real line numbers of the file at that moment.

::: note
**Who this chapter is for.** You do not need to know the physics to follow the *ideas*, but the code listings assume you can read C++. If you want to add a new kind of element (a new spring, a new joint, a new load), the last two sections give a recipe you can follow step by step, and the next chapter explains how to build and publish the result.
:::

# The Problem: One Engine, Infinitely Many Systems

A textbook solves one system at a time. For a mass on a spring you write one equation, m x'' + c x' + k x = F(t). For a double pendulum you derive a different, much longer pair of equations. For a quarter-car you derive a third. Each derivation is correct and each is useless for the next system. A **general** simulator cannot work that way, because the user decides the system *after* the program is written.

So the design question was:

::: def
**The generalization question.** What single description of a mechanical system is (1) rich enough to express every system the user can draw, (2) simple enough that one piece of code can advance any of them in time, and (3) honest enough that the answer can be checked against the physics we already know?
:::

The answer the program uses has three layers, each of which you will meet below.

| Layer | What it contains | Where it lives |
|-------|------------------|----------------|
| **Description** | Bodies (shape, mass, material, starting state) and Links (springs, dampers, rods, hinges, welds, rollers) with their attachment points | <code>core/include/vlab/model.hpp</code>, <code>core/src/model.cpp</code> |
| **Equations** | Newton's second law for every body in the same coordinates, with the forces written once, per *kind* of element, not per system | <code>core/src/world.cpp</code>: <code>computeForces</code>, <code>potentialEnergy</code> |
| **Constraints and contact** | Everything that is not a smooth force: hinges, welds, rails, collisions, friction | <code>core/src/world.cpp</code>: the impulse solver |

The rest of the chapter is these three layers, in that order, followed by time stepping, energy accounting, testing, and a recipe for adding to them.

# Choosing the Coordinates

## The choice: three numbers per body, in world axes, always

Every body in the program, whatever it is, is described by the same three numbers (**x, y, θ**) and their rates (**ẋ, ẏ, θ̇**): position of its centre of mass in the world, and the angle it has turned. That is all the engine ever stores about where a body is.

The structure that holds them is, in <code>world.hpp</code>:

```cpp
struct BodyState {
    Vec2 pos;
    double angle = 0;
    Vec2 vel;
    double omega = 0;
    Rot rot;                       // cached cos/sin of angle
};
```

* **<code>pos</code>** is the centre of mass in metres. The y axis points **down**, exactly like the screen, so gravity is a positive y force and a drawing needs no flipping.
* **<code>angle</code>** is in radians. Because y points down, a *positive* angle turns the body **clockwise on the screen**. This single convention decides the sign of every moment, torque and torsional spring in the program.
* **<code>vel</code>, <code>omega</code>** are the rates, in m/s and rad/s.
* **<code>rot</code>** is a cache of cos θ and sin θ, so the sine and cosine of an angle are computed once per body per step and not once per use. A step may rotate the same body's attachment points hundreds of times (a spring distribution has up to 40 elements).

## Why not "generalized coordinates"?

The classical way to analyse a pendulum is to choose the **one** number that describes it, the angle, and to derive the equation of motion for that number alone. This is the *minimal-coordinate* method, and it is the heart of Lagrangian textbooks. It gives the smallest possible set of equations, with no constraint forces to worry about.

The program deliberately does **not** do that, for four reasons.

1. **The user chooses the system at run time.** A minimal coordinate set depends on how the bodies are connected. Pin two bodies together and you have removed two coordinates. Remove the pin and they come back. The program would have to re-derive its equations every time you clicked. Maximal coordinates (three per body, always) do not change when you add or remove a connection.
2. **Contacts break and form.** A ball resting on a floor has two coordinates fewer than a ball in the air. A minimal-coordinate engine would need to change its coordinate set mid-run. With maximal coordinates the ball just *has* the same three numbers, and the floor contributes a force when they touch.
3. **Loops are easy.** A crank, a connecting rod and a piston form a closed loop (the single-cylinder engine example). Minimal coordinates for a loop need a loop-closure equation that is hard to solve generally. Maximal coordinates plus a pin constraint handle any loop identically.
4. **One code path.** The force on a body is the same few lines for every body.

::: why
**What it costs.** Maximal coordinates mean more numbers than the system really has, and the constraints (a pin says "these two points must coincide") are *extra* equations that must be enforced. Section "Constraints" below is the price of this choice. It is the standard trade that every general-purpose physics engine makes, because the alternative cannot be general.
:::

## Local frames: how orientation stops mattering

The second decision is where things attach. A spring does not attach to "the right side of the block at x = 1.5"; it attaches to a point **in the body's own frame**: <code>la</code> for the first body and <code>lb</code> for the second. The world position of the attachment is then computed fresh every step:

<div class="eq">p<sub>world</sub> = p<sub>body</sub> + R(θ) · l<sub>local</sub> <span class="tag">(5.1)</span></div>

where R(θ) is the 2 × 2 rotation matrix. In code this is <code>pos + rot.apply(local)</code>, and the inverse (world to local) is <code>rot.applyInv(world - pos)</code>. Both live in <code>math.hpp</code>:

@@src core/include/vlab/math.hpp | ^inline double dot | +8

@@src core/include/vlab/math.hpp | ^struct Rot | ^\};

* **<code>dot(a, b)</code>** is a·b = a<sub>x</sub>b<sub>x</sub> + a<sub>y</sub>b<sub>y</sub>: the projection of one vector on another. It appears whenever a force direction must be extracted from a velocity or a displacement.
* **<code>cross(a, b)</code>** returns the *scalar* z component of the 3-D cross product a × b = a<sub>x</sub>b<sub>y</sub> − a<sub>y</sub>b<sub>x</sub>. In 2-D this one number is all there is of a torque or of an angular momentum, and it is positive for a **clockwise** turn on the screen because y points down.
* **<code>cross(s, v)</code>** and **<code>cross(v, s)</code>** are the two mixed forms (scalar angular velocity times a vector, and the reverse). The first, <code>cross(omega, r)</code>, is the velocity of a point at lever arm r on a spinning body: **v<sub>point</sub> = v + ω × r**. It is used for every contact and every damper.
* **<code>Rot</code>** stores cos θ and sin θ. <code>apply</code> rotates a vector by θ; <code>applyInv</code> rotates it by −θ. Storing them avoids calling <code>std::cos</code> and <code>std::sin</code> (slow) in the innermost loops.

Because attachment points are in the **body's frame**, rotating a body, tilting a floor to 28°, or drawing the whole system upside down changes nothing in the equations. The attachment moves with the body automatically. This is the entire mechanism by which "any orientation" works: orientation lives in the *state* (<code>angle</code>), never in the *formulas*.

::: why
**Why not store world coordinates for attachments?** Because a rotating body would drag them out of place and every step would have to update them by hand. Local frames make the geometry **invariant**: the stored number never changes while the body moves.
:::

# Describing a System: Bodies, Links and the Model

The description layer is plain data. A <code>Model</code> is two lists, one of <code>Body</code> and one of <code>Link</code>, plus the environment (gravity, friction switches, time step) and the run settings. There is no behaviour in it. That is on purpose: the same Model can be saved to a file, shown in the interface, run by the engine, exported to Excel, or handed to the Unreal Engine plugin.

## The Link: one structure for six kinds of connection

All connections are one structure, distinguished by a <code>kind</code> field. The full definition today, including the distribution and torsion fields added in version 1.2:

@@src core/include/vlab/model.hpp | ^struct Link | ^\};

Read it field by field:

* **<code>kind</code>**: Spring, Damper, Rod, Pin, Weld or Roller. Springs and dampers are *smooth* forces; the other four are *constraints*. This split is the most important idea in the engine and Section "Two kinds of connection" returns to it.
* **<code>a</code>, <code>b</code>**: indices of the two bodies. A link always joins exactly two bodies. A wall or the ground is also a body (a *Static* or *Anchor* kind), so "spring to the ground" is not a special case.
* **<code>la</code>, <code>lb</code>**: the attachment points in each body's local frame (equation 5.1).
* **<code>k</code>, <code>k3</code>**: linear and cubic stiffness. The force is k s + k<sub>3</sub> s³ where s is the stretch.
* **<code>L0</code>** and **<code>autoRest</code>**: the rest length, and a flag saying "keep the rest length equal to the length as drawn". Section "Rest lengths" explains why.
* **<code>c</code>**: damping coefficient.
* **<code>refAngle</code>**: the relative angle a Weld holds, and (since 1.2) the rest angle of a torsional spring.
* **<code>jointC</code>, <code>motorTorque</code>, <code>motorAmp</code>, <code>motorW</code>**: a Pin's rotary damping and motor.
* **<code>kt</code>** (1.2): the torsional stiffness of a hinge.
* **<code>dist</code>, <code>la2</code>, <code>lb2</code>, <code>distM</code>** (1.2): a *distribution*. If <code>dist</code> is 0 the link is one element between <code>la</code> and <code>lb</code>. If it is 1 or 2 the link is a *row* of elements spread from (<code>la</code>, <code>lb</code>) to (<code>la2</code>, <code>lb2</code>), with constant or linearly varying strength.

::: why
**Why one structure and not a class hierarchy?** A <code>Spring</code> class, a <code>Damper</code> class and so on is the textbook object-oriented answer. It was rejected because (a) every operation (save, load, draw, edit, export) would need a virtual function per class, (b) adding a feature to *several* kinds at once (a distribution applies to both springs and dampers) would cross class boundaries, and (c) a plain struct with a tag is trivially copied, which the undo stack, the running world and the file saver all rely on. The cost is a few fields that mean nothing for some kinds. The code accepts that cost to keep the whole model one copyable value.
:::

## Mass properties: where m and I come from

A body's mass is either typed in or computed from density and area, and its moment of inertia about its centre of mass comes from the shape. <code>Model::updateMass</code> computes <code>massKg</code>, <code>invMass</code>, <code>inertia</code> and <code>invI</code> for every body. Two details matter for generality:

* **Inverse mass, not mass.** The solver stores 1/m and 1/I. A fixed body (a floor, an anchor) has **inverse mass zero**, which makes it immovable *without any special case*: an impulse P changes velocity by P/m = P · invMass = 0.
* **Locks.** A body on a rail (<code>lockY</code>, for instance) is not given a different equation. Its inverse mass **along the locked axis** is set to zero by the helper below, so no impulse the solver computes can ever move it that way:

@@src core/src/world.cpp | ^static inline Vec2 invMv | ^\}

<code>invMv</code> returns the inverse mass per axis. <code>kDir</code> returns the **effective inverse mass of a body along a direction d at lever arm r**, which is the key number in every constraint:

<div class="eq">k<sub>d</sub> = d<sub>x</sub>² / m<sub>x</sub> + d<sub>y</sub>² / m<sub>y</sub> + (r × d)² / I <span class="tag">(5.2)</span></div>

It answers the question "if I push this body along d at the point r, how much does that point accelerate per newton?" A heavy body or one with a long lever arm and a big inertia answers with a small number. The factor (r × d)² / I is the contribution of rotation: pushing off-centre spins the body, and the spin moves the point too.

# The Equations of Motion for Any Body

## Newton in maximal coordinates

For every dynamic body i the engine solves

<div class="eq">m<sub>i</sub> a<sub>i</sub> = F<sub>i</sub>      I<sub>i</sub> α<sub>i</sub> = τ<sub>i</sub> <span class="tag">(5.3)</span></div>

where F<sub>i</sub> is the **total force** at the centre of mass and τ<sub>i</sub> the **total moment** about it. Everything the user can add is a *contribution* to F<sub>i</sub> or τ<sub>i</sub>: gravity, a spring, a damper, a push, a moment, a distributed load, the mouse. The function that adds them up is <code>World::computeForces</code>. This is where generality is won: **each kind of element is written once**, and it works between any two bodies, at any position, at any angle.

The Lagrangian derivation of the same equations (chapters 2 and 3 of *Inside the Vibration Lab*) gives, for a set of coordinates q,

<div class="eq">M q̈ = − ∂V/∂q − ∂D/∂q̇ + Q <span class="tag">(5.4)</span></div>

with V the potential energy (springs and gravity), D the Rayleigh dissipation function (dampers) and Q the generalized forces of loads. Equation 5.3 and equation 5.4 are the same law. 5.4 tells us *where each force comes from* (the gradient of an energy), and that is how every new element was derived: **write the energy, differentiate it with respect to the coordinates, and the force and the moment fall out**.

## Gravity and the potential energy

Gravity is the simplest contribution, and it is written in <code>computeForces</code> as one line per body. The potential energy of gravity with y pointing down is V = −m g (y − y<sub>0</sub>) (lower means less energy), so the force is −∂V/∂y = +m g, a positive y force. The reference y<sub>0</sub> is the body's drawn position, so the energy starts at zero and the ledger (below) starts clean.

## The force function, line by line

Here is the part of <code>computeForces</code> that handles springs and dampers, including distributions. Read it as three nested ideas: *for each link*, *for each element of the link*, *compute one force and apply it to both bodies*.

@@src core/src/world.cpp | if \(l.kind != LinkKind::Spring && l.kind != LinkKind::Damper\) continue; | ^    \}\s*$

Go through it slowly.

* **<code>if (l.kind != Spring && l.kind != Damper) continue;</code>** Only the *smooth* links are handled here. Rods, pins, welds and rollers are constraints and are handled by the impulse solver. The test is first so that most links cost almost nothing.
* **<code>const BodyState &A = S[l.a], &B = S[l.b];</code>** The *state array* <code>S</code> is passed in, not read from the world, because the RK4 integrator evaluates the forces at four *trial* states. The same function therefore serves the real state and the trial states.
* **<code>const int N = l.distN();</code>** The number of elements: 1 for an ordinary link, 4 to 40 for a distribution (one element per 0.15 m of span, with a floor of 4 and a ceiling of 40).
* **<code>for (int q = 0; q < N; ++q)</code>** Every element does the same thing.
* **<code>pla = l.la + (l.la2 - l.la) * fr;</code>** The attachment point of element q on the first body: a linear interpolation along the span. <code>fr = (q + 0.5)/N</code> is the **midpoint** of the q-th slice. The same is done for the second body, so each element runs between the two matching points.
* **<code>ke = max(0, k + add) * span / N;</code>** The stiffness of this element. If the link stores k newtons per metre **per metre of span**, then a slice of width span/N has stiffness k × span/N. For a linear distribution, <code>add = distM * sd</code> where <code>sd</code> is the distance along the span, so the value per metre grows as y = m x. The <code>max(0, ...)</code> stops a negative slope from creating a negative stiffness (which would be a *pushing* spring that grows with stretch: unstable).
* **<code>ra = A.rot.apply(pla)</code>** The lever arm: the vector from the body's centre to its attachment point, in world axes.
* **<code>d = (B.pos + rb) - (A.pos + ra)</code>** The vector between the two attachment points in the world. <code>len = length(d)</code> is the spring's current length and <code>u = d / len</code> its direction. The guard <code>len > 1e-9</code> prevents division by zero when the two points coincide; the direction is then arbitrary (1, 0), and the force is zero anyway because the stretch is far from the rest length.
* **<code>s = len - l.L0</code>** The stretch. The force, <code>f = ke * s + l.k3 * s^3</code>, is dV/ds for V = ½ k s² + ¼ k<sub>3</sub> s⁴.
* **Damper branch.** The relative velocity of the two attachment points is <code>va = A.vel + cross(A.omega, ra)</code>, <code>vb = B.vel + cross(B.omega, rb)</code> (equation "v + ω × r"). The rate of change of the length is <code>rate = dot(u, vb - va)</code>, the projection of the relative velocity on the spring direction. The force is <code>ce * rate</code>, and the heat power, <code>ce * rate * rate</code>, is accumulated in <code>pv</code> for the energy ledger.
* **Applying it.** <code>Fa = u * f</code> is the force on body a (a stretched spring pulls a toward b). The reaction on b is <code>-Fa</code>. The moment on each body is the cross product of its lever arm with the force it feels: <code>tau[l.a] += cross(ra, Fa)</code>, <code>tau[l.b] += cross(rb, -Fa)</code>. A fixed body is skipped by <code>isDynamic()</code>.

::: mapx
| Maths | Code |
|-------|------|
| d = (p<sub>b</sub> + R<sub>b</sub> l<sub>b</sub>) − (p<sub>a</sub> + R<sub>a</sub> l<sub>a</sub>) | <code>d = (B.pos + rb) - (A.pos + ra)</code> |
| s = &#124;d&#124; − L<sub>0</sub> | <code>s = len - l.L0</code> |
| f = k s + k<sub>3</sub> s³ | <code>f = ke * s + l.k3 * s * s * s</code> |
| u = d / &#124;d&#124; | <code>u = d * (1.0 / len)</code> |
| ṡ = u · (v<sub>b</sub> − v<sub>a</sub>) | <code>rate = dot(u, vb - va)</code> |
| F<sub>a</sub> = f u,  F<sub>b</sub> = −f u | <code>Fa = u * f</code>, then <code>-Fa</code> |
| τ = r × F | <code>tau[l.a] += cross(ra, Fa)</code> |
:::

### Why the force is correct for any orientation: the derivation

Take a spring between point P<sub>a</sub> = p<sub>a</sub> + R(θ<sub>a</sub>) l<sub>a</sub> and P<sub>b</sub> = p<sub>b</sub> + R(θ<sub>b</sub>) l<sub>b</sub>. Its length is s + L<sub>0</sub> = |P<sub>b</sub> − P<sub>a</sub>|. The energy is V(s). The generalized force on any coordinate q is −(dV/ds)(∂s/∂q). The derivatives are:

<div class="eq">∂ℓ/∂p<sub>b</sub> = u,  ∂ℓ/∂p<sub>a</sub> = −u,  ∂ℓ/∂θ<sub>b</sub> = u · (R′ l<sub>b</sub>) = r<sub>b</sub> × u <span class="tag">(5.5)</span></div>

where r<sub>b</sub> = R l<sub>b</sub> is the lever arm and the last equality is the identity (R′ l) · u = r × u in 2-D. So the force on body b is −f u at its centre of mass and the moment is −f (r<sub>b</sub> × u) = r<sub>b</sub> × (−f u): the force acting at the attachment point. **That is the entire derivation of the code above.** Nothing in it depends on the angle of the spring, of either body, or of the world: only on the vectors r and u that the program computes fresh each step. This is why a spring that is vertical, horizontal, diagonal, or rotating with a tumbling body needs no special handling.

::: why
**Why not write the spring as forces along x and y and be done?** Because the moment would then be forgotten. A spring attached off-centre to a block makes the block *rotate*. A simulator that only moves the centre of mass gets that wrong. The lever-arm form includes the rotation for free.
:::

## Rest lengths: why "autoRest" exists

A spring's rest length L<sub>0</sub> is the length at which it exerts no force. The user, however, *draws* the spring between two points, and usually means "this is relaxed as drawn". The program records <code>L0</code> as the drawn length when the link is created and keeps updating it while the user edits the drawing (<code>Model::refreshAutoRest</code>), until the user types a different rest length (which sets <code>autoRest = false</code>: a **preloaded** spring). Distributions set <code>autoRest = false</code> and an explicit <code>L0</code> equal to the gap between the faces.

The alternative, asking the user for the rest length of every spring, was rejected: the drawing is the specification, and a model that starts at rest should not have to be told so.

## The potential energy function

<code>potentialEnergy</code> sums the same elements as the force function, but as energy, because the **energy ledger** needs to know V at every step:

@@src core/src/world.cpp | ^double World::potentialEnergy | ^\}

Notice that the loop over elements is **identical** in shape to the one in <code>computeForces</code>. This duplication is deliberate. The force must equal the negative gradient of the energy, and keeping the two loops side by side makes a mistake easy to see. If the stiffness weights in the force loop and the energy loop ever disagreed, the energy ledger would drift, and the validation test for distributions (below) would fail.

# Generalizing the Spring: Distributions

A single spring cannot represent a bed of springs: a floor on elastic mounts, a machine on a row of isolators, a beam on an elastic foundation (a *Winkler* foundation). The user wanted to say "spread this much stiffness along that face, either evenly or growing along it". That is the distribution.

## The mathematics: an integral turned into a sum

A distributed stiffness k(x) per metre along a span of length ℓ gives a total stiffness (for a pure translation of the object) and a stiffness-weighted centre:

<div class="eq">K = ∫<sub>0</sub><sup>ℓ</sup> k(x) dx,    k(x) = k<sub>0</sub> + m x <span class="tag">(5.6)</span></div>

For a constant distribution K = k<sub>0</sub> ℓ. For the linear distribution that starts from zero (y = m x), K = m ℓ² / 2. The code replaces the integral with the **midpoint rule**:

<div class="eq">K ≈ Σ<sub>q=0</sub><sup>N−1</sup> k(x<sub>q</sub>) (ℓ/N),    x<sub>q</sub> = (q + ½) ℓ / N <span class="tag">(5.7)</span></div>

and gives each of the N slices its own ordinary spring of stiffness k(x<sub>q</sub>) ℓ/N, placed at x<sub>q</sub>.

::: why
**Why the midpoint rule?** For a function that is constant or straight (the two cases offered) the midpoint rule is **exact**, not an approximation. Σ (q + ½)/N × (1/N) = ½ for every N, so a linear distribution is reproduced to rounding error. The validation test in <code>core_tests.cpp</code> checks exactly this: 100 N/m total stiffness from a constant distribution and from a linear one, both vibrating a 4 kg block at √(K/m) = 5 rad/s to within 0.00000 m over three seconds.
:::

## Alternatives that were considered

| Alternative | Why it was not chosen |
|-------------|-----------------------|
| **One equivalent spring** at the stiffness-weighted centroid | Cannot resist rotation correctly: a block on a row of springs has a *rotational* stiffness ∫ k(x)(x − x<sub>c</sub>)² dx that one spring does not give. The rows of elements give it automatically through the lever arms. |
| **Gaussian quadrature** (a few cleverly placed points) | Exact for smoother functions than the midpoint rule, but the two shapes offered (constant, linear) are already exact with the midpoint, and equal slices are easier to draw and to explain. If a curved distribution were added, Gauss points would be the upgrade. |
| **A continuum finite-element foundation** | Needs a mesh of the contact face and a matrix assembly; the engine has no such machinery for contact faces. N independent springs reach the same answer for rigid faces. |
| **Fixed N (say 10)** | Would give a long face the same resolution as a short one. N = ⌈ℓ / 0.15⌉ (clamped to 4 to 40) keeps the spacing about 15 cm. |

## Where the span comes from: the geometry of "ground and face"

The user only supplies **two things**: the grounded surface and the object face. Everything else is computed by <code>App::makeDistribution</code>. It is geometry, so it is worth reading in full:

@@src app/ui_canvas.cpp | ^void App::makeDistribution | ^\}

* **Which is the ground?** The fixed body of the pair is the ground (<code>!isDynamic()</code>); if neither is fixed the first one clicked is. This is why the *click order does not matter* when one of the two is fixed.
* **The normal n.** For a floor *line* the normal is the line's own perpendicular, flipped so that it points toward the object (<code>dot(n, po - pg) &lt; 0</code>). For any other ground body it is the unit vector from the ground's centre to the object's centre.
* **The tangent t = (−n<sub>y</sub>, n<sub>x</sub>)**, the direction along the face. The span lives along t.
* **<code>verts</code>** gathers the world-space corner points of a body (a circle gives its four extreme points, an anchor a short segment). From them the code takes the object's extent along t (<code>o0</code>, <code>o1</code>), the position of its face along n (<code>dObj</code> = the smallest n-coordinate of any corner), and the ground's surface <code>dG</code> and extent (<code>g0</code>, <code>g1</code>).
* **The span** is the *overlap* of the two extents, [max(o0, g0), min(o1, g1)]: the part of the object that actually sits over the ground. If there is no overlap it falls back to the object's own face, and if that is tiny it widens to half a metre either side.
* **Perpendicular** springs run from the ground surface straight to the face, so both ends use the same sideways position (<code>gs0 = s0</code>). Their rest length is the gap, <code>dObj − dG</code>.
* **Angled** springs run along a direction u chosen by the user (0° is right, 90° is down). If u makes angle φ with the normal, a spring crossing a gap g has length g / cos φ, and it meets the ground sideways by g tan φ. In the code, <code>c = dot(n, u)</code> is cos φ, <code>Lr = gap / c</code> is the length, and <code>shift = −Lr * dot(t, u)</code> moves the ground end. A direction nearly parallel to the face (<code>c &lt; 0.05</code>) is ignored, because the springs would be almost infinitely long.
* **Local coordinates.** Finally, <code>loc</code> converts every world point back into each body's local frame, because a link stores local points (equation 5.1).

::: why
**Why a function in the interface file and not in the engine?** The engine only knows links with local points; it does not know that a user *clicked* on faces. Translating a click into the stored description is interface work, so it lives in the interface (<code>app/ui_canvas.cpp</code>). The engine stays free of any idea of the mouse, which is what allows the validation tests to build the same links by hand and run without a window.
:::

# Generalizing the Hinge: The Torsional Spring

A hinge (Pin) holds two points together and lets the bodies turn. A **torsional spring** adds a restoring moment about the hinge: it resists the *relative angle* θ<sub>b</sub> − θ<sub>a</sub> from a rest angle θ<sub>0</sub>.

<div class="eq">V<sub>t</sub> = ½ k<sub>t</sub> (θ<sub>b</sub> − θ<sub>a</sub> − θ<sub>0</sub>)²,     τ<sub>b</sub> = − ∂V<sub>t</sub>/∂θ<sub>b</sub> = − k<sub>t</sub> (θ<sub>b</sub> − θ<sub>a</sub> − θ<sub>0</sub>),     τ<sub>a</sub> = −τ<sub>b</sub> <span class="tag">(5.8)</span></div>

It is two lines in <code>computeForces</code>, in the hinge loop that already handles motors and rotary damping:

```cpp
if (l.kt != 0) Tc -= l.kt * (B.angle - A.angle - l.refAngle);   // torsional spring: restoring torque on B
...
if (db) tau[l.b] += T + Tc;
if (da) tau[l.a] -= T + Tc;
```

* **Why a field on Pin, not a new kind of link?** A torsional spring *needs* a hinge: it only makes sense where two bodies are pinned. Making it a property of the Pin (a number <code>kt</code>, zero meaning a free hinge) means every place that already knows how to handle a hinge (the solver, the drawing, the file, the reaction readout) handles a torsional hinge for free. The "Torsional spring" tool in the Connections menu simply creates a Pin and sets <code>kt</code>.
* **Why reuse <code>refAngle</code>?** The Weld already stores "the relative angle as drawn". The torsional spring wants exactly the same thing: it should rest at the angle the user drew. <code>Model::refreshAutoRest</code> updates <code>refAngle</code> for a Weld, and, since 1.2, for a Pin whose <code>kt</code> is not zero.
* **Both bodies.** The moment is applied to B and the opposite moment to A, so the pair together exerts **no net moment** on the system (Newton's third law for moments). If A is the ground, its share is simply discarded.
* **Sign.** Positive relative angle turns B clockwise on the screen, so a positive stretch gives a negative (counter-clockwise) moment: the spring pulls it back.

The energy goes into <code>potentialEnergy</code> as the last loop (V<sub>t</sub> = ½ k<sub>t</sub> θ², summed over hinges), which keeps the energy ledger balanced. The validation test is the textbook one: a 3 kg beam pinned at one end, with I about the pin = I<sub>cm</sub> + m d² = 4.01 kg·m², released from 0.1 rad with k<sub>t</sub> = 40 N·m/rad, must cross zero after one **quarter** of the period π/2 · √(I/k<sub>t</sub>) = 0.4973 s. The program measures 0.4972 s.

# Generalizing the Loads: Moments and F(t)

A load is a force or moment that does not come from another body. The program has gone through three generations of loads, each a small generalization of the previous one.

1. **A constant push or pull** along a direction: <code>loadN</code> newtons, at <code>loadAngleDeg</code>, reversed if <code>loadPull</code>.
2. **A harmonic push**, F cos ωt (the Drive): used for resonance studies.
3. **A moment and an expression of time** (1.2): <code>momentN</code> and the formulas <code>loadExpr</code> and <code>momentExpr</code> evaluated at the current time.

All of them are added in <code>World::externalForce</code> and in the body loop of <code>computeForces</code>. The key to the third generation is that **the magnitude can be any function of time**, so the program needs a way to read a formula the user typed.

## The expression evaluator

The formula "3*t^3 + 2*t^2 + 0*t + 4" or "100*sin(2*pi*t)" is **text** typed at run time. The program cannot compile it into the C++ program, so it must *interpret* it. <code>core/src/expr.cpp</code> does this in two steps.

1. **Parsing.** A recursive-descent parser turns the text into a tree of nodes stored in a flat array (<code>std::vector&lt;Node&gt; nodes_</code>). Each node is a number, a variable (x, L or t), a unary minus, a binary operator, or a function call. Precedence is encoded in the order of the parsing functions: sums call products, products call powers, powers call unary minus and atoms. The rule "^ binds tighter than unary minus" (so −x² means −(x²)) is what the textbook convention requires and what a user types.
2. **Evaluation.** <code>eval(x, L, t)</code> walks the tree: one pass per call, no allocation. It is fast enough to be evaluated at every one of the 4000 steps a second the engine takes, for every body.

The parse is done **once**, when the world is built or when the user edits a formula (<code>World::rebuildExprs</code>); the interface caches parsed formulas by their text (<code>Expr::cached</code>) because it asks about the same formula every frame to draw the preview value.

::: why
**Why write a parser and not use a scripting language?** A full language (Lua, Python, JavaScript) would let the user write anything, but (a) it brings a large dependency into a small, self-contained C++ program; (b) it can loop forever or crash; (c) it makes the validation suite harder to trust because behaviour could not be bounded. A formula language with only numbers, a few functions and no statements **cannot hang** and **cannot do anything but compute a number from t**. The cost is that the user cannot write a loop or an if. The functions <code>step</code>, <code>sign</code>, <code>min</code>, <code>max</code> and <code>floor</code> cover most of what people actually need (pulses, switches, saw-tooth waves).
:::

## Applying a time-varying load

In <code>externalForce</code> the constant magnitude is replaced by the evaluated formula if the user has chosen "varies with time":

@@src core/src/world.cpp | ^Vec2 World::externalForce | ^\}

* **<code>mag = b.loadN</code>** by default; **<code>mag = lexpr_[i]->eval(0, 1, t)</code>** if <code>loadTimeVar</code> is set. The first two arguments, x and L, are not meaningful for a load on a whole body, so they are passed as 0 and 1.
* **Direction.** The force always acts along <code>loadAngleDeg</code>, reversed for a pull. A *negative* magnitude from the formula (a sine goes negative) therefore reverses the force at that instant, as a user expects.
* **A formula that fails to parse** leaves the pointer null and the magnitude zero: the body feels no force, and the interface shows the error message below the text box instead.
* **The moment** is the same idea in the body loop of <code>computeForces</code>: <code>M = momentN</code> or <code>mexpr_[i]->eval(0, 1, t)</code>, added to <code>tau[i]</code>, and its power M ω is added to the work-in integral. A body with locked rotation ignores it.

**Showing the load on the object.** A load the user cannot see is a load the user forgets. In the drawing code (<code>app/ui_canvas.cpp</code>) every load draws an arrow from the body's centre and a label beside it. A constant push or pull is labelled with its value (push 50 N). A time-varying one is labelled with its **formula** (push F(t) = 0 + 100*sin(6.2832*t + 0*pi/180)) and, while the simulation runs, the **value at this instant** (now 37.2 N), and its arrow grows, shrinks and **reverses** with the formula as the sign changes. A moment is a curved arrow round the centre, labelled in newton-metres and clockwise or counter-clockwise, and a time-varying moment shows M(t) and its current value in the same way. The label reads the same cached formula the engine uses, so what is drawn can never disagree with what is simulated.

**Power and the ledger.** Every load does work, and the ledger needs it: P = F · v for a push and P = M ω for a moment. These are accumulated into <code>pin</code> and integrated by the trapezoid rule in <code>World::step</code>. That is why a time-varying load does not upset the energy balance: its work is counted at exactly the moment it is done.

# Two Kinds of Connection: Smooth Forces and Constraints

So far every contribution has been a *force*: a spring pulls with a force that depends smoothly on positions and velocities. The equations of motion 5.3 can be integrated directly (the RK4 method below). A **hinge** is different. It does not exert a force that depends on position in any smooth way: it says "these two points shall be at the same place, always". Such a rule is a **constraint**.

## Why constraints are hard

The obvious way to enforce a constraint is to treat it as an extremely stiff spring: if two points drift apart by d, apply a force 10⁹ · d to pull them back. This is the **penalty method**. It is easy to write and wrong for a general engine, for three reasons.

1. **It forces a tiny time step**, because a stiff spring has a very high natural frequency, and the integrator must resolve it (the step must be much smaller than 1/ω).
2. **It never holds exactly.** The points always separate a little, and the error grows if the system is heavily loaded.
3. **It adds energy storage that is not in the physical system**, which pollutes the energy ledger.

The classical exact method is **Lagrange multipliers**: add unknown constraint forces λ, and solve the equations of motion *together with* the constraint equations, as one linear system, at each step. It is exact, but it needs a large matrix solve and becomes awkward when constraints can appear and disappear (every contact is a constraint that exists for a few milliseconds).

## The choice: sequential impulses

The program uses **sequential impulses**, the method used by most real-time physics engines. The idea:

1. Move all bodies by the smooth forces over the step, ignoring the constraints. Some constraints are now violated (a pin's two points have separated, or a ball is moving into the floor).
2. For **one constraint at a time**, compute the *impulse* P (a change of momentum, in N·s) that would exactly fix **that constraint's velocity error** if it were the only one. Apply it.
3. Fixing constraint 2 disturbs constraint 1. So **repeat the loop** over all the constraints a fixed number of times (14 by default, <code>env.solverIterations</code>). Each pass reduces the total error; for a stable arrangement it converges.

The impulse for a single constraint along a direction d comes straight from the effective inverse mass of equation 5.2. If the relative velocity along d is v<sub>rel</sub> and it should be zero:

<div class="eq">P = − v<sub>rel</sub> / (k<sub>a</sub> + k<sub>b</sub>),    k<sub>a</sub> = kDir(A, r<sub>a</sub>, d),   k<sub>b</sub> = kDir(B, r<sub>b</sub>, d) <span class="tag">(5.9)</span></div>

Applying P at the points r<sub>a</sub>, r<sub>b</sub> changes the velocities by exactly the amount that cancels v<sub>rel</sub>. The same formula works for a contact normal, for a friction direction, for one of the two axes of a hinge, for a rod's axis. Only the direction d and the target change.

::: why
**Why not Lagrange multipliers?** They would be *exact in one solve*, but they need a matrix assembled from every constraint at every step, and the matrix changes shape when a contact is made or broken. Sequential impulses need no matrix, handle a changing number of constraints trivially, and have a constant, small cost per constraint. The price is that a finite number of iterations leaves a small residual error, which the program controls with the iteration count and with the position correction below.
:::

## How locks, rails and fixed bodies come for free

Because the solver uses *inverse* masses and <code>invMv</code> gives zero along a locked axis, **every constraint automatically respects every lock**. A body on a Y rail is "infinitely heavy" in y: any impulse the solver wants to give it in y changes nothing. A static floor is infinitely heavy in both axes. No constraint needs to know about rails.

After each pass the function <code>applyLocks</code> also zeroes the locked velocity components explicitly, a belt-and-braces step that stops small numerical leakage.

## Hinges, welds, rollers and rods

The four constraint kinds are four small functions inside <code>World::solveJoints</code>. Each is a variation of equation 5.9:

* **Pin.** The constraint is two-dimensional: the two points must coincide in both x and y. The needed impulse is a 2-vector P solving the 2 × 2 system K P = −v<sub>rel</sub>, where the matrix K has entries k11, k12, k22 built from the inverse masses and lever arms (the code computes them and the determinant explicitly rather than calling a library). One impulse handles both axes, and it handles the coupling between them correctly.
* **Weld.** A pin plus a third constraint on the angle: the relative angular velocity must be zero, with impulse λ = −ω<sub>rel</sub> / (1/I<sub>a</sub> + 1/I<sub>b</sub>).
* **Roller.** One scalar constraint: the point must stay on a line, so the relative velocity along the line's *normal* is zeroed. The line is the support's x axis, rotated with its body.
* **Rod.** One scalar constraint along the rod's own direction: the *length* must not change, which means the relative velocity along the rod is zero.

Each function accumulates the impulses it applies in <code>jointImp_</code>. Dividing by the step gives the **reaction force** at the joint, which is what the "reaction force at a support" readout shows.

## Position drift and Baumgarte correction

A velocity-level fix cannot be perfect, and small position errors accumulate (a *drift*): after a thousand steps a pin's two points may have separated by a millimetre. The program removes the drift in a separate stage, <code>correctPositions</code>, run after the positions are advanced. It measures how far the constraint is violated and moves the bodies (not their velocities) by a fraction of that error, in proportion to their inverse masses:

<div class="eq">Δp<sub>b</sub> = + β · e · (invMass<sub>b</sub> / (invMass<sub>a</sub> + invMass<sub>b</sub>)),    β = 0.8 for joints, 0.2 for contacts <span class="tag">(5.10)</span></div>

For contacts the correction is limited to a small maximum per step (0.2 m) and ignores the first half-millimetre of overlap (<code>kSlop</code> = 0.0005 m). The slop is not laziness: a resting body *must* be allowed to sink in by a hair, otherwise the contact flickers on and off and the body jitters.

::: why
**Why not just remove the error from the velocity (Baumgarte stabilisation on velocities)?** Adding a velocity proportional to the position error injects energy, which the ledger would flag. Correcting the *positions* directly, as the program does, adds only a tiny amount of potential energy; the ledger even tracks it (<code>ledger_.posCorr</code>) so that the user can see how much was added.
:::

# Contacts: Collisions, Friction and Rolling

Contacts are the most complex constraint, because they appear and disappear and because they have a *direction-dependent* law (they can push but never pull). Three stages: **detection** (do two bodies overlap?), **manifold** (where, and along which normal?) and **solve** (what impulses?).

## Detection: three shape combinations

<code>World::collide</code> handles the three combinations. Circles are trivial (compare the distance with the sum of the radii). For polygons the program uses the **Separating Axis Theorem** (SAT): two convex shapes do not overlap if and only if there exists a line (an axis, one of the face normals) onto which their projections do not overlap. The function <code>maxSeparation</code> finds, for each face of one polygon, how far the other polygon lies *outside* that face; the largest such value, if positive, proves they are apart; if negative on every axis, it is the **penetration depth** on the axis of least penetration.

The face with the shallowest penetration is chosen as the **reference face**; a small bias (0.0005 m) prefers body A when two faces are almost tied, so that the choice does not flip every step and make the contact jitter. The other body's **incident edge** is the one most facing the reference face, and it is **clipped** to the reference face's side planes (<code>clipSegment</code>). The surviving end points become the contact points: **up to two for polygon against polygon** (an edge resting on an edge), one for circles.

::: why
**Why SAT and not GJK or a general distance routine?** GJK handles any convex shape, but gives only one contact point per query, and a box resting on a floor needs *two* contact points to be stable (one point would let it rock). SAT with clipping gives the full resting manifold directly. Since the program's shapes are all convex polygons and circles (a beam is a thin box), SAT's limitation, convexity, is not a limitation here.
:::

## The manifold and warm starting

Each contact point becomes a <code>Contact</code> with the world points on each body, the normal (from A to B), the separation, and **a key** built from the body indices and the feature identifiers (which edge, which corner). The key lets the solver recognise "the same contact as the last step" and re-apply last step's impulse as a starting guess: **warm starting**. Resting stacks converge in one or two iterations instead of dozens, and nothing creeps. The cache is rebuilt each step (<code>warm_</code>) so that contacts that have ended are forgotten.

## The impulses: normal, friction and rolling

In <code>solveContacts</code>, for each contact, in this order:

1. **Friction first**, bounded by the *previous* pass's normal impulse. The tangent is t = (n<sub>y</sub>, −n<sub>x</sub>). The impulse that would cancel the sliding is clamped to the **Coulomb cone**: |P<sub>t</sub>| ≤ μ P<sub>n</sub>. If the clamp is active, the surfaces are *sliding* and the kinetic coefficient μ<sub>k</sub> applies; otherwise they *stick* (static μ<sub>s</sub>). Clamping an **accumulated** impulse, not each increment, is what makes the iterated friction law correct.
2. **The normal impulse**, with the restitution target: λ = −m<sub>n</sub> (v<sub>n</sub> − bias), and the accumulated normal impulse is clamped at zero: **contacts push, never pull**. The bias is e · v<sub>n</sub> for a genuine impact (approach speed above <code>restThreshold</code>, 0.25 m/s by default) and zero otherwise, which is how a ball stops bouncing instead of micro-bouncing forever.
3. **Rolling resistance**, an angular impulse that opposes relative spin, limited by (rollRes · R) P<sub>n</sub>, applies only to circles.

The material properties of the pair are combined by the rule in the Environment (geometric mean for friction, maximum for restitution by default): <code>World::combine</code>.

@@src core/src/world.cpp | ^void World::solveContacts | ^\}

## Why a very small time step matters

Contacts are checked only at the **start** of a step. If a body moves further than its own thickness in one step it can jump straight through a wall: *tunnelling*. At the default step of 0.25 ms a body would have to move at more than 100 m/s to cross a 3 cm slab. The program adds a safety net in <code>World::step</code>: it measures how far every colliding body can move in the step (speed plus the speed the forces will add, plus rotation at the body's bounding radius) and, if that exceeds an eighth of the body's smallest dimension (a quarter of its half-width), divides the step into up to 64 **sub-steps**.

@@src core/src/world.cpp | ^void World::step\(\) | ^\}

The alternative, *continuous collision detection* (sweeping each shape through time to find the first contact), is exact but complex, and for the speeds this program meets sub-stepping is simpler and costs nothing when no body is fast.

# Advancing in Time

## Two integrators, chosen automatically

<code>World::usesConstraintSolver</code> decides which of two methods advances the world:

* **RK4** (the classical fourth-order Runge-Kutta method, <code>stepSmooth</code>), for scenes with only smooth forces: springs, dampers, gravity, loads. It evaluates the forces four times per step and combines them with the weights 1, 2, 2, 1 over 6. Its error falls as the *fifth* power of the step, and the validation test shows the mass-spring-damper matches its exact solution to 8 × 10⁻¹³ m over 10 s. No constraint or contact is involved.
* **The constrained step** (<code>stepConstrained</code>), as soon as there is a rod, pin, weld, roller, or two colliding bodies. It is **semi-implicit (symplectic) Euler** with the impulse solver in the middle, and its steps are:

1. Compute the smooth forces and update the **velocities**: v ← v + (F/m) h.
2. Apply the locks.
3. Detect contacts and set up the impulses (warm starting).
4. Run the solver: for 14 iterations, <code>solveJoints</code>, <code>solveContacts</code>, <code>applyLocks</code>.
5. Do the energy accounting for the contacts (exact: ΔKE = P · (v<sub>before</sub> + v<sub>after</sub>) / 2 for every impulse).
6. Update the **positions** from the new velocities: p ← p + v h.
7. Correct the position drift (<code>correctPositions</code>).

::: why
**Why change the method when constraints appear, rather than always using the constraint path?** RK4 is far more accurate for smooth systems, and the simplest examples (a mass on a spring) are what a student checks against a formula. Using the less accurate method for them would degrade the result for no reason. The automatic choice also means that **adding a hinge to a pure spring system silently changes the integrator**, which is why the torsional-spring test, which contains a pin, was validated at the constrained step's accuracy (0.4972 s against 0.4973 s), not RK4's.
:::

**Why velocities before positions?** Updating the velocity first and then using the *new* velocity to move the position is the semi-implicit (symplectic) Euler method. Unlike explicit Euler (position from the *old* velocity) it conserves energy over long runs for oscillating systems, with no systematic gain or loss. That is essential for a program that reports an energy ledger.

## The energy ledger: the program checking itself

Every joule the program creates or destroys is recorded. The identity is

<div class="eq">E(t) = E<sub>0</sub> + W<sub>in</sub> − D<sub>viscous</sub> − D<sub>drag</sub> − D<sub>friction</sub> − D<sub>impact</sub> + ε <span class="tag">(5.11)</span></div>

with E = T + V the kinetic plus potential energy, W<sub>in</sub> the work of the loads, the D terms the heat from each loss channel, and ε the numerical error, which is the only unexplained term. <code>ledger_.numerical()</code> returns ε. The validation suite fails the build if ε exceeds a small fraction of the energy that moved in **any** example.

The ledger is also how every generalization in this chapter was checked. When the distribution code was first written, the energy of ordinary springs jumped, and the ledger test failed: a copy-and-paste mistake used the wrong attachment point when a link was *not* a distribution. Without the ledger that error would have been invisible, because the motion still *looked* reasonable.

# Making and Keeping It General: Testing

Generality is a claim about **every** system, and it can only be tested on particular ones. The validation suite (<code>tests/core_tests.cpp</code>) is therefore built on one principle: **for every kind of element there is a system whose answer is known in closed form, and the program must reproduce it.**

| Generalization | Closed-form check |
|----------------|-------------------|
| Mass-spring-damper | x(t) = e<sup>−ζω t</sup>(cos ω<sub>d</sub> t + …) to 10⁻¹² |
| Pendulum on a rod | period 2π√(L/g) (with the large-angle correction) |
| Hinged beam | period 2π√(I/(m g d)) |
| Spring distribution (constant) | K = k ℓ: oscillates at √(kℓ/m) |
| Spring distribution (linear) | K = m ℓ²/2: the same frequency from a different distribution |
| Torsional spring | quarter period (π/2)√(I/k<sub>t</sub>) |
| Constant moment | ω = M t / I |
| F(t) = 3t² | v = t³ / m |
| M(t) = 4t | ω = 2t² / I |
| Elastic collision | momentum and kinetic energy both conserved |
| Rubber ball | rebound height e² h |
| Every example | the energy ledger closes |

When a new element is added, the **first thing written is its test**, and the test is a system the reader could check on paper. If the answer cannot be found on paper, the element is probably too complicated to trust.

# Recipe: Adding a New Element to the Engine

The torsional spring, the distributions and the time-varying loads were all added with the same recipe. Follow it for anything new (a rotational damper, a one-way spring, a gap element, a wind load).

1. **Write the physics on paper first.** Give the energy (or the dissipation function, or the work) in terms of the state, and differentiate. Check the signs against the clockwise-positive, y-down convention. Predict a closed-form result you can test.
2. **Add the data to the model** (<code>model.hpp</code>): as new fields on <code>Link</code> or <code>Body</code> with defaults that mean "off". A default of zero or empty means that **old files and old code are unaffected**.
3. **Add the force or constraint to the engine** (<code>world.cpp</code>): a smooth force in <code>computeForces</code> and its energy in <code>potentialEnergy</code>; a constraint in <code>solveJoints</code> or <code>solveContacts</code>. Keep the force loop and the energy loop in step.
4. **Account for its power in the ledger.** A load adds F·v or M·ω to the work-in integral; a dissipative element adds its heat to the right loss channel.
5. **Save and load it** (<code>project_io.cpp</code>): append the new fields to the *end* of the body or link line and read them only if the line is long enough (<code>f.size() &gt; n</code>). Files saved by older versions then open unchanged.
6. **Write the test** in <code>core_tests.cpp</code>, build, and run <code>vlab_tests.exe</code>. A new element is not finished until the suite passes **and** the ledger test still passes.
7. **Add the interface**: a tool or a field in the properties panel, how it is drawn, how it is picked (clicking it must select it), how it is deleted. The picking function (<code>App::pickLink</code>) was extended for distributions after a user could not select them; always test the *click*, not just the physics.
8. **Document it** in the guide, and then **publish** (next chapter): rebuild, repackage, sync the Unreal plugin, commit and push.

::: try
**Try it.** Add a *rotational damper* to the hinge: a torque −c<sub>θ</sub> ω<sub>rel</sub>. Hint: the Pin already has a rotary damper, <code>jointC</code>. Read how <code>computeForces</code> uses it (the variable <code>Tc</code>), how the dissipation power is added to <code>pv</code>, and why the damper only acts when <code>env.damping</code> is on. Then write down which test would prove it correct (a hinged beam with only the rotary damper should decay as e<sup>−c t / 2I</sup> in angular velocity).
:::

# Whys and Why-Nots at a Glance

| Decision | What was chosen | Main alternative | Why the choice |
|----------|-----------------|------------------|----------------|
| Coordinates | x, y, θ for every body | Minimal (generalized) coordinates | The user changes the system at run time; loops and contacts are trivial |
| Attachment points | In each body's local frame | In world coordinates | Rotation and orientation become automatic |
| Forces | Derived from an energy, written once per element kind | One equation per system | Works for any system; checkable |
| One link structure | A tagged struct | A class hierarchy | Copyable, savable, no virtual machinery |
| Constraints | Sequential impulses | Penalty springs / Lagrange multipliers | No stiff time step, no matrix, handles contact churn |
| Locks | Zero inverse mass on an axis | Special equations for rails | Every constraint respects every lock for free |
| Collision detection | SAT with edge clipping | GJK, signed distance fields | Two stable contact points per resting box |
| Integrator | RK4 or symplectic Euler, automatically | One method for everything | Accuracy for smooth systems, energy behaviour for constrained ones |
| Time loads | A small formula language | An embedded script engine | Cannot hang, no dependency, bounded behaviour |
| Distributions | N elements, midpoint rule | One equivalent spring, FE foundation | Exact for constant and linear; gives rotation for free |
| Tunnelling | Adaptive sub-stepping | Continuous collision detection | Simpler, free when nothing is fast |
| Verification | Energy ledger and closed-form tests | Visual inspection | A result that cannot be checked is not a result |
