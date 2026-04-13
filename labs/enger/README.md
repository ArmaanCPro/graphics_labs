# Requirements
- Vulkan 1.4
- Some other extensions, so a relatively modern (Turing+) GPU is pretty much required.

# Notes

- Genuinely happy with how the resource management system turned out to be.
    - The fact that the Pool (holder of GPU resources) could be used for the Bindless descriptor index is huge
- Bindless overall is pretty cool and fits so tightly that the API is easier than the bindful way (for `enger`).
