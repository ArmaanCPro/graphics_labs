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

    void destroy(Device *device, PipelineLayoutHandle handle)
    {
        if (device)
        {
            device->destroyPipelineLayout(handle);
        }
    }

    void destroy(Device *device, TextureHandle handle)
    {
        if (device)
        {
            device->destroyTexture(handle);
        }
    }

    void destroy(Device *device, DescriptorSetLayoutHandle handle)
    {
        if (device)
        {
            device->destroyDescriptorSetLayout(handle);
        }
    }

    void destroy(Device *device, ShaderModuleHandle handle)
    {
        if (device)
        {
            device->destroyShaderModule(handle);
        }
    }
}
