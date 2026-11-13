@@part Part II — The Engine, Line by Line
# The Model: engine.hpp, Part 1

This chapter reads the first third of <code>engine.hpp</code>: the data structures that describe *what the user drew*. The physics comes in Chapter 6. Every listing below is real source; read the listing first, then the explanation under it.

## The file header and the namespace

@@src engine.hpp | ^// engine.hpp | ^namespace lag

* **The first twenty or so lines** are a comment. In C++ everything after <code>//</code> on a line is ignored by the compiler and written for humans. This one summarises the physics so that anyone opening the file sees the plan at once: the model, the energies, and the Euler–Lagrange equation.
* **<code>#pragma once</code>** tells the compiler: "if another file includes me twice, only read me the first time." Without it, <code>live.cpp</code> and <code>model_io.hpp</code> would both pull in <code>engine.hpp</code>, and the compiler would complain that <code>struct Node</code> was defined twice.
* **<code>#include &lt;algorithm&gt;</code>** gives <code>std::fmax</code>-style helpers and <code>std::remove_if</code>; <code>&lt;cmath&gt;</code> gives <code>std::sqrt</code>, <code>std::hypot</code>, <code>std::cos</code>; <code>&lt;vector&gt;</code> gives <code>std::vector</code>, a resizable array.
* **<code>namespace lag { ... }</code>** puts every name inside a labelled box called <code>lag</code> (for *Lagrange*). Outside the box we write <code>lag::Model</code>. This stops our names (<code>Node</code>, <code>State</code>, <code>step</code>) from clashing with names in Windows' enormous header files.

::: why
**Why a namespace?** <code>windows.h</code> defines thousands of names, including macros with very common words such as <code>min</code>, <code>max</code> and <code>small</code>. Wrapping our code in <code>namespace lag</code> avoids silent collisions.
:::

## Constants and enumerations

@@src engine.hpp | ^constexpr int    MAX_NODES | ^enum Rail

* **<code>constexpr int MAX_NODES = 24;</code>** A *constant known at compile time*. We refuse to place more than 24 nodes. Why 24? The telemetry log stores several numbers per node for every frame, so a limit keeps memory use predictable. The limit is also enforced in <code>addMass</code> and <code>addGround</code>.
* **<code>constexpr double PI = 3.14159265358979323846;</code>** The ratio of a circle's circumference to its diameter, with 21 digits (more than a <code>double</code> can hold, which is fine). We define it ourselves because <code>M_PI</code> is not part of the C++ standard and is missing on some compilers.
* **<code>enum class NodeKind { Mass, Ground };</code>** A type that can only be one of two names. <code>enum class</code> (rather than plain <code>enum</code>) keeps the names inside the type, so we must write <code>NodeKind::Mass</code>. That prevents accidents like comparing a <code>NodeKind</code> to a <code>LinkKind</code>.
* **<code>enum class LinkKind { Spring, Damper };</code>** The same idea for links.
* **<code>enum Rail { FREE = 0, RAIL_X = 1, RAIL_Y = 2 };</code>** A plain <code>enum</code>, on purpose: its names are plain integers <code>0, 1, 2</code>. We want that, because the user edits the rail with a *slider*, and a slider produces a number. (See the field <code>rail</code> in <code>Node</code>.)

## Node: one thing on the canvas

@@src engine.hpp | ^struct Node \{ | ^\};

A **struct** is a bundle of named fields. A <code>Node</code> is either a mass or a ground anchor.

* **<code>NodeKind kind = NodeKind::Mass;</code>** Which of the two it is. The <code>= ...</code> is a *default value*, used when we write <code>Node n;</code>.
* **<code>double x = 0, y = 0;</code>** The **design position**: where you placed it with the mouse, in metres. Note the comment: *y points down*, like the screen. A larger y is lower on screen. Choosing the screen's convention avoids a sign flip every time the program converts between maths and pixels.
* **<code>double m = 1.0;</code>** The mass in kilograms. Ignored for ground anchors (an anchor is conceptually infinitely heavy).
* **<code>double vx0 = 0, vy0 = 0;</code>** The **starting velocity**. When you press Run, the masses begin with these speeds. This is how you "hit" a mass at time zero.
* **<code>double rail = FREE;</code>** The rail setting, as a <code>double</code>. It is a <code>double</code> and not an <code>enum</code> so the *same* slider code that edits masses and stiffnesses can edit it (with a "snap to whole numbers" option). It is converted back with <code>(int)std::lround(nd.rail)</code> wherever it is used.
* **<code>double driveF</code>, <code>driveAngle</code>, <code>driveW</code>.** A harmonic push <code>F cos(ωt)</code>: its amplitude (newtons), direction (degrees) and angular frequency (radians per second). Zero amplitude means no push.

::: whatif
**What if <code>rail</code> were an enum class?** We would need a separate, special-purpose control in the panel for choosing it, and a conversion to and from numbers for the save file. Storing it as a number made the slider code, the file code and the engine all simpler. The price is a cast, <code>(int)std::lround(...)</code>, in a few places.
:::

## Link: a spring or a damper

@@src engine.hpp | ^struct Link \{ | ^\};

* **<code>LinkKind kind</code>**: spring or damper. One struct serves both, because they share the same geometry; only the *force law* differs.
* **<code>int a = -1, b = -1;</code>** The **indices** of the two nodes in <code>Model::nodes</code>. The value −1 means "not set". Indices (rather than pointers) stay valid when a vector is copied, which matters for saving and for undo-style features.
* **<code>double k</code>, <code>k3</code>, <code>L0</code>.** Spring data: stiffness k, cubic stiffness k₃, and rest length L₀ (the length at which the spring pulls with zero force).
* **<code>bool autoRest = true;</code>** While true, L₀ follows the *drawn* length, so a spring you draw is born relaxed. Once you move the "rest length" slider by hand, this becomes false and L₀ stays where you put it. Chapter 10 shows where it is cleared.
* **<code>double c</code>.** Damper data: the damping coefficient. A damper ignores the spring fields, and a spring ignores <code>c</code>. The unused fields cost a few bytes and buy a very simple design.

## Model, part 1: the container and two helper functions

@@src engine.hpp | ^struct Model \{ | +4

* **<code>std::vector&lt;Node&gt; nodes;</code>** A resizable list of every mass and anchor. A node's *position in this list* is its identity: node 3 is <code>nodes[3]</code>.
* **<code>std::vector&lt;Link&gt; links;</code>** The same for springs and dampers.
* **<code>double gravity = 0.0;</code>** One number for the whole world, in m/s². Zero means weightless. The sliders allow up to 20.

@@src engine.hpp | ^    int massCount | ^    \}$

* **<code>int massCount() const</code>** Counts the nodes that are masses. The word <code>const</code> after the parentheses promises that this function does not change the model, so it can be called on a read-only model.
* **<code>n += (nd.kind == NodeKind::Mass);</code>** A comparison produces <code>true</code> or <code>false</code>, which C++ treats as 1 or 0, so adding it counts the matches. It is a compact way to count.
* **<code>for (const Node& nd : nodes)</code>** A *range-for*: "for every element in <code>nodes</code>". The <code>const Node&amp;</code> means "look at it, do not copy it, do not change it".

@@src engine.hpp | ^    double distance | ^    \}$

* **<code>std::hypot(dx, dy)</code>** computes √(dx² + dy²) without overflow or loss of accuracy. It is the length of the arrow between nodes a and b: equation (3.2).

## Model, part 2: adding things

@@src engine.hpp | ^    int addMass | ^    \}$

* **<code>if ((int)nodes.size() &gt;= MAX_NODES) return -1;</code>** The limit check. We return −1, which means *failure*; the caller (the click handler) shows a message. The cast <code>(int)</code> is needed because <code>size()</code> returns an unsigned type.
* **<code>Node n; n.kind = ...; n.x = x; ...</code>** Build a node on the stack, fill in the fields.
* **<code>nodes.push_back(n);</code>** Copy it onto the end of the list.
* **<code>return (int)nodes.size() - 1;</code>** Return its new index. The caller uses it to select the new node.

@@src engine.hpp | ^    int addGround | ^    \}$

* The same pattern as <code>addMass</code>, with <code>kind = Ground</code> and no mass value.

@@src engine.hpp | ^    // Returns the new link's index | ^    \}$

This function is the "gatekeeper" for links. It refuses nonsense and returns −1:

* **<code>if (a == b || a &lt; 0 || b &lt; 0 || a &gt;= nodes.size() ...)</code>** You cannot join a node to itself, and both indices must exist. Without this check, a stray click could create a link to node −1 and the program would read memory that is not ours (a crash).
* **<code>if (nodes[a].kind == Ground &amp;&amp; nodes[b].kind == Ground) return -1;</code>** A link between two fixed anchors can never do anything, because neither end can move, so we refuse it. (If we allowed it, <code>accel()</code> would still compute a force and then ignore it.)
* **The duplicate loop.** Two *springs* between the same pair would behave as one stiffer spring, which is confusing, so we refuse that. But a spring and a damper between the same pair is allowed: it is the classic **parallel spring-damper** (a car's shock absorber). The test <code>l.kind == kind &amp;&amp; ((l.a == a &amp;&amp; l.b == b) || (l.a == b &amp;&amp; l.b == a))</code> says "same kind, same two nodes, in either order".
* **<code>l.L0 = std::fmax(0.05, distance(a, b));</code>** A new spring starts at the distance between its ends (so it is relaxed), but never less than 5 cm. A zero-length rest length would make the geometry unstable.

## Model, part 3: removing things

@@src engine.hpp | ^    void removeLink | +1

* The one-line <code>removeLink</code> checks the index is valid, then <code>links.erase(links.begin() + i)</code> deletes element *i* and shifts later ones down.

@@src engine.hpp | ^    void removeNode | ^    \}$

Removing a node is trickier than removing a link, because links refer to nodes **by index**. Deleting node *i* changes the index of every later node. The function therefore does three things, in this order:

1. **Delete every link that touches the node.** The loop runs from the last link down to the first: <code>for (int j = (int)links.size() - 1; j &gt;= 0; --j)</code>. Going *backwards* matters, because erasing an element shifts the later ones down, which would make a forward loop skip an element.
2. **Renumber.** <code>for (Link&amp; l : links) { if (l.a &gt; i) --l.a; if (l.b &gt; i) --l.b; }</code>. Any link that pointed at a node *after* the deleted one now points one place earlier. Links that touched the deleted node are already gone.
3. **Delete the node itself.**

::: whatif
**What if we deleted the node first and renumbered later?** The remaining links would briefly point at the wrong nodes, and, worse, links attached to the deleted node would point at whatever node slid into its place. Always remove the *references* before removing the *thing*.
:::

## Model, part 4: automatic rest length

@@src engine.hpp | ^    // Springs that have never | ^    \}$

* Loops over all links; for every **spring** whose <code>autoRest</code> is still true, it sets L₀ to the current drawn distance. The interface calls this after every drag, so a spring you stretch while designing is still relaxed. Chapter 10 shows when.

## State: the numbers that change while it runs

@@src engine.hpp | ^// -.*the moving state | ^\};

* The **design** (the <code>Model</code>) says what exists. The **state** says where everything is *right now*. We keep them separate on purpose: pressing Stop must put every mass back at its design position, and that is trivial if the design was never touched while running.
* <code>std::vector&lt;double&gt; q, v;</code> Two flat arrays with **two numbers per node**: <code>q[2*i]</code> = x<sub>i</sub>, <code>q[2*i+1]</code> = y<sub>i</sub>, and the same for v. This layout is exactly equation (3.1) of Chapter 3.
* <code>double t = 0;</code> The simulated time in seconds.

::: why
**Why flat arrays rather than a vector of little point structs?** The integrator treats the whole state as one long list of numbers and does the same arithmetic to every entry (<code>q[i] += h * v[i]</code>). A flat array makes that a simple loop, and makes RK4 independent of how many nodes there are.
:::

@@src engine.hpp | ^inline State initialState | ^\}$

* <code>const int n = (int)M.nodes.size();</code> The number of nodes.
* <code>s.q.assign(2 * n, 0.0); s.v.assign(2 * n, 0.0);</code> Make both arrays of length 2n, filled with zero. Ground anchors keep zero velocity forever.
* The loop copies each node's design position into <code>q</code>. Only for **masses** it also sets the velocity.
* <code>int rail = (int)std::lround(nd.rail);</code> Rounds the slider's number to the nearest whole number (0, 1 or 2).
* <code>s.v[2*i] = (rail == RAIL_Y) ? 0.0 : nd.vx0;</code> The *conditional operator*: "if a vertical rail, the horizontal speed must be zero, otherwise use the chosen vx0". Likewise for y. This is the starting-velocity half of the rail rule: a mass cannot begin with speed in a direction it is not allowed to move.

::: check
In the program, load example 4 (quarter-car), select the wheel and look at the "start vx" slider: moving it changes nothing, because the wheel is on a Y rail. That is <code>initialState</code> at work.
:::

## Chapter checks

::: quiz
1. Why is <code>y</code> positive downward in this program?
2. Why does <code>removeNode</code> loop through the links *backwards*?
3. Why can a spring and a damper join the same two nodes, but two springs cannot?
4. What does <code>#pragma once</code> prevent?
5. Why is the rail stored as a <code>double</code>?
6. Where does the program keep the *design*, and where the *current positions*?
:::

::: answers
1. It matches the screen, so converting between metres and pixels needs no sign flip.
2. Erasing an element shifts later ones down; going backwards means the elements we still have to visit are not moved.
3. A spring + damper in parallel is a standard, meaningful part (a shock absorber); two parallel springs are just one stiffer spring and would only confuse.
4. The same file being read twice (which would define the same struct twice).
5. So the same slider code can edit it; it is cast back with <code>std::lround</code>.
6. Design in <code>Model</code>; current positions in <code>State</code>.
:::

# The Physics: engine.hpp, Part 2

This is the heart of the program. Every function in this chapter is one of the equations from Chapter 3, written in C++. Keep Chapter 3 open beside this one.

## linkInfo: the geometry of a link

@@src engine.hpp | ^struct LinkInfo | ^\};

A <code>LinkInfo</code> is a **report card** for one link at one instant. Instead of recomputing the length and direction in five different places, we compute them once and return a bundle:

* <code>len</code>: the length ℓ. <code>ux, uy</code>: the unit vector **u**. <code>stretch</code>: s = ℓ − L₀. <code>rate</code>: ṡ. <code>force</code>: the tension along the link, positive when it pulls its ends together.

@@src engine.hpp | ^inline LinkInfo linkInfo | ^\}$

Go through it line by line, with equations (3.2) to (3.7) beside you:

* **<code>const Link&amp; L = M.links[li];</code>** A shorthand name for "the li-th link", without copying it.
* **<code>double dx = q[2*L.b] - q[2*L.a], dy = ...;</code>** The vector **d** = **r**<sub>b</sub> − **r**<sub>a</sub> from (3.2). Node b's x is at <code>q[2*L.b]</code>.
* **<code>r.len = std::hypot(dx, dy);</code>** ℓ = |**d**|.
* **<code>if (r.len &gt; 1e-9) { r.ux = dx / r.len; ... }</code>** **u** = **d**/ℓ. The guard matters: if two nodes sit exactly on top of each other, ℓ = 0 and dividing by it would give not-a-number (NaN), which would then spread through every later calculation and destroy the simulation. When ℓ is tiny we keep the default **u** = (1, 0). 1e-9 means 10⁻⁹.
* **<code>double dvx = v[2*L.b] - v[2*L.a], dvy = ...;</code>** The relative velocity **v**<sub>b</sub> − **v**<sub>a</sub>.
* **<code>r.rate = r.ux * dvx + r.uy * dvy;</code>** The dot product **u · (v<sub>b</sub> − v<sub>a</sub>)**: equation (3.7), ṡ.
* **<code>if (L.kind == LinkKind::Spring) { r.stretch = r.len - L.L0; r.force = L.k * r.stretch + L.k3 * s*s*s; }</code>** The spring's stretch and the force f = k s + k₃ s³ from (3.5).
* **<code>else { r.force = L.c * r.rate; }</code>** The damper's force c ṡ from (3.8).

::: mapx
| Maths | Code |
|-------|------|
| d = r<sub>b</sub> − r<sub>a</sub> | <code>dx, dy</code> |
| ℓ = &#124;d&#124; | <code>r.len</code> |
| u = d / ℓ | <code>r.ux, r.uy</code> |
| s = ℓ − L₀ | <code>r.stretch</code> |
| ṡ = u · (v<sub>b</sub> − v<sub>a</sub>) | <code>r.rate</code> |
| f = k s + k₃ s³ (spring) | <code>r.force</code> |
| f = c ṡ (damper) | <code>r.force</code> |
:::

::: whatif
**What if we forgot the <code>len &gt; 1e-9</code> guard?** Drag a mass exactly onto its own anchor and the program would divide by zero. In floating-point arithmetic that gives infinity or NaN, the NaN poisons every number, and the screen would fill with garbage. The guard turns a catastrophe into a harmless no-op.
:::

## kinetic: T

@@src engine.hpp | ^inline double kinetic | ^\}$

* Loops over every node and **skips ground anchors** (<code>if (... == Mass)</code>), which have no motion.
* <code>0.5 * m * (vx*vx + vy*vy)</code> is T = ½ m |**v**|², since |**v**|² = vₓ² + v<sub>y</sub>². Summing over masses gives the total kinetic energy.
* Why <code>vx*vx</code> and not <code>pow(vx, 2)</code>? Multiplication is exact and fast; <code>pow</code> is a slower general-purpose function.

## potential: V

@@src engine.hpp | ^inline double potential | ^\}$

* <code>std::vector&lt;double&gt; zero(q.size(), 0.0);</code> A list of zeros the same size as <code>q</code>. We call <code>linkInfo</code> only to get the *geometry*, but that function needs a velocity list, so we hand it a list of zeros.
* The first loop adds, for every **spring** (dampers are skipped, they store no energy), V = ½ k s² + ¼ k₃ s⁴ from (3.4).
* The second loop adds each mass's gravity energy: <code>V -= m * g * (y - y_design)</code>. This is (3.9): <code>-=</code> because V<sub>g</sub> = −m g (y − y₀). Moving *down* (y grows) lowers the potential energy, which is why a falling ball speeds up.

::: why
**Why reference the design y?** If we used the raw y, a system drawn near the bottom of the canvas would begin with a large positive V and one drawn near the top with a large negative V. Using <code>(y - y_design)</code> makes every system start with V<sub>gravity</sub> = 0, which makes the telemetry easy to read. Forces are unaffected, since forces come from *differences* in V.
:::

## rayleigh: D

@@src engine.hpp | ^inline double rayleigh | ^\}$

* The mirror image of <code>potential</code> for dampers: skip springs, and add <code>0.5 * c * rate * rate</code>, which is D = ½ c ṡ² from (3.8). It is used to compute the heat power 2D in the energy law (3.13).

## driveForce: the harmonic push

@@src engine.hpp | ^inline void driveForce | ^\}$

* <code>double f = n.driveF * std::cos(n.driveW * t);</code> The size of the push at time t: F₀ cos(ωt). It swings between +F₀ and −F₀.
* <code>double a = n.driveAngle * PI / 180.0;</code> Degrees to radians: multiply by π/180. C++ trigonometric functions use radians.
* <code>fx = f * std::cos(a); fy = f * std::sin(a);</code> Split the push into x and y parts.
* **<code>double&amp; fx, double&amp; fy</code>** are *reference parameters*: the function writes its two answers into the caller's variables. That is how a C++ function returns two values without making a struct.

## inputPower: P = Q · v

@@src engine.hpp | ^inline double inputPower | ^\}$

* This is P<sub>in</sub> = Σ **Q · v** from (3.13): force times velocity is power. For every mass it adds the drive force and the mouse force (<code>ext</code>) and multiplies by the velocity.
* <code>if (2*i + 1 &lt; ext.size())</code> guards against a shorter <code>ext</code> vector. The mouse-force list is created when the simulation starts; this check stops a crash if a caller passes an empty list (as the unit tests sometimes do).

## accel: the Euler–Lagrange equations

@@src engine.hpp | ^// -.*Euler-Lagrange equations | ^\}$

This one function is the whole physics of the program. Read it in two halves.

**The setup (first lines).**

* <code>const size_t n = M.nodes.size();</code> The node count.
* <code>std::vector&lt;double&gt; F(2 * n, 0.0);</code> One force per coordinate, all starting at zero. After loop 1 it holds the total *link* force on every node.

**Loop 1: every spring and damper.**

* <code>LinkInfo in = linkInfo(M, q, v, (int)li);</code> The geometry report for this link, including its <code>force</code>.
* <code>F[2*L.a] += in.force * in.ux;</code> and <code>F[2*L.a+1] += in.force * in.uy;</code> The force on node **a** is **+f u** (3.6). The loop adds, so a node touched by several links receives all of their forces.
* <code>F[2*L.b] -= in.force * in.ux;</code> and the y-version. The force on node **b** is **−f u**.

Notice that **springs and dampers go through exactly the same four lines**. The only difference is how <code>in.force</code> was computed inside <code>linkInfo</code>. This sharing is the practical payoff of the derivation in Chapter 3.

**Loop 2: every mass.**

* <code>a.assign(2 * n, 0.0);</code> The answer array, starting at zero, so ground anchors stay at 0.
* <code>if (nd.kind != NodeKind::Mass) continue;</code> Skip anchors: they never accelerate.
* <code>double fx = F[2*i], fy = F[2*i+1];</code> Start from the link forces.
* <code>fy += nd.m * M.gravity;</code> Add gravity: −∂V<sub>g</sub>/∂y = +m g (3.9).
* <code>driveForce(nd, t, dx, dy); fx += dx; fy += dy;</code> Add the drive.
* <code>if (2*i + 1 &lt; ext.size()) { fx += ext[2*i]; ... }</code> Add the mouse force.
* <code>double ax = fx / nd.m, ay = fy / nd.m;</code> Newton's law, rearranged: a = F/m. This is (3.11).
* <code>if (rail == RAIL_X) ay = 0; if (rail == RAIL_Y) ax = 0;</code> The rail rule: the blocked acceleration is zeroed.
* <code>a[2*i] = ax; a[2*i+1] = ay;</code> Store the answer.

::: mapx
| Equation | Line of code |
|----------|--------------|
| (3.6) spring/damper force on a: +f u | <code>F[2*L.a] += in.force * in.ux</code> |
| (3.6) spring/damper force on b: −f u | <code>F[2*L.b] -= in.force * in.ux</code> |
| (3.9) gravity force +m g | <code>fy += nd.m * M.gravity</code> |
| drive F cos(ωt) | <code>driveForce(...)</code> then <code>fx += dx</code> |
| mouse force | <code>fx += ext[2*i]</code> |
| (3.10) m a = ΣF | <code>ax = fx / nd.m</code> |
| rails | <code>if (rail == RAIL_X) ay = 0</code> |
:::

::: whatif
**What if we computed <code>fx / nd.m</code> for a ground anchor?** Ground has the default mass 1, so it would move when pulled! The <code>continue</code> for non-masses is what makes a wall a wall.
:::

::: try
Select a spring in example 2, raise its stiffness to the maximum, and watch the masses vibrate faster. Then lower the *mass* slider of a block to its minimum (0.1 kg) and raise the stiffness again. If the motion became violent or the numbers blew up, you are seeing what the stability limit of Chapter 4 protects against. The program notices non-finite numbers and stops with a message.
:::

## Chapter checks

::: quiz
1. Why does <code>linkInfo</code> return a struct, not just one number?
2. What does the guard <code>len &gt; 1e-9</code> prevent?
3. Why does <code>potential</code> pass a list of zeros to <code>linkInfo</code>?
4. In <code>accel</code>, which two lines apply a spring's force to its two ends, and why do their signs differ?
5. Which line makes a wall a wall?
6. Why is the mouse force added in <code>accel</code> and not in the integrator?
:::

::: answers
1. Because the length, unit vector, stretch, rate and force are all needed by different callers; computing them once avoids repeating the work and the bugs.
2. Division by zero when two nodes coincide, which would create NaN.
3. It needs only geometry, but <code>linkInfo</code> requires velocities, so we give it zeros.
4. <code>F[2*L.a] += ...</code> and <code>F[2*L.b] -= ...</code>: the two ends are pulled in opposite directions (equal and opposite forces).
5. <code>if (nd.kind != NodeKind::Mass) continue;</code>, so ground nodes keep zero acceleration.
6. Because the force is part of the equation of motion at every stage of RK4 and must be included whenever the acceleration is evaluated.
:::

# The Integrator and the Meter: engine.hpp, Part 3

## rk4: one step of the fourth-order Runge–Kutta method

@@src engine.hpp | ^// -.*one RK4 step | ^\}$

This function advances the state by one small time step *h*. Compare it with equation (4.3) and Figure 4.2.

* **<code>const size_t N = s.q.size();</code>** The number of coordinates (twice the node count).
* **<code>std::vector&lt;double&gt; a1, a2, a3, a4, qt(N), vt(N), v2, v3, v4;</code>** Scratch space. <code>a1..a4</code> hold the four acceleration samples (the "slopes" of velocity). <code>qt, vt</code> are the trial positions and velocities at which each new sample is taken. <code>v2, v3, v4</code> remember the trial velocities, because the *position* updates need them.
* **Stage 1.** <code>accel(M, s.t, s.q, s.v, ext, a1);</code> Acceleration at the start of the step: that is k₁ (for velocity). The slope of position at the start is just the velocity <code>s.v</code>.
* **Stage 2.** <code>qt[i] = s.q[i] + 0.5*h*s.v[i]; vt[i] = s.v[i] + 0.5*h*a1[i];</code> Walk half a step along the stage-1 slopes to a trial point. <code>v2 = vt;</code> remembers that trial velocity (it is the position-slope at stage 2). Then <code>accel(M, s.t + 0.5*h, qt, vt, ext, a2);</code> measures the acceleration there.
* **Stage 3.** Same again, but walk half a step along the *stage-2* slopes: <code>qt = q + 0.5*h*v2; vt = v + 0.5*h*a2</code>. Save <code>v3</code>, measure <code>a3</code>.
* **Stage 4.** Walk a *full* step along the stage-3 slopes: <code>qt = q + h*v3; vt = v + h*a3</code>. Save <code>v4</code>, measure <code>a4</code> at time <code>t + h</code>.
* **The final update.** <code>s.q[i] += h/6.0 * (s.v[i] + 2*v2[i] + 2*v3[i] + v4[i]);</code> and <code>s.v[i] += h/6.0 * (a1[i] + 2*a2[i] + 2*a3[i] + a4[i]);</code> The weighted average of equation (4.3), with weights 1, 2, 2, 1.
* **<code>s.t += h;</code>** Time moves forward.

::: why
**Why do the position slopes (v, v2, v3, v4) differ?** Position changes at the rate of *velocity*, and velocity itself changes during the step. At the start the velocity is <code>s.v</code>; the trial velocities at the later stages are the best estimates of the velocity half-way and at the end. RK4 averages all four, which is the source of its accuracy.
:::

::: whatif
**What if <code>accel</code> did not receive the time <code>t</code> at each stage?** The drive force F cos(ωt) depends on t. If every stage used the *start* time, the drive would be sampled only once per step and RK4 would lose its accuracy for driven systems. That is why <code>accel</code> takes <code>t + 0.5*h</code> and <code>t + h</code> at the later stages.
:::

## The Meter: checking the law of energy

@@src engine.hpp | ^// -.*energy book-keeping | ^\};

The Meter keeps the books of equation (3.14).

* <code>T, V, E</code>: the current kinetic, potential and total energy.
* <code>dissipated</code>: the running total of ∫ 2D dt, the heat produced so far.
* <code>workIn</code>: the running total of ∫ P<sub>in</sub> dt, the work done by outside forces.
* <code>E0</code>: the energy at the start (or at the last "rebase").
* **<code>double residual() const { return E - (E0 + workIn - dissipated); }</code>** The *error*: the difference between the energy we measure and the energy the law (3.14) says we should have. In a perfect simulation it is zero. The telemetry box shows it as "energy-law error".
* <code>measure(M, s)</code> recomputes T, V and E from the state.
* <code>reset(M, s)</code> measures, zeroes the two totals, and records E₀. It is called when you press Run.
* <code>rebase(M, s)</code> is the subtle one. When you move a slider *during* a run (say, doubling a spring's stiffness), the potential energy jumps instantly. That is not a violation of physics, it is you reaching in and changing the system. To avoid a false alarm, <code>rebase</code> sets <code>E0 = E - (workIn - dissipated)</code>, so that the residual is exactly zero right after the edit, and the books continue from there.

::: why
**Why rebase instead of ignoring the check while sliders move?** Because the check is only trustworthy when we keep it honest. After a rebase the residual is zero, and any later growth is a real warning.
:::

## advance: step and keep the books

@@src engine.hpp | ^// Step the simulation AND | ^\}$

* <code>double p0 = inputPower(...), d0 = 2.0 * rayleigh(...);</code> The input power and the heat power 2D at the **start** of the step.
* <code>rk4(M, s, ext, h);</code> Take the step.
* <code>double p1 = ..., d1 = ...;</code> The same two powers at the **end** of the step.
* <code>meter.workIn += 0.5 * (p0 + p1) * h;</code> and <code>meter.dissipated += 0.5 * (d0 + d1) * h;</code> The **trapezoid rule**: the area under the power curve over one step is the average of the two ends times the width. This is accurate to second order, so the books stay accurate.
* <code>meter.measure(M, s);</code> Update T, V and E from the new state.

This is the function the interface calls 1000 times per simulated second.

## dominantFrequency: the frequency finder

@@src engine.hpp | ^// -.*frequency finder | ^\}$

The function takes a list of samples and the sample rate, and returns the strongest frequency in hertz (or 0 if nothing stands out).

* <code>if (N &lt; 32) return 0;</code> Too few samples to say anything.
* The **mean** is computed and later subtracted, so a steady offset (sag) does not count.
* <code>for (double f = 0.05; f &lt;= std::fmin(30.0, sampleRate / 2.2); f += 0.025, ++count)</code> Try frequencies from 0.05 Hz up to 30 Hz in steps of 0.025 Hz. The upper limit is also capped at <code>sampleRate/2.2</code>: you cannot see a wiggle faster than half the sample rate (the **Nyquist limit**); 2.2 leaves a safety margin.
* The inner loop computes the real and imaginary parts of equation (4.4): <code>re += (x[i]-mean) * w * cos(ang)</code> and <code>im -= (x[i]-mean) * w * sin(ang)</code>, where <code>w</code> is the Hann window. Together <code>re</code> and <code>im</code> measure how much a cosine and a sine of that frequency match the data.
* <code>double mag = std::hypot(re, im);</code> The strength at this frequency.
* The best strength and its frequency are remembered; the sum <code>total</code> and <code>count</code> let us compute the **average** strength.
* **The last line:** <code>return (best &gt; 1e-6 &amp;&amp; count &gt; 0 &amp;&amp; best &gt; 4.0 * total / count) ? bestF : 0;</code> A real tone must stand out: its strength must be at least **four times the average** over all tested frequencies. A flat, noisy spectrum (no clear tone) returns 0 and the interface shows "--".

::: whatif
**What if there were no threshold?** The function would always return *some* frequency, even for a motionless system or pure noise, and the display would show a meaningless number. A measurement that can say "I don't know" is more honest than one that always answers.
:::

## Chapter checks

::: quiz
1. How many times does <code>rk4</code> call <code>accel</code>?
2. Why are <code>v2</code>, <code>v3</code>, <code>v4</code> saved?
3. What does <code>residual()</code> measure, and what value would a perfect run give?
4. Why is <code>rebase</code> needed?
5. What does the trapezoid rule do in <code>advance</code>?
6. What would <code>dominantFrequency</code> return for a motionless system?
:::

::: answers
1. Four times per step.
2. Because the position updates in stages 3 and 4 and in the final average use those trial velocities.
3. The gap between the measured energy and E₀ + work in − heat. Zero.
4. After a live edit the energy jumps for non-physical reasons; rebasing zeroes the residual so later growth is a real warning.
5. It averages the power at the start and end of the step to integrate it accurately.
6. 0 (the signal has no peak above the threshold).
:::

# Saving and Examples: model_io.hpp

The second file of the engine layer has two jobs: save and load a design to a text file, and provide four ready-made example designs.

## The file format

A design file is plain text, one thing per line. Here is what example 1 looks like when saved:

```text
vlab 1
# Vibration Lab design file
gravity 0
ground -3 0
mass 0 0 2 0 0 1 0 0 0
spring 0 1 30 0 2 0
damper 0 1 1.5
```

* The first word of a line says what it is. <code>mass</code> is followed by nine numbers (x, y, m, vx0, vy0, rail, driveF, driveAngle, driveW). <code>ground</code> has two. <code>spring</code> has k, k₃, L₀ and the autoRest flag; <code>damper</code> has c.
* Nodes appear in order, and links refer to them by **index** (0, 1, ...). So in this file the spring joins node 0 (the ground) and node 1 (the mass).
* A <code>#</code> starts a comment. The first line, <code>vlab 1</code>, is a **magic word with a version number**: the loader refuses files without it, so it cannot mistake some other text file for a design.

::: why
**Why a text format and not a binary one?** You can open the file in NOVA and read or edit it, send it by email, and compare two versions line by line. A binary file would be a few bytes smaller and impossible to inspect. For a few dozen numbers, text is the better trade.
:::

## saveModel

@@src model_io.hpp | ^inline bool saveModel | ^\}$

* <code>std::ofstream f(path); if (!f) return false;</code> Open the file for writing; if it cannot be opened (a locked folder, say), return <code>false</code> so the interface can show an error.
* <code>f.precision(9);</code> Write numbers with 9 significant digits, enough that loading gives back the same design to within rounding.
* <code>f &lt;&lt; "vlab 1\n# ..."</code> writes the header. <code>\n</code> is a newline.
* Then one loop over <code>M.nodes</code> writes a <code>mass</code> or <code>ground</code> line, and a second loop over <code>M.links</code> writes a <code>spring</code> or <code>damper</code> line. <code>(l.autoRest ? 1 : 0)</code> writes a boolean as 1 or 0.
* <code>return true;</code> when finished. The file closes automatically when <code>f</code> goes out of scope (a C++ feature called RAII: the object cleans up after itself).

## loadModel

@@src model_io.hpp | ^inline bool loadModel | ^\}$

Reading is harder than writing, because **files can be wrong**: the user may have edited them, or they may be damaged. So this function is defensive.

* <code>Model M;</code> Read into a **temporary** model, not the real one. The real model changes only on success, at the very end: <code>out = M;</code>. If anything goes wrong halfway, the user's current design is untouched.
* <code>while (std::getline(f, line))</code> Read one line at a time.
* <code>size_t h = line.find('#'); if (h != npos) line.erase(h);</code> Throw away comments.
* <code>std::istringstream ss(line); std::string word; if (!(ss &gt;&gt; word)) continue;</code> Make the line readable like a stream; read the first word; if the line is empty, skip it.
* The chain of <code>if (word == "gravity") ... else if (word == "mass") ...</code> chooses what to do. <code>ss &gt;&gt; n.x &gt;&gt; n.y ...</code> reads numbers in order. If a number is missing, that field keeps its default.
* <code>if (n.m &lt; 0.01) n.m = 0.01;</code> A mass of zero or less in a hand-edited file would make <code>accel</code> divide by zero; clamp it to a small positive value.
* <code>if ((int)M.nodes.size() &lt; MAX_NODES) M.nodes.push_back(n);</code> Respect the node limit.
* For links: <code>if (l.a &gt;= 0 &amp;&amp; l.b &gt;= 0 &amp;&amp; l.a &lt; n &amp;&amp; l.b &lt; n &amp;&amp; l.a != l.b) M.links.push_back(l);</code> **Validate the indices** against the number of nodes read so far. A link pointing at node 99 in a file with three nodes is silently dropped instead of crashing the program later.
* <code>if (!header) return false;</code> No <code>vlab</code> line means it is not our file.

::: whatif
**What if the loader trusted the file?** A file with <code>spring 0 99 ...</code> would create a link to a node that does not exist. The first time <code>accel</code> indexed <code>q[2*99]</code> the program would read memory beyond the array, giving garbage or a crash. **Never trust data that comes from outside your program.**
:::

## The four examples

@@src model_io.hpp | ^inline Model presetOneDof | ^\}

* Builds the classic single-mass system: <code>addGround(-3, 0)</code> (a wall 3 m to the left), <code>addMass(0, 0, 2.0)</code> (a 2 kg block), forced onto an X rail so it moves along a line.
* <code>addLink(Spring, g, m)</code> and <code>addLink(Damper, g, m)</code> join them in parallel.
* <code>M.links[s].k = 30; M.links[s].L0 = 2.0; M.links[s].autoRest = false;</code> The rest length is set to **2 m** while the drawn distance is **3 m**, so the spring starts **stretched by 1 m**. That is how this example "pulls the block aside and lets go" without needing a starting-velocity or a mouse. <code>autoRest = false</code> stops it from being reset to 3 m.
* <code>M.links[d].c = 1.5;</code> The damper.

This is exactly the system analysed in Chapter 3 Step 10: ω<sub>n</sub> = 3.873 rad/s, ζ = 0.0968.

@@src model_io.hpp | ^inline Model presetTwoDof | ^\}

* Two masses, two walls, three spring-damper pairs, built with a small loop over a table: <code>int ids[3][2] = {{g1, a}, {a, b}, {b, g2}};</code> lists which two nodes each pair joins, and the arrays <code>k[3]</code> and <code>c[3]</code> give their stiffness and damping. A table plus a loop is shorter, and easier to change, than six hand-written blocks.
* <code>M.nodes[a].vx0 = 3.0;</code> gives the first mass a push at the start.
{{img:vl-twodof.png|Example 2 after five seconds: two coupled masses. Each curve is a mixture of two vibration patterns, so the energy slowly passes back and forth between the masses.}}

* Two coupled masses have **two** natural frequencies (they can move together or against each other). In the telemetry you will see the motion is a mixture of both: a "beat" pattern.

@@src model_io.hpp | ^inline Model presetSpringPendulum | ^\}

* <code>M.gravity = 9.81;</code> Earth's gravity. One anchor on the "ceiling" at (0, −1.8) and one **free** mass (no rail) at (1.6, 0.6), joined by a spring with rest length 2 m. A mass on a spring that can both bounce and swing is a **spring pendulum**; its motion is rich and, for some starting positions, chaotic. It is the best example of why we used a general 2-D engine rather than a 1-D one.

@@src model_io.hpp | ^inline Model presetQuarterCar | ^\}

* The **quarter-car** model used in vehicle engineering: one wheel and one quarter of the car's body. The road is a ground anchor; the tyre is a spring (k = 220); the suspension is a spring (k = 70) plus a damper (c = 5) between the wheel (1 kg) and the body (4 kg). Both masses are on Y rails: they can only move up and down.
* <code>M.nodes[wheel].vy0 = -2.0;</code> Gives the wheel an upward speed of 2 m/s (negative y is up): the wheel has just hit a bump. Watch the body ride it out.
* With gravity switched on, the system first **sags** to its static equilibrium (Chapter 3, formula 3.18) before the bump response is visible.

@@src model_io.hpp | ^inline Model presetByNumber | ^\}

* A small **switch**: number 1 gives the first example, 2 the second, 3 the third, anything else the fourth. The buttons 1 to 4 and the keys 1 to 4 call this.

## Chapter checks

::: quiz
1. Why does <code>loadModel</code> read into a temporary <code>Model</code>?
2. What does the first word <code>vlab</code> protect against?
3. Why is <code>autoRest</code> set to <code>false</code> in example 1?
4. Which example needs a free (railless) mass, and why?
5. What bad thing could happen if <code>loadModel</code> did not check link indices?
:::

::: answers
1. So a failed or partial load leaves the user's current design untouched; the real model is replaced only on success.
2. Loading some other text file by mistake: without the header the loader returns false.
3. Otherwise the rest length would be reset to the drawn length (3 m) and the spring would start relaxed instead of stretched by 1 m.
4. Example 3 (the spring pendulum): it must swing in two dimensions.
5. The engine would later read outside its arrays: garbage numbers or a crash.
:::
