#include "Resources.h"

#include "vulkan/Device.h"

// unfortunate we have to include Vulkan here, but it is what it is. Maybe refactor later by seperating Holder<>?
#include "vulkan/Queue.h"

namespace enger
{
    void destroy(Device *device, Queue *queue, ComputePipelineHandle handle)
    {
        if (device)
        {
            device->destroyComputePipeline(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, GraphicsPipelineHandle handle)
    {
        if (device)
        {
            device->destroyGraphicsPipeline(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, PipelineLayoutHandle handle)
    {
        if (device)
        {
            device->destroyPipelineLayout(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, TextureHandle handle)
    {
        if (device)
        {
            device->destroyTexture(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, BufferHandle handle)
    {
        if (device)
        {
            device->destroyBuffer(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, DescriptorSetLayoutHandle handle)
    {
        if (device)
        {
            device->destroyDescriptorSetLayout(handle, queue);
        }
    }

    void destroy(Device *device, Queue *queue, ShaderModuleHandle handle)
    {
        if (device)
        {
            device->destroyShaderModule(handle, queue);
        }
    }

    void destroy(Device* device, Queue* queue, SamplerHandle handle)
    {
        if (device)
        {
            device->destroySampler(handle, queue);
        }
    }
}
