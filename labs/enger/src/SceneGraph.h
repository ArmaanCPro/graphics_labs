#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "MeshLoader.h"
#include "vulkan/vk.h"

#include "Resources.h"

namespace enger
{
    // MATERIALS

    enum class MaterialPass : uint8_t
    {
        MainColor,
        Transparent,
        Other
    };

    // UBO for material color factors
    struct MaterialConstants
    {
        glm::vec4 colorFactors;
        glm::vec4 metallicRoughnessFactors;
        // padding, needed for uniform buffers
        glm::vec4 extra[14];
    };

    // holder of material data
    struct MaterialResources
    {
        TextureHandle colorImage;
        SamplerHandle colorSampler;
        TextureHandle metallicRoughnessImage;
        SamplerHandle metallicRoughnessSampler;
        BufferHandle dataBuffer;
        BufferHandle materialConstantsBuffer; // this is a handle to a buffer of MaterialConstants
        uint32_t materialConstantsBufferOffset;
    };

    // We will have 2 pipelines, one for opaque and one for transparent objects.
    struct MaterialPipeline
    {
        Holder<GraphicsPipelineHandle> pipeline;
        Holder<PipelineLayoutHandle> pipelineLayout;
    };

    struct MaterialInstance
    {
        MaterialPipeline* pipeline;
        MaterialResources resources;
        MaterialPass passType;
    };

    struct GLTFMaterial
    {
        MaterialInstance material;
    };

    // Houses everything pertinent to materials. A helper.
    struct GLTFMetallic_Roughness
    {
        MaterialPipeline opaquePipeline;
        MaterialPipeline transparentPipeline;

        void buildPipelines(Device& device, vk::Format drawFormat, vk::Format depthFormat);
        MaterialInstance writeMaterial(MaterialPass pass, MaterialResources&& resources);
    };

    // A low level struct containing pertinent GPU information for a renderable object.
    struct RenderObject
    {
        uint32_t firstIndex;
        uint32_t indexCount;
        BufferHandle indexBuffer;
        BufferHandle vertexBuffer;

        MaterialInstance* material;

        glm::mat4 transform;
    };

    struct DrawContext
    {
        std::vector<RenderObject> opaqueSurfaces;
    };

    // Base class for a renderable dynamic object.
    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
    };

    class Node : public IRenderable
    {
    public:
        std::weak_ptr<Node> parent;
        std::vector<std::shared_ptr<Node>> children;

        glm::mat4 localTransform;
        glm::mat4 worldTransform;

        void refreshTransform(const glm::mat4& parentTransform)
        {
            worldTransform = parentTransform * localTransform;
            for (auto& child: children)
            {
                child->refreshTransform(worldTransform);
            }
        }

        virtual void draw([[maybe_unused]] const glm::mat4& topMatrix, DrawContext& ctx) override
        {
            for (auto& c : children)
            {
                c->draw(worldTransform, ctx);
            }
        }
    };

    struct MeshAsset;
    class MeshNode : public Node
    {
    public:
        std::shared_ptr<MeshAsset> mesh;

        void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
    };

    // Encompasses all GLTF data, and holds its own Nodes. Basically represents an entire glTF File (Scene)
    class LoadedGLTF : public IRenderable
    {
    public:
        explicit LoadedGLTF(Device& device) : device(device) {}

        // Storage/Handles to all the data on a glTF file
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes_;
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes_;
        std::unordered_map<std::string, TextureHandle> images_;
        std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials_;

        // nodes that don't have a parent. Files are in a tree-like structure, so there will be roots
        std::vector<std::shared_ptr<Node>> topNodes_;

        std::vector<SamplerHandle> samplers_;

        Holder<BufferHandle> materialDataBuffer;

        virtual ~LoadedGLTF() override { clearAll(); };

        void draw(const glm::mat4& topMatrix, DrawContext& ctx) override
        {
            for (auto& n : topNodes_)
            {
                n->draw(topMatrix, ctx);
            }
        }

    private:
        Device& device;

        void clearAll() {}
    };
}
