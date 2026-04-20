#include "MeshLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <ktx.h>

#include <print>

#include "vulkan/Device.h"

#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "SceneGraph.h"

#include "SceneManager.h"

#include <algorithm>
#include <execution>

namespace enger
{
    GPUMeshBuffers uploadMesh(Device& device, std::span<uint32_t> indices, std::span<Vertex> vertices)
    {
        ENGER_PROFILE_FUNCTION()
        const size_t vbSize = sizeof(Vertex) * vertices.size();
        const size_t ibSize = sizeof(uint32_t) * indices.size();

        GPUMeshBuffers surface;

        auto* queue = &device.graphicsQueue();
        if (device.transferQueue().has_value())
        {
            queue = &device.transferQueue().value();
        }

        surface.vertexBuffer = device.createBuffer(
            vbSize,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            queue,
            "VertexBuffer");
        surface.indexBuffer = device.createBuffer(
            ibSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            queue,
            "IndexBuffer");

        auto staging = device.createBuffer(
            vbSize + ibSize, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            queue,
            "StagingBuffer"
        );

        auto* stagingBuffer = device.getBuffer(staging);

        stagingBuffer->bufferSubData(device.allocator(), 0, vbSize, vertices.data());
        stagingBuffer->bufferSubData(device.allocator(), vbSize, ibSize, indices.data());

        queue->submitImmediate([&](CommandBuffer& cmd) {
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

    struct TextureTask
    {
        // cpu data, filled by loadImage
        std::variant<std::filesystem::path, std::span<const std::byte> > data{};
        fastgltf::MimeType mimeType;
        // gpu data, filled by lambda
        int width = 0, height = 0, comp = 0;
        std::optional<std::variant<stbi_uc*, ktxTexture2*> > gpuPixels;
        vk::Format format = vk::Format::eR8G8B8A8Unorm;

        ~TextureTask()
        {
            if (gpuPixels.has_value())
            {
                std::visit(fastgltf::visitor{
                               [](stbi_uc* pixels) {
                                   stbi_image_free(static_cast<void*>(pixels));
                               },
                               [](ktxTexture2* texture) {
                                   ktxTexture2_Destroy(texture);
                               },
                           }, gpuPixels.value());
            }
        }
    };

    TextureTask loadImage(fastgltf::Asset& asset, fastgltf::Image& image,
                          const std::filesystem::path& gltfPath)
    {
        ENGER_PROFILE_FUNCTION()

        return std::visit<TextureTask>(
            fastgltf::visitor{
                [&](auto&) {
                    std::cerr << "loadImage: Unhandled image data type: " << typeid(image).name() << '\n';
                    return TextureTask{};
                },
                [&](fastgltf::sources::URI& filePath) {
                    assert(filePath.fileByteOffset == 0);
                    assert(filePath.uri.isLocalPath());

                    const std::string path(filePath.uri.path().begin(),
                                           filePath.uri.path().end());
                    const std::filesystem::path fullPath = gltfPath.parent_path() / path;
                    auto canonicalPath = std::filesystem::absolute(fullPath);

                    return TextureTask{
                        .data = std::move(canonicalPath),
                        .mimeType = filePath.mimeType,
                    };
                },
                [&](fastgltf::sources::Vector& vector) {
                    return TextureTask{
                        .data = std::span(vector.bytes.data(),
                                          vector.bytes.size()),
                    };
                },
                [&](fastgltf::sources::BufferView& view) {
                    auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                    auto& buffer = asset.buffers[bufferView.bufferIndex];
                    return std::visit<TextureTask>(fastgltf::visitor{
                                                       [&](auto&) {
                                                           std::cerr << "loadImage: Unhandled buffer data type: " <<
                                                               typeid(image).name();
                                                           return TextureTask{};
                                                       },
                                                       [&](fastgltf::sources::Array& array) {
                                                           return TextureTask{
                                                               .data = std::span{
                                                                   array.bytes.data() + bufferView.byteOffset,
                                                                   bufferView.byteLength
                                                               }
                                                           };
                                                       },
                                                       [&](fastgltf::sources::ByteView& byteView) {
                                                           return TextureTask{
                                                               .data = std::span{
                                                                   byteView.bytes.data() + bufferView.byteOffset,
                                                                   bufferView.byteLength
                                                               }
                                                           };
                                                       },
                                                       [&](fastgltf::sources::Vector& vector) {
                                                           return TextureTask{
                                                               .data = std::span{
                                                                   vector.bytes.data() + bufferView.byteOffset,
                                                                   bufferView.byteLength
                                                               }
                                                           };
                                                       }
                                                   }, buffer.data);
                },
            },
            image.data
        );
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

    std::optional<std::unique_ptr<LoadedGLTF> > LoadGltf(
        Device& device, SceneManager& sceneManager, const std::filesystem::path& filePath)
    {
        ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_CREATE)

        std::println("Loading GLTF: {}", filePath.string());

        std::unique_ptr<LoadedGLTF> scene = std::make_unique<LoadedGLTF>(device, &sceneManager);
        LoadedGLTF& file = *scene;

        auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
        if (!data)
        {
            std::cerr << "Failed to load gltf file: " << filePath.string();
            return std::nullopt;
        }

        constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers
                                     | fastgltf::Options::DontRequireValidAssetMember
                                     | fastgltf::Options::AllowDouble;

        fastgltf::Parser parser{fastgltf::Extensions::KHR_texture_basisu};

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

        auto& gltf = asset.get(); {
            ENGER_PROFILE_ZONEN("Sampler Creation")
            for (fastgltf::Sampler& sampler : gltf.samplers)
            {
                file.samplers_.push_back(device.createSampler(SamplerDesc{
                                                                  .magFilter = extractFilter(
                                                                      sampler.magFilter.value_or(
                                                                          fastgltf::Filter::Linear)),
                                                                  .minFilter = extractFilter(
                                                                      sampler.minFilter.value_or(
                                                                          fastgltf::Filter::Linear)),
                                                                  .mipmapMode = extractMipmapMode(
                                                                      sampler.minFilter.value_or(
                                                                          fastgltf::Filter::Linear)),
                                                                  .anisotropyEnable = true,
                                                              }, nullptr));
            }
        }

        // temporal arrays for all the objects to use while creating the GLTF data
        std::vector<std::shared_ptr<MeshAsset> > meshes;
        std::vector<std::shared_ptr<Node> > nodes;
        std::vector<TextureHandle> images;
        std::vector<std::shared_ptr<GLTFMaterial> > materials;

        // load textures
        {
            ENGER_PROFILE_ZONEN("Texture Creation")
            // Build tasks sequentially
            std::vector<TextureTask> tasks;
            tasks.reserve(gltf.images.size());
            std::vector<std::string_view> names; {
                ENGER_PROFILE_ZONEN("Texture Task Generation");
                for (fastgltf::Image& image: gltf.images)
                {
                    tasks.push_back(std::move(loadImage(gltf, image, filePath)));
                    names.push_back(image.name);
                }
            }

            // Process IO in parallel
            {
                ENGER_PROFILE_ZONEN("Texture IO");
                std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&](TextureTask& task) {
                    if (task.mimeType == fastgltf::MimeType::KTX2)
                    {
                        static constexpr auto transcode = [](ktxTexture2* tex) -> bool {
                            if (ktxTexture2_NeedsTranscoding(tex))
                            {
                                const auto res = ktxTexture2_TranscodeBasis(tex,
                                                                            KTX_TTF_BC7_RGBA,
                                                                            0);
                                // TODO support for other formats like ASTC for mobile in the future?
                                if (res != KTX_SUCCESS)
                                {
                                    std::cerr << "Failed to transcode ktx2 texture: " << ktxErrorString(res) << '\n';
                                    return false;
                                }
                            }
                            return true;
                        };

                        std::visit(fastgltf::visitor{
                                       [&](const std::filesystem::path& path) {
                                           ktxTexture2* tex;
                                           const auto ec = ktxTexture2_CreateFromNamedFile(
                                               path.string().c_str(),
                                               ktxTextureCreateFlagBits::KTX_TEXTURE_CREATE_CHECK_GLTF_BASISU_BIT,
                                               &tex);
                                           if (ec != KTX_SUCCESS)
                                           {
                                               std::cerr << "Failed to load ktx2 texture: " << ktxErrorString(ec) <<
                                                   '\n';
                                               return;
                                           }
                                           if (!transcode(tex))
                                               return;
                                           task.gpuPixels = tex;
                                           task.format = static_cast<vk::Format>(tex->vkFormat);
                                           task.width = tex->baseWidth;
                                       },
                                       [&](const std::span<const std::byte>& bytes) {
                                           ktxTexture2* tex;
                                           const auto ec = ktxTexture2_CreateFromMemory(
                                               reinterpret_cast<const ktx_uint8_t*>(bytes.data()),
                                               static_cast<int>(bytes.size()),
                                               ktxTextureCreateFlagBits::KTX_TEXTURE_CREATE_CHECK_GLTF_BASISU_BIT,
                                               &tex);
                                           if (ec != KTX_SUCCESS)
                                           {
                                               std::cerr << "Failed to load ktx2 texture: " << ktxErrorString(ec) <<
                                                   '\n';
                                               return;
                                           }
                                           if (!transcode(tex))
                                               return;
                                           task.gpuPixels = tex;
                                           task.format = static_cast<vk::Format>(tex->vkFormat);
                                       }
                                   }, task.data);
                    }
                    else
                    {
                        std::visit(fastgltf::visitor{
                                       [&](const std::filesystem::path& path) {
                                           task.gpuPixels = stbi_load(path.string().c_str(),
                                                                      &task.width, &task.height, &task.comp, 4);
                                       },
                                       [&](const std::span<const std::byte>& bytes) {
                                           task.gpuPixels = stbi_load_from_memory(
                                               reinterpret_cast<stbi_uc const*>(bytes.data()),
                                               static_cast<int>(bytes.size()),
                                               &task.width, &task.height, &task.comp, 4
                                           );
                                       }
                                   }, task.data);
                    }
                });
            } {
                ENGER_PROFILE_ZONEN("Texture GPU Upload");
                // Upload to GPU is faster sequentially rather than parallel due to PCIe bandwith constraint
                for (uint32_t i = 0; i < tasks.size(); i++)
                {
                    auto& task = tasks[i];
                    bool hasValue = task.gpuPixels.has_value();
                    if (hasValue)
                    {
                        std::visit(
                            [&](auto& pixels) {
                                hasValue = pixels != nullptr;
                            }
                            , task.gpuPixels.value());
                    }
                    if (hasValue)
                    {
                        TextureDesc desc{
                            .format = task.format,
                            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                                     vk::ImageUsageFlagBits::eTransferSrc,
                        };
                        std::visit(fastgltf::visitor{
                                       [&](stbi_uc* pixels) {
                                           desc.dimensions = {
                                               static_cast<uint32_t>(task.width), static_cast<uint32_t>(task.height), 1
                                           };
                                           desc.generateMipMaps = false; // still blit the mips for STBI
                                           desc.subresources.push_back(TextureSubresource{
                                               .data = pixels,
                                               .extent = desc.dimensions,
                                               .mipLevel = 0,
                                               .arrayLayer = 0,
                                               .size = static_cast<size_t>(task.width * task.height * 1 * 4),
                                           });
                                       },
                                       [&](ktxTexture2* tex) {
                                           desc.dimensions = {tex->baseWidth, tex->baseHeight, tex->baseDepth};
                                           desc.mipLevels = tex->numLevels;
                                           desc.arrayLayers = tex->numLayers;

                                           for (auto level = 0u; level < tex->numLevels; level++)
                                           {
                                               ktx_size_t offset = 0;
                                               ktxTexture2_GetImageOffset(tex, level, 0, 0, &offset);

                                               desc.subresources.push_back(TextureSubresource{
                                                   .data = tex->pData + offset,
                                                   .extent = {
                                                       std::max(1u, tex->baseWidth >> level),
                                                       std::max(1u, tex->baseHeight >> level),
                                                       std::max(1u, tex->baseDepth >> level),
                                                   },
                                                   .mipLevel = level,
                                                   .arrayLayer = 0, // TODO handles layers for cubemaps
                                                   .size = ktxTexture_GetImageSize(ktxTexture(tex), level)
                                               });
                                           }
                                       }
                                   }, task.gpuPixels.value());

                        auto img = device.createTexture(desc, nullptr);
                        images.push_back(img);
                        file.images_[names[i].data()] = std::move(img);
                    }
                    else
                    {
                        std::cerr << "Failed to load image: " << stbi_failure_reason() << " " << names[i] << std::endl;
                        images.push_back(sceneManager.m_ErrorCheckerboardImage);
                    }
                }
            }
        }

        if (!gltf.materials.empty())
        {
            ENGER_PROFILE_ZONEN("Material Constants Buffer Creation")
            file.materialDataBuffer_ = device.createBuffer(sizeof(MaterialConstants) * gltf.materials.size(),
                                                           vk::BufferUsageFlagBits::eUniformBuffer |
                                                           vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                           vk::MemoryPropertyFlagBits::eHostVisible |
                                                           vk::MemoryPropertyFlagBits::eHostCoherent,
                                                           nullptr, "MaterialConstants FOR MESH LOADER");
        }

        int dataIndex = 0; {
            ENGER_PROFILE_ZONEN("Material Creation")
            for (fastgltf::Material& material : gltf.materials)
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

                materialResources.materialConstantsBuffer = file.materialDataBuffer_;
                materialResources.materialConstantsBufferOffset = dataIndex * sizeof(MaterialConstants);

                // get texture data
                if (material.pbrData.baseColorTexture.has_value())
                {
                    const auto textureIndex = material.pbrData.baseColorTexture.value().textureIndex;
                    const auto imageIndex = gltf.textures[textureIndex].basisuImageIndex.value_or(gltf.textures[textureIndex].imageIndex.value());
                    auto sampler = gltf.textures[textureIndex].samplerIndex;

                    if (imageIndex < images.size())
                    {
                        materialResources.colorImage = images[imageIndex];
                    }
                    else
                    {
                        std::cerr << "Failed to load texture: " << material.pbrData.baseColorTexture.value().
                            textureIndex << std::endl;
                    }
                    if (sampler.has_value())
                    {
                        materialResources.colorSampler = file.samplers_[sampler.value()];
                    }
                    else
                    {
                        std::cerr << "Failed to load sampler: " << material.pbrData.baseColorTexture.value().
                            textureIndex << std::endl;
                    }
                }

                std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
                newMaterial->material = sceneManager.m_GLTFMetallic_Roughness.writeMaterial(
                    passType, std::move(materialResources));
                materials.push_back(newMaterial);
                file.materials_[material.name.c_str()] = std::move(newMaterial);

                dataIndex++;
            }
        }

        std::vector<uint32_t> indices;
        std::vector<Vertex> vertices; {
            ENGER_PROFILE_ZONEN("Mesh Creation")
            for (fastgltf::Mesh& mesh : gltf.meshes)
            {
                std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
                meshes.push_back(newMesh);
                file.meshes_[mesh.name.c_str()] = newMesh;
                newMesh->name = mesh.name;

                indices.clear();
                vertices.clear();

                for (auto&& p : mesh.primitives)
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

                    if (p.materialIndex.has_value() && !materials.empty())
                    {
                        newSurface.material = materials[p.materialIndex.value()];
                    }
                    else if (!materials.empty())
                    {
                        newSurface.material = materials[0];
                    }

                    newMesh->surfaces.push_back(newSurface);
                }

                newMesh->meshBuffers = uploadMesh(device, indices, vertices);
            }
        }

        // load all nodes and their meshes
        {
            ENGER_PROFILE_ZONEN("Node Creation")
            for (fastgltf::Node& node : gltf.nodes)
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
        }

        // run loop again to setup transform hierarchy
        {
            ENGER_PROFILE_ZONEN("Node Transform Hierarchy")
            for (int i = 0; i < gltf.nodes.size(); i++)
            {
                fastgltf::Node& node = gltf.nodes[i];
                std::shared_ptr<Node>& sceneNode = nodes[i];

                for (auto& c : node.children)
                {
                    sceneNode->children.push_back(nodes[c]);
                    nodes[c]->parent = sceneNode;
                }
            }
        }

        // find the top nodes, with no parents
        {
            ENGER_PROFILE_ZONEN("Top Nodes")
            for (auto& node : nodes)
            {
                if (node->parent.lock() == nullptr)
                {
                    file.topNodes_.push_back(node);
                    node->refreshTransform(glm::mat4(1.0f));
                }
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
