# SA_Rebuilt Runtime

Experimental Unreal Engine C++ gameplay systems developed for SA_Rebuilt by Tambov68.
This source edition is intended for learning, reuse and community contributions.
It contains the runtime plugin, not the complete game or its asset library.

## Features

- BMX interaction: mounting, dismounting, ground recovery and collision-aware exit checks.
- Procedural bicycle movement, pedals, steering, hops, wheelies and rider IK targets.
- Vehicle interaction and ride camera components.
- Photo mode and a project-specific cinematic director.

## Requirements and installation

1. Install Unreal Engine 5.8 and its supported C++ build tools.
2. Create a C++ Unreal project, or use an existing compatible project.
3. Obtain the NVIDIA DLSS plugin for your engine version from NVIDIA and install it separately.
   DLSS is currently a required build dependency through `DLSSBlueprint`.
4. Copy `Plugins/SA_BMXRuntime` into your project's `Plugins` directory.
5. Enable `SA_BMXRuntime`, `ChaosVehiclesPlugin` and `DLSS` in your project.
6. Regenerate project files, build your Editor target and open the project.

The plugin contains no binary assets. You must supply your own meshes, collision,
character, animation Blueprint and vehicle setup. Compilation alone does not create a playable demo.

## BMX integration contract

The implementation currently finds components by exact names:
`BMX_Frame`, `BMX_SteeringPivot`, `BMX_Steering`, `BMX_Crank`,
`BMX_Pedal_L`, `BMX_Pedal_R`, `BMX_Wheel_F`, `BMX_Wheel_R`.
The bicycle uses StaticMesh components rather than a bicycle skeletal mesh.

Add `UBMXVehicleComponent` to the bicycle actor and `UBMXInteractionComponent`
to your character. Set `RiderAnimClass` and provide the sockets required by
`TryMount` and the rider IK update. `Socket_Seat` must be on the frame;
hand sockets must follow the steering component. See `BMXInteractionComponent.cpp`
for the current socket names and animation parameter contract.

Default input includes F to interact, W/S to move, A/D to steer, Space to hop,
Left Alt to brake, Left Control for wheelie, Left Shift for sprint/stoppie,
and mouse axes for looking. The implementation has state-dependent input behavior.

## Current limitations

- Experimental, project-specific code; no standalone sample scene is included yet.
- Rider animation integration uses named properties and reflection.
- Vehicle interaction has fallback references to `/Game/Vehicles/F40/Animation/...`.
  Those animations are not distributed; assign your own animations through the component properties.
- The cinematic director assumes actors and staging from the original project.
- Multiplayer, packaging and compatibility outside UE 5.8 are not verified.
- This source snapshot is byte-identical to the private project's plugin.
  The public package has not received a fresh independent build or PIE test.

## Useful first contributions

- [ ] Add a minimal sample using original primitive meshes and a documented character/AnimBP setup.
- [ ] Make DLSS optional, with a working non-DLSS path.
- [ ] Replace fixed component names and F40 asset paths with configurable references.
- [ ] Document sockets, IK spaces and animation parameters with an integration example.
- [ ] Add focused regressions for blocked dismount, slope support and input transitions.
- [ ] Verify a clean packaged build and document supported configurations.

See [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

## License and asset boundary

The proposed MIT license applies only to the original source files in this repository.
Unreal Engine and NVIDIA DLSS remain governed by their own licenses and must be obtained separately.
No Unreal Engine source, third-party plugin implementation, marketplace asset, GTA/San Andreas
asset, model, texture, animation, map, private project history or local configuration is included.
The complete private SA_Rebuilt repository is not a public distribution source.

SA_Rebuilt is an independent project; this source edition does not claim affiliation
with Rockstar Games, Take-Two Interactive, Epic Games or NVIDIA.

---

Русский: это исходный код собственного игрового плагина, а не готовая игра.
Полная рабочая версия и сторонние ассеты остаются приватными. Для запуска потребуются
собственный проект UE 5.8, отдельно установленный DLSS и настройка персонажа/Blueprint.
