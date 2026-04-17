#pragma once
#include "SceneGraph.h"

#include "Camera.h"
#include "Stats.h"

namespace enger
{
    struct GPUSceneData
    {
        alignas(16) glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection; // w for sun power
        glm::vec4 sunlightColor;
    };

    class SceneManager
    {
    public:
        SceneManager(Device& device, vk::Format renderFormat, vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples);

        void loadScene(const std::filesystem::path& filePath);

        [[nodiscard]] const DrawContext& updateScene(float width, float height, const Camera& camera, EngineStats& stats);

        [[nodiscard]] const DrawContext& drawContext() const { return m_DrawContext; }
        [[nodiscard]] BufferHandle sceneDataBuffer() const { return m_GPUSceneDataBuffer; }
    private:
        Device& m_Device;

        DrawContext m_DrawContext;
        std::unordered_map<std::string, std::unique_ptr<LoadedGLTF>> m_LoadedScenes;

        GPUSceneData m_SceneData;
        Holder<BufferHandle> m_GPUSceneDataBuffer;

    public:
        Holder<TextureHandle> m_WhiteImage;
        Holder<TextureHandle> m_BlackImage;
        Holder<TextureHandle> m_GrayImage;
        Holder<TextureHandle> m_ErrorCheckerboardImage;

        Holder<SamplerHandle> m_DefaultSamplerLinear;
        Holder<SamplerHandle> m_DefaultSamplerNearest;

        GLTFMetallic_Roughness m_GLTFMetallic_Roughness;
        MaterialInstance m_DefaultMaterial;
    };
}
