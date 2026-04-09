#include "Resources.h"

#include "vulkan/Device.h"

namespace enger
{
    void destroy(Device* device, ComputePipelineHandle handle)
    {
        if (device)
        {
            device->destroyComputePipeline(handle);
        }
    }

    void destroy(Device *device, TextureHandle handle)
    {
        if (device)
        {
            device->destroyTexture(handle);
        }
    }
}
