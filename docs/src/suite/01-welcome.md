@@unnumbered
# Welcome

## What Vibration Lab Suite does

**Vibration Lab Suite** is a sandbox for mechanical motion. You draw a system on a canvas: balls, blocks and polygons, floor lines and ramps with their own friction, beams with pinned, roller or cantilever supports, springs, dampers, rods and hinges with motors. You choose how realistic the physics should be. Then you run it for as long as you like and watch it move, with live numbers and plots beside it.

When you are done, one button turns the whole run into an **Excel workbook**: every number, with units, plus charts that are already drawn. A second button records an **.mp4 video**.

It is built for three kinds of people:

* **Students** of dynamics and vibrations, who want to *see* what the equations say and check their homework against a simulation.
* **Engineers** who want a quick what-if model before they commit to heavy software.
* **Teachers and developers** who want convincing, correct motion for demonstrations, courses and games.

## Installing

There is nothing to install. Double-click **VibrationLabSuite.exe**. The program opens straight into the design screen with a worked example loaded.

::: note
You need Windows 10 or 11 (64-bit) and a graphics card that supports Direct3D 11 (every PC of the last ten years does). Excel is **not** needed to run the program or to create the workbook. You only need Excel (or LibreOffice, Google Sheets, Numbers) to *open* the workbook afterwards.
:::

Your files are kept in <code>Documents\Vibration Lab Suite\</code>:

| Folder | What goes there |
|--------|-----------------|
| <code>Designs</code> | Systems you save (<code>.vlab</code> files) |
| <code>Exports</code> | Excel workbooks and CSV files |
| <code>Recordings</code> | Videos |

## The five-minute tour

{{img:suite-welcome.png|The first time you start the program it explains itself. You can reopen this window any time from Help, Quick start.}}

The screen is divided into panels that you can drag, resize and rearrange.

{{img:suite-design.png|The design screen. Library (left): shapes and tools. Simulation (centre): the canvas. Run (right, top): how long, run, export. Properties and Environment (right, bottom). Telemetry (bottom): plots and analysis.}}

| Panel | What it is for |
|-------|----------------|
| **Library** | Pick a shape or connection, then click the canvas. Also holds the settings for the next object and the example list. |
| **Simulation** | The canvas. Wheel zooms, right-drag pans. In a run, drag a moving body to push it. |
| **Run** | Type how many seconds to run, press Run, then export. |
| **Properties** | Everything about the selected body or connection: size, radius, mass, material, friction, starting motion, stiffness, damping. |
| **Environment** | Ideal or Realistic physics, each effect on or off, gravity. |
| **Telemetry** | Plots, the energy ledger, paths, spectrum, statistics, modal analysis and frequency response. |

## Your first run in one minute

1. Look at the example already loaded: a mass joined to a wall by a spring and a damper. The spring is stretched by 1 metre.
2. In the **Run** panel press **Run**. The mass swings back and forth and settles.
3. Open the **Telemetry** panel. Tick a few channels (try **dx** and **vx**) and watch them draw themselves.
4. Press **Export to Excel (.xlsx) with charts**. Choose where to save it. Open the file in Excel: the Charts sheet shows the run.

::: try
Change the number in "How long should the model run?" to 30, press **Compute the whole run now (fast)**, and export again. The workbook now holds thirty seconds of data and an extra "first 10 seconds" chart so the early oscillation is not squashed.
:::
