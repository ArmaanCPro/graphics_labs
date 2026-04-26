#pragma once

#include <glm/glm.hpp>

#include "MeshLoader.h"
#include "vulkan/vk.h"

#include "Resources/Resources.h"
#include "Profiling/Profiler.h"

namespace enger
{
    // MATERIALS

    enum class MaterialPass : uint8_t
    {
        MainColor,
        Unlit,
        Additive,
        Transparent,
        Other,
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
        BufferHandle materialConstantsBuffer;
        uint32_t materialConstantsBufferOffset = 0;
    };

    // We will have 2 pipelines, one for opaque and one for transparent objects.
    struct MaterialPipeline
    {
        Holder<GraphicsPipelineHandle> pipeline;
        PipelineLayoutHandle pipelineLayout;
    };

    struct MaterialInstance
    {
        MaterialPipeline* pipeline = nullptr;
        MaterialResources resources;
        MaterialPass passType = MaterialPass::Other;
    };

    struct GLTFMaterial
    {
        MaterialInstance material;
    };

    // Houses everything pertinent to materials. A helper.
    struct GLTFMetallic_Roughness
    {
        MaterialPipeline opaquePipeline;
        MaterialPipeline unlitPipeline;
        MaterialPipeline additivePipeline;
        MaterialPipeline transparentPipeline;

        void buildPipelines(Device& device, vk::Format drawFormat, vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples, ShaderModuleHandle
                            shaderOverride = {});
        MaterialInstance writeMaterial(MaterialPass pass, MaterialResources&& resources);
    };

    // A low level struct containing pertinent GPU information for a renderable object.
    struct RenderObject
    {
        uint32_t firstIndex = 0;
        uint32_t indexCount = 0;
        BufferHandle indexBuffer;
        BufferHandle vertexBuffer;

        MaterialInstance* material = nullptr;

        glm::mat4 transform = glm::mat4(1.0f);
    };

    struct DrawContext
    {
        std::vector<RenderObject> opaqueSurfaces;
        std::vector<RenderObject> unlitSurfaces;
        std::vector<RenderObject> additiveSurfaces; // Additive Blending
        std::vector<RenderObject> transparentSurfaces; // Alpha Blending
        BufferHandle sceneDataBuffer;
        glm::mat4 viewProj;
        glm::vec3 cameraPos{0};
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
            ENGER_PROFILE_FUNCTION()
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

    class SceneManager;
    // Encompasses all GLTF data, and holds its own Nodes. Basically represents an entire glTF File (Scene)
    class LoadedGLTF : public IRenderable
    {
    public:
        explicit LoadedGLTF(Device& device, SceneManager* sceneManager) : m_Device(device), m_SceneManager(sceneManager) {}

        // Storage/Handles to all the data on a glTF file
        std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes_;
        std::unordered_map<std::string, std::shared_ptr<Node>> nodes_;
        std::unordered_map<std::string, Holder<TextureHandle>> images_;
        std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials_;

        // nodes that don't have a parent. Files are in a tree-like structure, so there will be roots
        std::vector<std::shared_ptr<Node>> topNodes_;

        std::vector<Holder<SamplerHandle>> samplers_;

        Holder<BufferHandle> materialDataBuffer_;

        std::string name_;

        void draw(const glm::mat4& topMatrix, DrawContext& ctx) override
        {
            for (auto& n : topNodes_)
            {
                n->draw(topMatrix, ctx);
            }
        }

    private:
        Device& m_Device;
        SceneManager* m_SceneManager;
    };
}
