# Vibration Lab for Unreal Engine 5 (plugin, beta scaffold)

The plugin runs the **same engine** as the Vibration Lab Suite desktop application (springs, dampers, rigid bodies,
friction, collisions, energy ledger, telemetry). Unreal draws the result; the motion itself is computed by the engine,
which is checked against closed-form solutions by `vlab_tests.exe` (see `docs/validation_report.txt`).

> **Status, honestly:** this plugin was written against the Unreal 5 API but **has not been compiled** (no Unreal Engine was
> installed on the machine that built it). The engine code it wraps is built and tested; the Unreal glue
> (`VLabSystemActor`, `VLabSystemAsset`, `VLabRuntime.Build.cs`) is small and should need only minor fixes on first build.

## Install

1. Copy the folder `VLab` into your project's `Plugins` folder (or the engine's `Engine/Plugins`).
2. Right-click your `.uproject`, *Generate Visual Studio project files*, open it, build.
3. In the editor: *Edit > Plugins > Physics > Vibration Lab* should be enabled.

## Use

1. In the desktop app, design a system and **File > Save as...** (a `.vlab` file).
2. In Unreal: *Add > Miscellaneous > Data Asset > VLab System Asset*. Open it and press **Load From File**
   (or paste the file's text into *Vlab Text*).
3. Place a **VLab System Actor** in the level, set its *System* property, press **Play**.
4. Blueprint nodes: *Start / Stop / Reset Simulation*, *Set Ideal Physics*, *Set Gravity*, *Get Body Location*,
   *Get Body Speed*, *Get Total Energy*, *Get Energy Lost*, *Save Telemetry Csv* (opens in Excel).

The 2-D world lives on a plane in the level. By default the simulation's x axis maps to Unreal's Y axis and "up"
to Unreal's Z. Change *Plane Right* and *Plane Up* to place it on any plane. The simulation's y axis points down, so
"up" is minus y.

## What is in the box

| Path | What it is |
|------|------------|
| `Source/VLabRuntime/ThirdParty/` | the engine core, copied from `core/` by `sync_core.py` |
| `Source/VLabRuntime/Public`, `Private` | the Unreal glue: actor, data asset, module |
| `VLab.uplugin` | the plugin descriptor |

## Known limits and the roadmap

* **Two physics worlds.** The engine does not collide with Unreal's own (Chaos) objects. The planned *Chaos bridge*
  feeds Chaos bodies in as kinematic inputs (one-way) and, later, applies the engine's forces back to them (two-way).
* Visual meshes are basic engine shapes (sphere, cube); polygons are drawn as their bounding box. Assign your own meshes
  per body in a later version.
* Exceptions are enabled for this module (the engine's file parser uses `try/catch`); the module rules say so.
* Sign of the rotation depends on the plane you choose: use *Flip Rotation* if a body spins the wrong way.
* For Fab: add screenshots and a demo map, test on each engine version you list, include the third-party notices.
