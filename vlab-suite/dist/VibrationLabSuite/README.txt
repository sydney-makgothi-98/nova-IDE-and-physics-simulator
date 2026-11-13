VL LAB MECHANICS SUITE 1.2
A dynamics and vibrations sandbox, by Team NOVA
==================================================

START
-----
Double-click VibrationLabSuite.exe. That is all: no installer, nothing else to download.
The program opens straight into the design screen with a worked example loaded.
(Windows 10 or 11, 64-bit. A graphics card with Direct3D 11 support, which every PC from the last ten years has.)

THE FIVE-MINUTE TOUR
--------------------
1. DESIGN    Left panel (Library) is a navbar. Click Objects (Ball, Block, Polygon, Wedge, Beam), Ground and supports
             (Floor line, Anchor, Pin / Fixed / Roller support) or Connections (Spring, Damper, Rod, Pin, Weld) and pick
             from the drop-down, then click the canvas. Q / E (or Ctrl + wheel) rotate the object before you place it.
             A floor is a hatched LINE with zero thickness but with a material (friction, bounce). A beam falls unless it
             is hinged or supported; a Fixed support is a cantilever. Shift + click keeps the tool.
             Examples opens Basic, Intermediate and Complex example lists (gantry crane, robotic arm, ...).
             Right panel (Properties): select anything with the Select tool to set its size, radius, mass, material,
             friction, bounciness, starting speed, a harmonic push, spring stiffness, damping, hinge motors and so on.
             Click a support while running to read its reaction force and moment.

2. PHYSICS   Environment panel: press Ideal (frictionless, perfectly elastic, no damping) or Realistic (friction and
             energy loss from the materials), or switch each effect on and off separately. Set gravity.

3. RUN       Run panel: type how many seconds you want (or press 5 s / 10 s / 30 s / 1 min / 5 min).
             Run     plays it in real time. Drag any moving body with the mouse to push it.
             "Compute the whole run now" finishes the whole run instantly and shows all the data.

4. READ      Telemetry panel (bottom): Plots (tick any channels), Energy ledger (where every joule went),
             Path and phase, Spectrum (frequency content), Statistics, Modal analysis (natural frequencies and
             mode shapes) and Frequency response (resonance curve).

   NEW IN 1.2
             Deformation   beams bend and compact bodies squash under load. Use "Show deformation" on the canvas (top right):
                           Auto picks an exaggeration so the bending is visible, or choose True / x10 ... x100k. "Realistic"
                           makes a block sink with the beam and its underside follow the bent surface. The Deformation tab in
                           Telemetry plots every body's deflection, contact force, the bent shape at any instant, the bending
                           stress and span / deflection. Set the stiffness E in Properties > Material.
             Examples      five new Complex examples: car suspension, railgun, truck on a bridge, crank-slider engine,
                           pendulum and domino chain. A body's push can now be a single pulse (Properties > Pulse length).
             Layout        one sidebar on the left: the navbar on top, then Run / Properties / Environment as tabs under it, so the
                           canvas gets the whole width. The names that floated over bodies are now a legend at the bottom left.
             Dashboard     the front page: a cover video, the design you are working on and a card for every saved project.
                           To use your own cover video press "Choose cover video..." (an H.264 .mp4); it is copied to
                           Documents\Vibration Lab Suite\dashboard_cover.mp4. You can also drop dashboard_cover.mp4 next to the exe.
             Loads         select a moving object: Properties > Apply a load (push or pull, newtons with a linked kilograms field, any
                           direction), and Distributed load for a box or beam: uniform, triangular (three kinds) or your own equation q(x)
                           with trig, polynomial, exp, sqrt ... (x = distance from the left end, L = length, t = time). Optional Dead load
                           (the beam's own weight per metre) is a checkbox on the canvas; it is off unless you switch it on.
             Sections      Properties > Cross-section: rectangle, circle, hollow box, tube, I-beam, T-beam, channel. It gives the area, the
                           second moment of area I, the section modulus Z and the neutral axis, and the beam is as stiff as that I says.
             Diagrams      Telemetry > Beam diagrams: load, shear force, bending moment and deflection along the beam at any instant, and
                           Cross-section stress: bending, axial and shear stress through the depth at any position along the beam.
             Temperature   Properties > Temperature (and the Environment panel for the whole scene): a temperature change makes a beam
                           grow (free end) or carry stress -E alpha dT (held at both ends); a hotter lower face bows it by alpha dT L^2 / 8 h.
             Sidebar       one column on the left, in headed groups: Dashboard and Workspace; DESIGN TOOLS (select, objects, ground and
                           supports, connections, examples); SETTINGS (Physical settings, Object properties, Simulation setup, Grid
                           settings, Data extraction: each opens in place under its button); VIEW (light or dark). The open design and
                           Save sit at the bottom. Run, Pause, Stop, Step and Compute are along the top of the canvas.
             Connections   springs, dampers, rods and joints attach where you choose: click a body near its edge and the point lands ON the
                           edge. A small box (appears when you pick a connection tool) sets where it attaches (Edge, Key points = corners /
                           middle / thirds, Centre, Exact) and which way it runs (Free, Perpendicular to the face, Horizontal, Vertical or
                           an angle), with a live preview. Spring on the left and damper on the right of a mass, like a textbook figure.
             Schematic     Data > Schematic diagram saves the design as a labelled figure (SVG): outlined shapes, spring and damper symbols,
                           supports, load arrows, and every part named with a leader line. Opens in a browser, Word or PowerPoint.
             Theme         View > Light theme / Dark theme, or the button under the logo. The choice is remembered.
             Graphs        steady axes (no flicker) and the Spectrum / Plots tabs open on a signal that actually moves.

5. EXPORT    "Export to Excel (.xlsx) with charts" creates a workbook:
               Summary    your whole setup, the energy ledger and statistics of every channel
               Telemetry  every sample of every channel, with units
               Charts     position, velocity, energy, energy ledger, forces - already plotted
               (+ Spectrum, Modal and FrequencyResponse sheets when you computed them)
             "Export CSV" makes a plain table. "Record video (F9)" saves an .mp4 of the canvas.

FILES
-----
Your work is saved in  Documents\Vibration Lab Suite\   (Designs, Exports, Recordings).
Save a design with Ctrl+S; reopen with Ctrl+O. The Examples folder next to this file has 11 ready-made designs.

KEYS
----
Space run/pause   Esc stop   F9 record   Ctrl+S save   Ctrl+O open   Ctrl+E export   Ctrl+Z / Ctrl+Y undo / redo
V select  C ball  B block  P polygon  W wedge  T beam  F floor  A anchor  S spring  D damper  R rod  J pin
1 2 3 pin / fixed / roller support   Q E rotate while placing   Home fit view
Mouse wheel zooms, right or middle button drag pans, hold Alt to ignore the grid.

TOOLS IN THE Tools FOLDER
-------------------------
vlab_cli.exe    runs a saved design without a window:  vlab_cli --file my.vlab --duration 60 --out result.xlsx
vlab_tests.exe  the validation suite: checks the physics against closed-form answers (see Documents\validation_report.txt)

HONEST LIMITS
-------------
* 2-D: bodies move in a plane and have a depth of 1 m.
* Material values (friction, bounce) are generic textbook numbers. Measure your own surfaces for engineering decisions.
* The solver is validated against theory for springs, dampers, pendulums, friction, rolling, restitution and collisions.
  It is not a certified engineering tool.
* If numbers blow up (a very light body on a very stiff spring) the program stops and says so: soften the spring or add mass.

UNREAL ENGINE 5
---------------
The folder UnrealPlugin contains a plugin that runs the same engine inside Unreal. It is a beta scaffold: see its README.

Re-orienting a connection: select a placed spring, damper or rod, then in Object properties use Make it vertical / Make it horizontal / Set this angle (choose which end stays fixed), or edit the end coordinates. While a connection tool is active, the box on the canvas sets Direction (Free / Perpendicular / Horizontal / Vertical / Angle) before you place it.
