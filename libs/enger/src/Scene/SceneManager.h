#pragma once
#include "Scene/SceneGraph.h"

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

    class ENGER_EXPORT SceneManager
    {
    public:
        SceneManager(Device& device, vk::Format renderFormat, vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples,
            std::string_view shaderPath);

        // This doesn't actually load the new scene, it just schedules the scene load to updateScene
        void loadScene(const std::filesystem::path& filePath);

        [[nodiscard]] DrawContext& updateScene(float width, float height, const Camera& camera,
                                                     EngineStats& stats);

        [[nodiscard]] const DrawContext& drawContext() const { return m_DrawContext; }
        [[nodiscard]] BufferHandle sceneDataBuffer() const { return m_GPUSceneDataBuffer; }
    private:
        Device& m_Device;

        DrawContext m_DrawContext;
        std::unordered_map<std::string, std::unique_ptr<LoadedGLTF>> m_LoadedScenes;

        GPUSceneData m_SceneData;
        Holder<BufferHandle> m_GPUSceneDataBuffer;

        std::filesystem::path m_PendingScenePath;
        bool m_IsScenePending = false;

    public:
        Holder<TextureHandle> m_WhiteImage;
        Holder<TextureHandle> m_BlackImage;
        Holder<TextureHandle> m_GrayImage;
        Holder<TextureHandle> m_ErrorCheckerboardImage;

        Holder<SamplerHandle> m_DefaultSamplerLinear;
        Holder<SamplerHandle> m_DefaultSamplerNearest;

        GLTFMetallic_Roughness m_GLTFMetallic_Roughness;
        MaterialInstance m_DefaultMaterial;

        float m_Aspect = 16.0f/9.0f;
        float m_FovH = glm::radians(90.0f);
        float m_FovY = glm::radians(90.0f);
    };
}
