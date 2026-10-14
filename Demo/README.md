# Primitive BMX demo

This is a native runtime scene assembled from Unreal Engine's installed basic
shapes. No game assets or engine assets are copied into this repository.

## Run it in your own host project

1. Install and build the plugin using the main README.
2. Create a blank **Basic** level in your C++ host project.
3. Remove its default floor and lights: the demo GameMode creates a floor and directional light.
4. Add a Player Start at `(0, 0, 100)` centimetres, facing positive X.
5. In World Settings, set **GameMode Override** to **SARuntimeDemoMode**.
6. Save the level and start PIE in your chosen viewport.
7. Walk toward the primitive bicycle, then press F to mount.

The GameMode spawns the floor and bicycle at runtime. Stop PIE to remove those
runtime actors. No map or private Blueprint is needed from SA_Rebuilt.

## Controls

| Input | Action |
|---|---|
| W / S | Forward / backward |
| A / D | Steer when riding; walk sideways on foot |
| Mouse | Look |
| F | Mount / dismount |
| Hold then release Space | Charge / release hop |
| Left Alt | Brake |
| Left Control | Wheelie |
| Left Shift | Sprint; interacts with brake/stoppie behavior |

The screen shows the interaction status, including failed mount/exit checks.
Use the default third-person demo view. First-person V relies on an animated
rider's head/neck and is not part of this placeholder demonstration.

## Scope and limits

The rider is a simple box, with no skeletal mesh or IK animation. The demo uses
the real runtime interaction and movement component, but simplified collision,
solid disc wheels, and no art/material quality demonstration. It is an integration
starting point, not a physically realistic bicycle simulator.

The code uses transient copies of engine primitive meshes to attach sockets;
the installed Engine assets are never saved or modified. Editor/Game compilation
results are recorded in the main README. Interactive PIE behavior must still be
checked by a user; do not treat a successful build as a gameplay test.
