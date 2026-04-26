#include "SceneManager.h"

#include <ranges>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "MeshLoader.h"

#include "vulkan/Device.h"

#include "Utils/Spirv.h"

namespace enger
{
    SceneManager::SceneManager(Device& device, vk::Format renderFormat, vk::Format depthFormat,
                               vk::SampleCountFlagBits msaaSamples)
        : m_Device(device)
    {
        ENGER_PROFILE_FUNCTION()
        // Default Textures
        uint32_t white = glm::packUnorm4x8(glm::vec4{1.0f, 1.0f, 1.0f, 1.0f});
        m_WhiteImage = m_Device.createTexture({
                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                  .dimensions = {1, 1, 1},
                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                           vk::ImageUsageFlagBits::eTransferDst,
                                                  .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                  .subresources = {
                                                      TextureSubresource{
                                                          .data = &white,
                                                          .extent = {1, 1, 1},
                                                          .size = sizeof(uint32_t),
                                                      }
                                                  }
                                              }, nullptr, "WhiteImage");

        uint32_t gray = glm::packUnorm4x8(glm::vec4{0.66f, 0.66f, 0.66f, 1.0f});
        m_GrayImage = m_Device.createTexture({
                                                 .format = vk::Format::eR8G8B8A8Unorm,
                                                 .dimensions = {1, 1, 1},
                                                 .usage = vk::ImageUsageFlagBits::eSampled |
                                                          vk::ImageUsageFlagBits::eTransferDst,
                                                 .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                 .subresources = {
                                                     TextureSubresource{
                                                         .data = &gray,
                                                         .extent = {1, 1, 1},
                                                         .size = sizeof(uint32_t),
                                                     }
                                                 },
                                             }, nullptr, "GrayImage");

        uint32_t black = glm::packUnorm4x8(glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});
        m_BlackImage = m_Device.createTexture({
                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                  .dimensions = {1, 1, 1},
                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                           vk::ImageUsageFlagBits::eTransferDst,
                                                  .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                  .subresources = {
                                                      TextureSubresource{
                                                          .data = &black,
                                                          .extent = {1, 1, 1},
                                                          .size = sizeof(uint32_t),
                                                      }},
                                              }, nullptr, "BlackImage"); {
            // checkerboard image
            uint32_t magenta = glm::packUnorm4x8(glm::vec4{1.0f, 0.0f, 1.0f, 1.0f});
            std::array<uint32_t, 16 * 16> pixels;
            for (int x = 0; x < 16; ++x)
            {
                for (int y = 0; y < 16; ++y)
                {
                    pixels[y * 16 + x] = (x & 1) ^ (y & 1) ? magenta : black;
                }
            }
            m_ErrorCheckerboardImage = m_Device.createTexture(TextureDesc{
                                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                                  .dimensions = {16, 16, 1},
                                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                                           vk::ImageUsageFlagBits::eTransferDst |
                                                                           vk::ImageUsageFlagBits::eTransferSrc,
                                                                  .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                                  .generateMipmaps = true,
                                                                  .subresources = {
                                                                      TextureSubresource{
                                                                          .data = pixels.data(),
                                                                          .extent = {16, 16, 1},
                                                                          .size = sizeof(uint32_t) * 16 * 16,
                                                                      }
                                                                  }
                                                              }, nullptr, "ErrorCheckerboardImage");

            m_DefaultSamplerLinear = m_Device.createSampler({
                                                                .magFilter = vk::Filter::eLinear,
                                                                .minFilter = vk::Filter::eLinear,
                                                            }, nullptr, "DefaultSamplerLinear");

            m_DefaultSamplerNearest = m_Device.createSampler({
                                                                 .magFilter = vk::Filter::eNearest,
                                                                 .minFilter = vk::Filter::eNearest,
                                                             }, nullptr, "DefaultSamplerNearest");
        }

        m_GPUSceneDataBuffer = m_Device.createBuffer(sizeof(GPUSceneData),
                                                     vk::BufferUsageFlagBits::eUniformBuffer |
                                                     vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                     vk::MemoryPropertyFlagBits::eHostVisible |
                                                     vk::MemoryPropertyFlagBits::eHostCoherent,
                                                     nullptr, "GPUSceneData");

        // MATERIALS
        auto expectedShaderData = loadSpirvFromFile("shaders/mesh.spv");
        if (!expectedShaderData)
        {
            LOG_ERROR("Failed to load shader data: {}", expectedShaderData.error());
            std::terminate();
        }

        auto shaderModuleHolder = device.createShaderModule(std::move(expectedShaderData.value()),
            nullptr, "GLTFMetallic_Roughness: MeshShaderModule");

        m_GLTFMetallic_Roughness.buildPipelines(m_Device, renderFormat,
                                                depthFormat, msaaSamples, shaderModuleHolder);

        MaterialResources materialResources;
        materialResources.colorImage = m_WhiteImage;
        materialResources.colorSampler = m_DefaultSamplerLinear;
        materialResources.metallicRoughnessImage = m_WhiteImage;
        materialResources.metallicRoughnessSampler = m_DefaultSamplerLinear;

        auto materialConstants = m_Device.createBuffer(sizeof(MaterialConstants),
                                                       vk::BufferUsageFlagBits::eUniformBuffer |
                                                       vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                       vk::MemoryPropertyFlagBits::eHostVisible |
                                                       vk::MemoryPropertyFlagBits::eHostCoherent,
                                                       nullptr, "MaterialConstants"
        );
        MaterialConstants materialConstantsData{
            .colorFactors = glm::vec4(1),
            .metallicRoughnessFactors = glm::vec4(1, 0.5, 0, 0)
        };
        m_Device.getBuffer(materialConstants)->bufferSubData(m_Device.allocator(), 0, sizeof(MaterialConstants),
                                                             &materialConstantsData);

        materialResources.materialConstantsBuffer = std::move(materialConstants);

        m_DefaultMaterial = m_GLTFMetallic_Roughness.writeMaterial(MaterialPass::MainColor,
                                                                   std::move(materialResources));

        loadScene("assets/structure.glb");
    }

    void SceneManager::loadScene(const std::filesystem::path& filePath)
    {
        if (filePath.empty())
        {
            return;
        }
        m_PendingScenePath = filePath;
        m_IsScenePending = true;
    }

    const DrawContext& SceneManager::updateScene(float width, float height, const Camera& camera, EngineStats& stats)
    {
        ENGER_PROFILE_FUNCTION()
        auto start = std::chrono::high_resolution_clock::now();

        if (m_IsScenePending)
        {
            auto file = LoadGltf(m_Device, *this, m_PendingScenePath);
            if (file.has_value())
            {
                m_LoadedScenes.clear();
                m_LoadedScenes[file.value()->name_] = std::move(file.value());
                m_IsScenePending = false;
                m_PendingScenePath.clear();
            }
        }

        m_DrawContext.opaqueSurfaces.clear();
        m_DrawContext.unlitSurfaces.clear();
        m_DrawContext.additiveSurfaces.clear();
        m_DrawContext.transparentSurfaces.clear();

        for (auto& scene : m_LoadedScenes | std::views::values)
        {
            scene->draw(glm::mat4(1.0f), m_DrawContext);
        }

        glm::mat4 view = camera.viewMatrix();
        m_SceneData.view = view;
        if (const auto aspect = width / height; m_Aspect != aspect)
        {
            m_Aspect = aspect;
            m_FovH = m_Aspect >= 21.0f/9.0f ? glm::radians(110.0f) : glm::radians(70.0f);
            m_FovY = 2 * glm::atan(glm::tan(m_FovH / 2.0f), m_Aspect);
        }
        m_SceneData.proj = glm::perspective(m_FovY,
                                            m_Aspect,
                                            10000.0f, 0.1f);

        m_SceneData.viewProj = m_SceneData.proj * m_SceneData.view;

        m_SceneData.ambientColor = glm::vec4(0.2f);
        m_SceneData.sunlightColor = glm::vec4(0.8f, 0.6f, 0.7f, 1.0f);
        m_SceneData.sunlightDirection = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);

        m_Device.getBuffer(m_GPUSceneDataBuffer)->bufferSubData(m_Device.allocator(), 0, sizeof(GPUSceneData),
                                                                &m_SceneData);
        m_DrawContext.sceneDataBuffer = m_GPUSceneDataBuffer;

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.sceneUpdateTime = elapsed.count() / 1000.0f;

        m_DrawContext.viewProj = m_SceneData.viewProj; // todo cleanup duplication of this data
        m_DrawContext.cameraPos = camera.position_;

        return m_DrawContext;
    }
}
