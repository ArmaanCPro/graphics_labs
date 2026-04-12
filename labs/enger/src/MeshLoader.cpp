#include "MeshLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <print>

#include "vulkan/Device.h"

#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

namespace enger
{
    GPUMeshBuffers uploadMesh(Device& device, std::span<uint32_t> indices, std::span<Vertex> vertices)
    {
        const size_t vbSize = sizeof(Vertex) * vertices.size();
        const size_t ibSize = sizeof(uint32_t) * indices.size();

        GPUMeshBuffers surface;

        surface.vertexBuffer = device.createBuffer(
            vbSize,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &device.graphicsQueue(),
            "VertexBuffer");
        surface.indexBuffer = device.createBuffer(
            ibSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &device.graphicsQueue(),
            "IndexBuffer");

        auto staging = device.createBuffer(
            vbSize + ibSize, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &device.graphicsQueue(),
            "StagingBuffer"
        );

        auto* stagingBuffer = device.getBuffer(staging);

        stagingBuffer->bufferSubData(device.allocator(), 0, vbSize, vertices.data());
        stagingBuffer->bufferSubData(device.allocator(), vbSize, ibSize, indices.data());

        device.graphicsQueue().submitImmediate([&](CommandBuffer cmd) {
            vk::BufferCopy vertexCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vbSize,
            };
            cmd.copyBuffer(staging, surface.vertexBuffer, vertexCopy);

            vk::BufferCopy indexCopy{
                .srcOffset = vbSize,
                .dstOffset = 0,
                .size = ibSize,
            };

            cmd.copyBuffer(staging, surface.indexBuffer, indexCopy);
        });

        return surface;
    }

    std::optional<std::vector<std::shared_ptr<MeshAsset> > > LoadMeshes(
        Device& device, const std::filesystem::path& filePath)
    {
        std::println("Loading meshes from: {}", filePath.string());

        auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
        if (!data)
        {
            std::println("Failed to load gltf file: {}", filePath.string());
            return std::nullopt;
        }

        constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

        fastgltf::Parser parser{};
        auto asset = parser.loadGltfBinary(data.get(), filePath.parent_path(), gltfOptions);
        if (!asset)
        {
            std::println("Failed to parse gltf file: {}", filePath.string());
            return std::nullopt;
        }
        if (asset.error() != fastgltf::Error::None)
        {
            std::println("Failed to parse gltf file: {}, error: {}", filePath.string(), getErrorMessage(asset.error()));
            return std::nullopt;
        }

        auto& model = asset.get();
        //auto defaultScene = model.defaultScene.value_or(0);

        std::vector<std::shared_ptr<MeshAsset> > meshes;

        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        for (fastgltf::Mesh& mesh: model.meshes)
        {
            MeshAsset newMesh;

            newMesh.name = mesh.name;

            indices.clear();
            vertices.clear();

            for (auto&& p: mesh.primitives)
            {
                GeoSurface newSurface;
                newSurface.startIndex = static_cast<uint32_t>(indices.size());
                newSurface.indexCount = static_cast<uint32_t>(asset->accessors[p.indicesAccessor.value()].count);

                size_t initialVertex = vertices.size();

                // load indices
                {
                    auto& indicesAccessor = asset->accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indicesAccessor.count);

                    fastgltf::iterateAccessor<std::uint32_t>(asset.get(), indicesAccessor, [&](std::uint32_t index) {
                        indices.push_back(index);
                    });
                }

                // load vertex positions
                {
                    auto& posAccessor = asset->accessors[p.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), posAccessor,
                                                                  [&](glm::vec3 pos, size_t index) {
                                                                      vertices[initialVertex + index] = Vertex{
                                                                          .position = pos,
                                                                          .uv_x = 0,
                                                                          .normal = {1, 0, 0},
                                                                          .uv_y = 0,
                                                                          .color = glm::vec4{1}
                                                                      };
                                                                  });
                }

                // Load vertex normals
                if (auto normals = p.findAttribute("NORMAL"))
                {
                    auto& normalAccessor = asset->accessors[normals->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), normalAccessor,
                                                                  [&](glm::vec3 normal, size_t index) {
                                                                      vertices[initialVertex + index].normal = normal;
                                                                  });
                }

                // Load UVs
                if (auto* uv = p.findAttribute("TEXCOORD_0"))
                {
                    auto& uvAccessor = asset->accessors[uv->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset.get(), uvAccessor,
                                                                  [&](glm::vec2 uv, size_t index) {
                                                                      vertices[initialVertex + index].uv_x = uv.x;
                                                                      vertices[initialVertex + index].uv_y = uv.y;
                                                                  });
                }

                // Load Vertex Colors
                if (auto* colors = p.findAttribute("COLOR_0"))
                {
                    auto& colorAccessor = asset->accessors[colors->accessorIndex];

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset.get(), colorAccessor,
                        [&](glm::vec3 color, size_t index) {
                            vertices[initialVertex + index].color = glm::vec4{color, 1.0f};
                        });
                }

                newMesh.surfaces.push_back(std::move(newSurface));
            }

            // Display Vertex Normals
            static constexpr bool overrideColors = true;
            if constexpr (overrideColors)
            {
                for (Vertex& vertex: vertices)
                {
                    vertex.color = glm::vec4{vertex.normal, 1.0f};
                }
            }

            newMesh.meshBuffers = uploadMesh(device, indices, vertices);

            meshes.push_back(std::make_shared<MeshAsset>(std::move(newMesh)));
        }

        return meshes;
    }
}
