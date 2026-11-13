# Running the Model

## How long to run

The **Run** panel starts with the question *How long should the model run?* Type any number of seconds from a fraction of a second to an hour, or press **5 s, 10 s, 30 s, 1 min, 5 min**. The model stops by itself when the time is up.

Underneath, **Data rows per second** sets how often the program records a row of data. Higher rates give smoother data and bigger Excel files. A good starting point is 100 rows per second; for a 5-minute run use 20 to 50. The panel tells you how many rows to expect. (A workbook can hold a little over one million rows, so very long runs are automatically thinned to fit.)

## Two ways to run

| Button | What happens | Use it when |
|--------|--------------|-------------|
| **Run** | Plays in real time. You can pause, drag bodies with the mouse, and change sliders while it runs. | You want to watch and play with it. |
| **Compute the whole run now (fast)** | Runs the full duration as fast as the computer can and shows all the data at once. | You only need the numbers: a 5-minute run takes about a second. |

Other controls: **Pause/Resume** (Space), **Stop** (Esc) returns to design mode, **Step 1/60 s** advances one frame while paused, and the **real-time speed** slider runs from 0.1x (slow motion) to 4x.

{{img:suite-live.png|A run in progress. The state, time and mode are shown at the top of the canvas; the spring colours show how hard each one pulls (warm) or pushes (cool).}}

::: tip
**Push things around.** While a run is playing, press the mouse on a moving body and drag. A soft invisible rubber band pulls it toward the cursor, and the energy you put in appears in the ledger as "work in". Let go and watch how the system recovers.
:::

## Changing things while it runs

Sliders and number fields in **Properties** and **Environment** (stiffness, damping, friction, mass, gravity, even ideal/realistic) take effect immediately in a running model. Changes to the *layout* (adding, deleting, moving things) need you to press **Stop** first. Press **Stop** and your data from the last run is still there to plot and export.

## Recording a video

Press **Record video (F9)** at any time, press it again to finish. The video shows the canvas (or the whole window if you tick the box) and is saved as an **.mp4** in <code>Documents\Vibration Lab Suite\Recordings</code>.

While recording, every video frame is exactly 1/60 of a second of physics, so the video plays at the correct speed even if your computer was busy while recording. The program uses the video encoder that is already part of Windows; nothing else needs installing.

# Telemetry and Excel

## What is recorded

For every moving body, at every sample: its position (x, y), its **displacement** from the start (dx, dy), angle, velocity (vx, vy), speed, spin and kinetic energy. For every spring: length, stretch and force. For every damper: length, stretching rate, force and the heat power it is producing. For the whole system: kinetic, potential and total energy, the energy ledger, and the number of contacts.

## In the program

The **Telemetry** panel has seven tabs:

* **Plots** - tick any channels and plot them against time. Scroll to zoom, drag to pan, double-click to fit. Quick-pick buttons choose positions, velocities, energy, forces or angles.
* **Energy ledger** - where every joule went (see the previous chapter).
* **Path and phase** - the path of a body on the canvas, and its phase portrait (velocity against displacement).
* **Spectrum** - the frequency content of any signal.
* **Statistics** - minimum, maximum, mean, RMS and final value of every channel.
* **Modal analysis** - natural frequencies and mode shapes.
* **Frequency response** - a resonance curve.

{{img:suite-bounce.png|The telemetry of a bouncing ball in a realistic environment: the displacement plot shows each bounce a little lower than the last.}}

## Exporting to Excel

Press **Export to Excel (.xlsx) with charts** (or Ctrl+E), choose a name and a folder. The workbook is a normal Excel file with these sheets:

| Sheet | What is in it |
|-------|---------------|
| **Summary** | Your whole setup in words and numbers (so the report explains itself): run settings, environment, every body with its shape, mass and material, every connection, the final energy ledger, and statistics of every channel. |
| **Telemetry** | One row per sample, one column per channel, with units in the headings. The first row is frozen so the headings stay visible. |
| **Charts** | Ready-made charts: displacement, velocity, rotation angle, energy, the energy ledger, forces and damper power. For runs longer than 20 seconds, an extra chart shows the first 10 seconds in detail. |
| Spectrum, Modal, FrequencyResponse | Included automatically if you computed them before exporting. |

{{img:suite-xl-summary.png|The Summary sheet of the ramp experiment, as it opens in Microsoft Excel.}}

{{img:suite-xl-telemetry.png|The Telemetry sheet: every sample of every channel, with units.}}

The charts are real Excel charts that stay linked to the Telemetry sheet. Edit the data and they update; right-click to restyle them.

{{img:suite-xl-chart3.png|One of the charts Excel shows straight away: the rotation angle of the four blocks on the ramp. The wood block tips as it hits the wall.}}

### Making your own chart

On the Telemetry sheet, select the **Time** column and the channel you want (hold Ctrl to select non-adjacent columns), then **Insert, Scatter with Straight Lines**. Every channel is a column; every row is a moment.

### CSV

**Export CSV** makes a plain table that any program can read (Excel, Python, MATLAB, R). The file starts with a marker so that Excel shows the units correctly.

::: tip
**For homework and lab reports:** export the workbook, copy the charts you need into your report, and keep the Summary sheet: it records exactly which design produced the numbers.
:::

# Worked Tutorials

## 1. Natural frequency and damping of a mass on a spring

Load **Mass-spring-damper (1 degree of freedom)** (mass 2 kg, spring 30 N/m, damper 1.5 N s/m, released from 1 m stretch).

1. Press **Compute the whole run now (fast)**.
2. Open **Telemetry, Spectrum**, choose the channel **Mass dx**, press **Compute spectrum**. The peak reads **0.61 Hz**; the tab also prints the damping ratio estimated from the decay: **0.097**.
3. Theory: natural frequency sqrt(k/m) / (2 pi) = sqrt(15) / (2 pi) = 0.616 Hz. Damping ratio = c / (2 sqrt(k m)) = 1.5 / (2 sqrt(60)) = 0.097.
4. Now double the spring to 60 N/m in Properties and repeat: the frequency should rise by a factor of about 1.41.

## 2. Resonance

Same system, switched to **zero gravity**. Select the mass and set a **harmonic push**: 5 N, direction 0, frequency **3.9 rad/s** (the natural frequency). Run for 30 seconds and look at **Plots** and the **Energy ledger**: the amplitude grows until the energy fed in each cycle equals the heat the damper removes. Now set the push frequency to 8 rad/s: the same push barely moves it. Switch dampers off (press **Ideal**) at resonance and the amplitude grows without limit.

## 3. Which blocks slide?

Load **Friction on an incline: four materials**. The ramp is 28 degrees, so tan(28 degrees) = 0.53. A block slides only when the static friction between it and the ramp is below 0.53. With a wooden ramp the combined values are rubber 0.71, steel 0.61, wood 0.50, ice 0.22, so rubber and steel stay and wood and ice slide. Run it and confirm, then check the **Energy ledger**: the "friction" bar is the heat from the two sliding blocks.

## 4. Bounce and restitution

Load **Bouncing ball: ideal vs realistic**. A rubber ball (e = 0.8) dropped from 2 m should rebound to e squared x 2 = 1.28 m, then 0.82 m, and so on. Read the peaks from the **Plots** tab (channel **Ball dy**) and compare. The ledger's **impacts** bar after the first bounce is m g h (1 - e squared) = 3.5 J for the default 0.5 kg ball.

## 5. A tuned mass damper

Load **Tuned mass damper**: a 10 kg main mass on a spring, pushed at its resonance, with a small 1 kg absorber attached by its own spring and damper.

1. Run 40 seconds and look at **Plots**: the main mass moves very little because the absorber takes the motion.
2. Open **Frequency response** and press **Run frequency sweep**: instead of one tall resonance peak you see two small ones either side. That split peak is the signature of a tuned absorber.
3. Open **Modal analysis** and press the button: the two natural frequencies match the two peaks.

{{img:suite-frf.png|Frequency response of the tuned mass damper: two modest peaks instead of one tall one. The vertical lines mark the natural frequencies from the modal analysis.}}
