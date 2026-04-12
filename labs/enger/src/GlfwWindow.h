#pragma once

#include <span>
#include <functional>
#include <GLFW/glfw3.h>

namespace enger
{
    class GlfwWindow
    {
    public:
        GlfwWindow(uint32_t width, uint32_t height, const char* title, bool resizable = true)
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, resizable);

            m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

            glfwSetWindowUserPointer(m_Window, this);

            glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
                auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
                if (self->m_ResizeCallback)
                {
                    self->m_ResizeCallback(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
                }
            });
        }

        ~GlfwWindow()
        {
            glfwDestroyWindow(m_Window);
            glfwTerminate();
        }

        std::pair<uint32_t, uint32_t> framebufferSize() const
        {
            int width, height;
            glfwGetFramebufferSize(m_Window, &width, &height);
            return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        }

        std::vector<const char*> requiredInstanceExtensions() const
        {
            uint32_t count = 0;
            auto glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
            return std::vector<const char*>(glfwExtensions, glfwExtensions + count);
        }

        void setResizeCallback(std::function<void(uint32_t, uint32_t)> callback)
        {
            m_ResizeCallback = std::move(callback);
        }

        GLFWwindow* nativeHandle() const
        {
            return m_Window;
        }

        bool shouldClose() const
        {
            return glfwWindowShouldClose(m_Window);
        }

        void poll() const
        {
            glfwPollEvents();
        }

    private:
        GLFWwindow* m_Window = nullptr;

        std::function<void(uint32_t, uint32_t)> m_ResizeCallback;
    };
}
