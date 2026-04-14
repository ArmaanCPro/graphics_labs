#pragma once

#include "vulkan/vk.h"
#include "Resources.h"

#include <unordered_map>
#include <filesystem>

#include <glm/glm.hpp>

#include "SceneGraph.h"

namespace enger
{
    // Describes vertex data.
    struct Vertex
    {
        glm::vec3 position;
        float uv_x;
        glm::vec3 normal;
        float uv_y;
        glm::vec4 color;
    };

    // Describes the GPU buffers for a mesh (vertex & index).
    struct GPUMeshBuffers
    {
        Holder<BufferHandle> vertexBuffer;
        Holder<BufferHandle> indexBuffer;
    };

    struct GLTFMaterial
    {
        MaterialInstance& material;
    };

    // Defines the indices of a surface. A surface is a sub-mesh: a unique draw call.
    struct GeoSurface
    {
        uint32_t startIndex;
        uint32_t indexCount;
        std::shared_ptr<GLTFMaterial> material;
    };

    // Describes a mesh consisting of different sub-meshes, all stored in a single vertex & index buffer.
    struct MeshAsset
    {
        std::string name;
        std::vector<GeoSurface> surfaces;
        GPUMeshBuffers meshBuffers;
    };

    class Device;
    std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadMeshes(Device& device, const std::filesystem::path& filePath);
}
