# SA_Rebuilt Runtime

Experimental Unreal Engine C++ gameplay systems developed for SA_Rebuilt by Tambov68.
This source edition is intended for learning, reuse and community contributions.
It contains the runtime plugin and a primitive demo, not the complete game or its asset library.

## Features

- BMX interaction: mounting, dismounting, ground recovery and collision-aware exit checks.
- Procedural bicycle movement, pedals, steering, hops, wheelies and rider IK targets.
- Vehicle interaction and ride camera components.
- Photo mode and a project-specific cinematic director.

## Requirements and installation

1. Install Unreal Engine 5.8 and its supported C++ build tools.
2. Create a C++ Unreal project, or use an existing compatible project.
3. Copy `Plugins/SA_BMXRuntime` into your project's `Plugins` directory.
4. Enable `SA_BMXRuntime` and `ChaosVehiclesPlugin` in your project.
5. Regenerate project files, build your Editor target and open the project.

DLSS is optional. Default builds use Unreal's TSR path in photo mode and do not
reference `DLSSBlueprint`. To opt in, install the matching NVIDIA DLSS plugin,
enable it explicitly in your host project's `.uproject`, set the environment
variable `SA_BMX_WITH_DLSS=1` in the process running UnrealBuildTool, and rebuild.
For example, in PowerShell: `$env:SA_BMX_WITH_DLSS = '1'` before invoking the build.
Unset the variable for a build without DLSS. Restart build processes that were
started before changing the environment; the value is a build-time option.

The plugin contains no binary assets. You must supply your own meshes, collision,
character, animation Blueprint and vehicle setup for a full integration.
A native primitive demo is included: see [Demo/README.md](Demo/README.md).

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

- Experimental, project-specific code; the included primitive demo has no animated rider or IK.
- Rider animation integration uses named properties and reflection.
- Vehicle interaction has fallback references to `/Game/Vehicles/F40/Animation/...`.
  Those animations are not distributed; assign your own animations through the component properties.
- The cinematic director assumes actors and staging from the original project.
- Bicycle dimensions, support height and rider offsets are tuned for the original BMX;
  adapting a differently sized model requires checking these values and contacts.
- Multiplayer, packaging and compatibility outside UE 5.8 are not verified.
- The source edition includes optional DLSS integration and native demo classes absent from the private plugin.
- Clean UE 5.8 `BuildPlugin` validation without DLSS: Editor Win64 Development,
  UnrealGame Win64 Development and UnrealGame Win64 Shipping. Interactive demo PIE,
  a complete packaged host game, and the DLSS-enabled configuration are not yet verified.

## Useful first contributions

- [ ] Extend the primitive demo with a clearly licensed animated rider and documented AnimBP/IK setup.
- [ ] Validate both DLSS build configurations and the TSR photo-mode fallback in a clean host project.
- [ ] Replace fixed component names and F40 asset paths with configurable references.
- [ ] Document sockets, IK spaces and animation parameters with an integration example.
- [ ] Add focused regressions for blocked dismount, slope support and input transitions.
- [ ] Verify a clean packaged build and document supported configurations.

See [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.

## License and asset boundary

The MIT license applies only to the original source files in this repository.
Unreal Engine and NVIDIA DLSS remain governed by their own licenses and must be obtained separately.
No Unreal Engine source, third-party plugin implementation, marketplace asset, GTA/San Andreas
asset, model, texture, animation, map, private project history or local configuration is included.
The complete private SA_Rebuilt repository is not a public distribution source.

SA_Rebuilt is an independent project; this source edition does not claim affiliation
with Rockstar Games, Take-Two Interactive, Epic Games or NVIDIA.

---

Русский: это исходный код собственного игрового плагина, а не готовая игра.
Полная рабочая версия и сторонние ассеты остаются приватными. Для запуска потребуются
собственный проект UE 5.8 и настройка персонажа/Blueprint. DLSS необязателен.
