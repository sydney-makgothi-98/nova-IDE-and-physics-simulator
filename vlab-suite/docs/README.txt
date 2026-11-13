VIBRATION LAB SUITE 1.0
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
