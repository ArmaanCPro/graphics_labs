#include "MeshLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <print>

#include "vulkan/Device.h"

#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "SceneGraph.h"

#include "SceneManager.h"

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

        device.graphicsQueue().submitImmediate([&](CommandBuffer& cmd) {
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

    Holder<TextureHandle> loadImage(Device& device, fastgltf::Asset& asset, fastgltf::Image& image)
    {
        Holder<TextureHandle> newImage;

        int width, height, nrChannels;
        std::visit(
            fastgltf::visitor{
                [&](auto&) {
                    std::cerr << "loadImage: Unhandled image data type: " << typeid(image).name();
                },
                [&](fastgltf::sources::URI& filePath) {
                    assert(filePath.fileByteOffset == 0);
                    assert(filePath.uri.isLocalPath());

                    const std::string path(filePath.uri.path().begin(),
                                           filePath.uri.path().end());
                    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                    if (data)
                    {
                        vk::Extent3D imageExtent{
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .depth = 1,
                        };

                        newImage = device.createTexture({
                                                            .format = vk::Format::eR8G8B8A8Unorm,
                                                            .dimensions = imageExtent,
                                                            .usage = vk::ImageUsageFlagBits::eSampled |
                                                                     vk::ImageUsageFlagBits::eTransferDst,
                                                            .initialData = data,
                                                        }, nullptr);

                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::Vector& vector) {
                    unsigned char* data = stbi_load_from_memory(
                        reinterpret_cast<stbi_uc const*>(
                            vector.bytes.data()), static_cast<int>(vector.bytes.size()),
                        &width, &height, &nrChannels, 4);

                    if (data)
                    {
                        vk::Extent3D imageExtent{
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .depth = 1,
                        };
                        newImage = device.createTexture({
                                                            .format = vk::Format::eR8G8B8A8Unorm,
                                                            .dimensions = imageExtent,
                                                            .usage = vk::ImageUsageFlagBits::eSampled
                                                                     | vk::ImageUsageFlagBits::eTransferDst,
                                                            .initialData = data,
                                                        }, nullptr);

                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::BufferView& view) {
                    auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                    auto& buffer = asset.buffers[bufferView.bufferIndex];

                    std::visit(fastgltf::visitor{
                                   [&](auto&) {
                                       std::cerr << "loadImage: Unhandled buffer data type: " << typeid(image).name();
                                   },
                                   [&](fastgltf::sources::Array& array) {
                                       unsigned char* data = stbi_load_from_memory(
                                           reinterpret_cast<stbi_uc const*>(
                                               array.bytes.data() + bufferView.byteOffset),
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4);

                                       if (data)
                                       {
                                           vk::Extent3D imageExtent{
                                               .width = static_cast<uint32_t>(width),
                                               .height = static_cast<uint32_t>(height),
                                               .depth = 1,
                                           };
                                           newImage = device.createTexture({
                                                                               .format = vk::Format::eR8G8B8A8Unorm,
                                                                               .dimensions = imageExtent,
                                                                               .usage = vk::ImageUsageFlagBits::eSampled
                                                                                   | vk::ImageUsageFlagBits::eTransferDst,
                                                                               .initialData = data,
                                                                           }, nullptr);

                                           stbi_image_free(data);
                                       }
                                   },
                                   [&](fastgltf::sources::ByteView& byteView) {
                                       unsigned char* data = stbi_load_from_memory(
                                           reinterpret_cast<stbi_uc const*>(
                                               byteView.bytes.data() + bufferView.byteOffset),
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4);

                                       if (data)
                                       {
                                           vk::Extent3D imageExtent{
                                               .width = static_cast<uint32_t>(width),
                                               .height = static_cast<uint32_t>(height),
                                               .depth = 1,
                                           };
                                           newImage = device.createTexture({
                                                                               .format = vk::Format::eR8G8B8A8Unorm,
                                                                               .dimensions = imageExtent,
                                                                               .usage = vk::ImageUsageFlagBits::eSampled
                                                                                   | vk::ImageUsageFlagBits::eTransferDst,
                                                                               .initialData = data,
                                                                           }, nullptr);

                                           stbi_image_free(data);
                                       }
                                   },
                                   [&](fastgltf::sources::Vector& vector) {
                                       unsigned char* data = stbi_load_from_memory(
                                           reinterpret_cast<stbi_uc const*>(
                                               vector.bytes.data() + bufferView.byteOffset),
                                           static_cast<int>(bufferView.byteLength), &width, &height,
                                           &nrChannels, 4);

                                       if (data)
                                       {
                                           vk::Extent3D imageExtent{
                                               .width = static_cast<uint32_t>(width),
                                               .height = static_cast<uint32_t>(height),
                                               .depth = 1,
                                           };
                                           newImage = device.createTexture({
                                                                               .format = vk::Format::eR8G8B8A8Unorm,
                                                                               .dimensions = imageExtent,
                                                                               .usage = vk::ImageUsageFlagBits::eSampled
                                                                                   | vk::ImageUsageFlagBits::eTransferDst,
                                                                               .initialData = data,
                                                                           }, nullptr);

                                           stbi_image_free(data);
                                       }
                                   }
                               }, buffer.data);
                },
            },
            image.data
        );

        return newImage;
    }

    vk::Filter extractFilter(fastgltf::Filter filter)
    {
        switch (filter)
        {
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::NearestMipMapLinear:
                return vk::Filter::eNearest;

            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapNearest:
            case fastgltf::Filter::LinearMipMapLinear:
                return vk::Filter::eLinear;

            default:
                break;
        }

        return vk::Filter::eLinear;
    }

    vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
    {
        switch (filter)
        {
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::LinearMipMapNearest:
                return vk::SamplerMipmapMode::eNearest;

            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::LinearMipMapLinear:
                return vk::SamplerMipmapMode::eLinear;

            default:
                break;
        }
        return vk::SamplerMipmapMode::eLinear;
    }

    std::optional<std::shared_ptr<LoadedGLTF> > LoadGltf(
        Device& device, SceneManager& sceneManager, const std::filesystem::path& filePath)
    {
        std::println("Loading GLTF: {}", filePath.string());

        std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>(device, &sceneManager);
        LoadedGLTF& file = *scene;

        auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
        if (!data)
        {
            std::cerr << "Failed to load gltf file: " <<  filePath.string();
            return std::nullopt;
        }

        constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers
                                     | fastgltf::Options::DontRequireValidAssetMember
                                     | fastgltf::Options::AllowDouble;

        fastgltf::Parser parser{};

        auto asset = parser.loadGltf(data.get(), filePath.parent_path(), gltfOptions);
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

        auto& gltf = asset.get();

        for (fastgltf::Sampler& sampler: gltf.samplers)
        {
            file.samplers_.push_back(device.createSampler(SamplerDesc{
                                                              .magFilter = extractFilter(sampler.magFilter.value()),
                                                              .minFilter = extractFilter(sampler.minFilter.value()),
                                                              .mipmapMode = extractMipmapMode(
                                                                  sampler.minFilter.value_or(
                                                                      fastgltf::Filter::Nearest)),
                                                          }, nullptr));
        }

        // temporal arrays for all the objects to use while creating the GLTF data
        std::vector<std::shared_ptr<MeshAsset> > meshes;
        std::vector<std::shared_ptr<Node> > nodes;
        std::vector<TextureHandle> images;
        std::vector<std::shared_ptr<GLTFMaterial> > materials;

        // load textures
        for (fastgltf::Image& image: gltf.images)
        {
            auto img = loadImage(device, gltf, image);
            if (img.valid())
            {
                images.push_back(img);
                file.images_[image.name.c_str()] = std::move(img);
            }
            else
            {
                images.push_back(sceneManager.m_ErrorCheckerboardImage);
                std::cerr << "Failed to load image: " << image.name << std::endl;
            }
        }

        file.materialDataBuffer_ = device.createBuffer(sizeof(MaterialConstants) * gltf.materials.size(),
                                                      vk::BufferUsageFlagBits::eUniformBuffer |
                                                      vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                      vk::MemoryPropertyFlagBits::eHostVisible |
                                                      vk::MemoryPropertyFlagBits::eHostCoherent,
                                                      nullptr, "MaterialConstants FOR MESH LOADER");

        int dataIndex = 0;

        for (fastgltf::Material& material: gltf.materials)
        {
            MaterialConstants constants{};
            constants.colorFactors.r = material.pbrData.baseColorFactor[0];
            constants.colorFactors.g = material.pbrData.baseColorFactor[1];
            constants.colorFactors.b = material.pbrData.baseColorFactor[2];
            constants.colorFactors.a = material.pbrData.baseColorFactor[3];

            constants.metallicRoughnessFactors.x = material.pbrData.metallicFactor;
            constants.metallicRoughnessFactors.y = material.pbrData.roughnessFactor;

            device.getBuffer(file.materialDataBuffer_)->bufferSubData(device.allocator(),
                                                                     dataIndex * sizeof(MaterialConstants),
                                                                     sizeof(MaterialConstants), &constants);

            MaterialPass passType = MaterialPass::MainColor;
            if (material.alphaMode == fastgltf::AlphaMode::Blend)
            {
                passType = MaterialPass::Transparent;
            }

            MaterialResources materialResources;
            // default the material textures
            materialResources.colorImage = sceneManager.m_WhiteImage;
            materialResources.colorSampler = sceneManager.m_DefaultSamplerLinear;
            materialResources.metallicRoughnessImage = sceneManager.m_WhiteImage;
            materialResources.metallicRoughnessSampler = sceneManager.m_DefaultSamplerLinear;

            materialResources.dataBuffer = sceneManager.sceneDataBuffer();

            materialResources.materialConstantsBuffer = file.materialDataBuffer_;
            materialResources.materialConstantsBufferOffset = dataIndex * sizeof(MaterialConstants);

            // get texture data
            if (material.pbrData.baseColorTexture.has_value())
            {
                size_t img = gltf.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
                size_t sampler = gltf.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex.
                    value();

                materialResources.colorImage = images[img];
                materialResources.colorSampler = file.samplers_[sampler];
            }

            std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
            newMaterial->material = sceneManager.m_GLTFMetallic_Roughness.writeMaterial(
                passType, std::move(materialResources));
            materials.push_back(newMaterial);
            file.materials_[material.name.c_str()] = std::move(newMaterial);

            dataIndex++;
        }

        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices;

        for (fastgltf::Mesh& mesh: gltf.meshes)
        {
            std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
            meshes.push_back(newMesh);
            file.meshes_[mesh.name.c_str()] = newMesh;
            newMesh->name = mesh.name;

            indices.clear();
            vertices.clear();

            for (auto&& p: mesh.primitives)
            {
                GeoSurface newSurface;

                newSurface.startIndex = static_cast<uint32_t>(indices.size());
                newSurface.indexCount = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

                uint32_t initial_vtx = static_cast<uint32_t>(vertices.size());

                // load indexes
                {
                    fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);

                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                                                             [&](std::uint32_t idx) {
                                                                 indices.push_back(idx + initial_vtx);
                                                             });
                }

                // load vertex positions
                {
                    fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                                  [&](glm::vec3 v, size_t index) {
                                                                      Vertex newvtx;
                                                                      newvtx.position = v;
                                                                      newvtx.normal = {1, 0, 0};
                                                                      newvtx.color = glm::vec4{1.f};
                                                                      newvtx.uv_x = 0;
                                                                      newvtx.uv_y = 0;
                                                                      vertices[initial_vtx + index] = newvtx;
                                                                  });
                }

                // load vertex normals
                auto normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
                                                                  [&](glm::vec3 v, size_t index) {
                                                                      vertices[initial_vtx + index].normal = v;
                                                                  });
                }

                // load UVs
                auto uv = p.findAttribute("TEXCOORD_0");
                if (uv != p.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
                                                                  [&](glm::vec2 v, size_t index) {
                                                                      vertices[initial_vtx + index].uv_x = v.x;
                                                                      vertices[initial_vtx + index].uv_y = v.y;
                                                                  });
                }

                // load vertex colors
                auto colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end())
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
                                                                  [&](glm::vec4 v, size_t index) {
                                                                      vertices[initial_vtx + index].color = v;
                                                                  });
                }

                if (p.materialIndex.has_value())
                {
                    newSurface.material = materials[p.materialIndex.value()];
                }
                else
                {
                    newSurface.material = materials[0];
                }

                newMesh->surfaces.push_back(newSurface);
            }

            newMesh->meshBuffers = uploadMesh(device, indices, vertices);
        }

        // load all nodes and their meshes
        for (fastgltf::Node& node: gltf.nodes)
        {
            std::shared_ptr<Node> newNode;

            if (node.meshIndex.has_value())
            {
                newNode = std::make_shared<MeshNode>();
                static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
            }
            else
            {
                newNode = std::make_shared<Node>();
            }

            nodes.push_back(newNode);
            file.nodes_[node.name.c_str()] = newNode;

            std::visit(fastgltf::visitor{
                           [&](fastgltf::math::fmat4x4 matrix) {
                               memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                           },
                           [&](fastgltf::TRS transform) {
                               glm::vec3 tl(transform.translation[0], transform.translation[1],
                                            transform.translation[2]);
                               glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                             transform.rotation[2]);
                               glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);

                               glm::mat4 tm = glm::translate(glm::mat4(1.0f), tl);
                               glm::mat4 rm = glm::toMat4(rot);
                               glm::mat4 sm = glm::scale(glm::mat4(1.0f), scale);

                               newNode->localTransform = tm * rm * sm;
                           }
                       }, node.transform);
        }

        // run loop again to setup transform hierarchy
        for (int i = 0; i < gltf.nodes.size(); i++)
        {
            fastgltf::Node& node = gltf.nodes[i];
            std::shared_ptr<Node>& sceneNode = nodes[i];

            for (auto& c: node.children)
            {
                sceneNode->children.push_back(nodes[c]);
                nodes[c]->parent = sceneNode;
            }
        }

        // find the top nodes, with no parents
        for (auto& node: nodes)
        {
            if (node->parent.lock() == nullptr)
            {
                file.topNodes_.push_back(node);
                node->refreshTransform(glm::mat4(1.0f));
            }
        }

        if (gltf.defaultScene.has_value() && !gltf.scenes[gltf.defaultScene.value()].name.empty())
        {
            file.name_ = gltf.scenes[gltf.defaultScene.value()].name;
        }
        else if (!gltf.scenes.empty() && !gltf.scenes[0].name.empty())
        {
            file.name_ = gltf.scenes[0].name;
        }
        else
        {
            file.name_ = filePath.stem().string();
        }

        return scene;
    }
}
