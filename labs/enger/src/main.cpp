#include <array>
#include <span>
#include <stack>
#include <thread>

#include "vulkan/vk.h"

#include <GLFW/glfw3.h>

#include "Camera.h"
#include "GlfwWindow.h"
#include "Renderer.h"
#include "vulkan/Device.h"
#include "vulkan/Instance.h"
#include "vulkan/Surface.h"
#include "vulkan/SwapChain.h"

#include "Framing.h"
#include "SceneManager.h"
#include "Stats.h"

#include "FileLoader.h"

constexpr auto WIDTH = 800;
constexpr auto HEIGHT = 600;

struct ResizeEvent
{
    uint32_t newWidth;
    uint32_t newHeight;
};

int main()
{
    NFD_Init();

    enger::GlfwWindow window{WIDTH, HEIGHT, "Enger"};

    enger::NFDEFileLoader fileLoader{};

    std::stack<ResizeEvent> resizeEventBus;

    window.setResizeCallback([&](uint32_t width, uint32_t height) {
        resizeEventBus.push({width, height});
    });

    std::vector<const char*> instanceExtensions;
#ifndef NDEBUG
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    auto glfwExtensions = window.requiredInstanceExtensions();
    instanceExtensions.insert(instanceExtensions.end(), glfwExtensions.begin(), glfwExtensions.end());

    enger::Instance instance{instanceExtensions};

    // maybe: don't make it vulkan specific, instead maybe map some generic extension names to vulkan specific
    std::array requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
    };

    auto surface = enger::Surface{window, instance.instance()};

    auto device = enger::Device{instance.instance(), surface.surface(), requiredDeviceExtensions};

    auto swapchain = enger::SwapChain{
        device, surface.surface(), window,
        vk::PresentModeKHR::eMailbox
    };

    Camera camera{window};
    camera.velocity_ = glm::vec3{0.0f, 0.0f, 0.0f};
    camera.position_ = glm::vec3(30.0f, 0.0f, -85.0f);
    camera.pitch_ = 0;
    camera.yaw_ = 0;

    enger::Renderer renderer{device, swapchain};
    enger::ImguiLayer imguiLayer{instance, device, window, swapchain};

    enger::framing::FrameOrchestrator frameOrchestrator{device, swapchain, window};

    enger::SceneManager sceneManager{device, renderer.renderFormat(), renderer.depthFormat(), renderer.msaaSamples()};

    EngineStats stats{};

    bool shouldRender = true;

    while (!window.shouldClose())
    {
        window.poll();

        const auto start = std::chrono::high_resolution_clock::now();

        if (!resizeEventBus.empty())
        {
            auto event = resizeEventBus.top();
            while (!resizeEventBus.empty())
            {
                resizeEventBus.pop();
            }

            if (event.newWidth == 0 || event.newHeight == 0)
            {
                shouldRender = false;
                continue;
            }

            frameOrchestrator.onWindowResize(event.newWidth, event.newHeight);
            renderer.onResize(event.newWidth, event.newHeight);
            imguiLayer.onResize(event.newWidth, event.newHeight);

            shouldRender = true;
        }

        camera.update();
        const auto& dctx = sceneManager.updateScene(static_cast<float>(swapchain.swapChainExtent().width),
                                                    static_cast<float>(swapchain.swapChainExtent().height), camera, stats);

        if (!shouldRender)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto fctx = frameOrchestrator.beginFrame();
        if (fctx.has_value())
        {
            renderer.render(fctx.value(), dctx, stats);

            imguiLayer.beginFrame();
            ImGui::Begin("Stats");
            ImGui::Text("FPS: %.1f", 1000.0f / stats.frameTime);
            ImGui::Text("Frame Time: %.2f ms", stats.frameTime);
            ImGui::Text("Draw Time: %.2f ms", stats.meshDrawTime);
            ImGui::Text("Update Time: %.2f ms", stats.sceneUpdateTime);
            ImGui::Text("Triangles: %i", stats.triangleCount);
            ImGui::Text("Draw Calls: %i", stats.drawCalls);

            if (ImGui::Button("Import glTF"))
            {
                std::array<enger::FileItem, 1> fileItems = { enger::FileItem{"glTF", "gltf,glb"} };

                auto path = fileLoader.openDialog(fileItems);
                if (!path.has_value() && path.error() != enger::OpenDialogError::Cancelled)
                {
                    std::cerr << "Failed to open file dialog" << fileLoader.getLastError() << std::endl;
                }
                else if (path.has_value())
                {
                    std::cout << "Selected file: " << path.value().string() << std::endl;
                    sceneManager.loadScene(*path);
                }
            }
            ImGui::End();
            imguiLayer.endFrame(fctx.value());

            frameOrchestrator.endFrame(fctx.value());

            imguiLayer.postRenderFinished();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frameTime = elapsed.count() / 1000.0f;
    }

    device.waitIdle();
}
