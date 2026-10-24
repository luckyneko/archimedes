# Archimedes

A Vulkan renderer library: the words its API, docs and tests use for the things it renders with.

## Language

### Devices and drivers

**Hardware device**:
An `acm::Device` created on a GPU (a discrete, integrated or virtual GPU).
_Avoid_: GPU device, real device

**CPU device**:
An `acm::Device` created on a Vulkan implementation that runs on the CPU. It is a pathway a caller can choose, not only a substitute for a missing GPU.
_Avoid_: fallback, software renderer, CPU backend, SwiftShader device

**CPU driver**:
The Vulkan driver behind a CPU device. SwiftShader is archimedes' CPU driver; its name is not a synonym for the concept.
_Avoid_: software driver, SwiftShader (as the general term)

**Staged driver**:
A Vulkan driver that archimedes copies beside a runnable binary and points the loader at, as opposed to one installed on the system. MoltenVK and the CPU driver are staged.
_Avoid_: vendored driver, bundled ICD
