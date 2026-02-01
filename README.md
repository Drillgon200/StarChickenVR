# Star Chicken VR
This will one day be a sci-fi themed game, but for now is just engine tech.

This project is designed to build and run with Visual Studio 2022, though I imagine other versions would work.

When running the project, choose the "DebugEditor" run configuration. Other configurations will attempt to run in VR mode, which is currently broken as I work on other features.

When running for the first time, the project will throw an access violation exception. This is intended, as Star Chicken makes use of growable arena data structures that only commit memory when first accessed. Simply check the box to ignore this exception when coming from StarChickenVR and hit continue.

### Editor Controls
- Left click to select, left click and drag to box select, shift to select multiple
- Middle Mouse + Left Click to enter fly mode
- Esc to exit fly mode
- WASD/Space/Shift for flight, LCtrl for faster move speed
- V for vertical panel split, H for horizontal
- File->Cubemap Gen to generate filtered cubemaps from an equirectangular image for PBR image based lighting. Currently takes in a user selected file with a raw array of 1024x512 floats and writes the output to the cubemap_test directory if it exists. This will be changed to a better interface in the future.

### Notable Features
- Vulkan PBR Renderer, based on the OpenPBR standard
- Custom shader language (DSL), allowing for faster compile times, better integration into the engine, and better ergonomics for shader development
- UI system used for the editor interface
- MSDF font rendering + the ability to generate MSDF images from vector graphics
- Skeletal skinning for deformable animation
- Physics solver based on Augmented Vertex Block Descent (WIP, currently only supports 3d points and distance constraints)
- OpenXR integration for VR rendering

I will at some point write a separate readme describing the somewhat unconventional project structure and code style, as well as the high level engine architecture and render path.

VR mode is currently unsupported as I continue to build the editor and basic rendering features, but will be worked on again as soon as those are in a good state.

Star Chicken is currently designed for Windows only, though Linux support is planned.