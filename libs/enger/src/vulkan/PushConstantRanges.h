#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "vulkan/vk.h"

namespace enger
{
    // PUSH CONSTANT RANGES
    struct ComputePushConstants
    {
        alignas(16) glm::vec4 data1;
        alignas(16) glm::vec4 data2;
        alignas(4) uint32_t textureIndex;
    };

    struct GraphicsPushConstants
    {
        // TODO move worldMatrix into a SSBO (BDA to SSBO could exist within sceneData)
        alignas(16) glm::mat4 worldMatrix;
        alignas(8) vk::DeviceAddress vertexBufferDeviceAddress;
        alignas(8) vk::DeviceAddress sceneDataBDA;
        alignas(8) vk::DeviceAddress materialBDA;
        alignas(4) uint32_t colorTextureIndex;
        alignas(4) uint32_t metallicRoughnessTextureIndex;
        alignas(4) uint32_t samplerIndex;
    };
}
