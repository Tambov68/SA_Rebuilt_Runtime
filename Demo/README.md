# BMX demo with Manny

The host project includes our black/gold BMX and Epic Games' template Manny.
See [ASSET_NOTICES.md](ASSET_NOTICES.md) for authorship and license boundaries.
The runtime also retains a primitive fallback for hosts without those assets.

## Included host project

Clone with Git LFS installed (`git lfs pull` if needed), then open
`Demo/SA_RuntimeDemo.uproject` in UE 5.8. The plugin source lives in `../Plugins`,
referenced through `AdditionalPluginDirectories`. Allow Unreal to compile the
plugin using your installed C++ toolchain. The saved demo map and GameMode are
already selected. Launch PIE yourself in the viewport.

## Run it in your own host project

1. Install and build the plugin using the main README.
2. Create a blank **Basic** level in your C++ host project.
3. Remove its default floor and lights: the demo GameMode creates a floor and directional light.
4. Add a Player Start at `(0, 0, 100)` centimetres, facing positive X.
5. In World Settings, set **GameMode Override** to **SARuntimeDemoMode**.
6. Save the level and start PIE in your chosen viewport.
7. Walk toward the bicycle, then press F to mount.

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

The included host uses Manny's reference pose, without rider animation or IK.
Do not expect the hands and feet to grip the bicycle yet. The demo uses real
runtime interaction and movement; it is an integration starting point, not a
physically realistic bicycle simulator. Without the configured rider/bike assets,
the runtime falls back to a box character and primitive bicycle.

Hosts with their own compatible rider AnimBP can configure
`DemoRiderAnimClass=/Game/YourAnimBP.YourAnimBP_C` under
`[/Script/SA_BMXRuntime.SADemoCharacter]` in `Config/DefaultGame.ini`.
The animation class must use the selected mesh's skeleton and expose the named
IK parameters expected by `UpdateRiderIK`. Third-party animation packages are
not bundled here. The bicycle's collision ignores the Camera trace channel,
so it does not retract the demo spring arm into the rider; world obstacles
still participate in camera collision.

`DemoRiderAnimClass` is used only while mounted. Hosts can also configure
`DemoIdleAnimation` and `DemoWalkAnimation` in the same section to supply compatible
on-foot clips. The demo selects walk from horizontal speed, restores on-foot
playback after dismount, and does not implement a full locomotion blend tree.

The code uses transient copies of engine primitive meshes to attach sockets;
the installed Engine assets are never saved or modified. Editor/Game compilation
results are recorded in the main README. Interactive PIE behavior must still be
checked by a user; do not treat a successful build as a gameplay test.
