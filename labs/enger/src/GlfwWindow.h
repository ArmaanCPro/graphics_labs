#pragma once

#include <span>
#include <functional>
#include <GLFW/glfw3.h>

#include "Profiling/Profiler.h"

namespace enger
{
    class GlfwWindow
    {
    public:
        GlfwWindow(uint32_t width, uint32_t height, const char* title, bool resizable = true)
        {
            ENGER_PROFILE_FUNCTION_COLOR(ENGER_PROFILE_COLOR_CREATE)
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, resizable);

            m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);

            glfwSetInputMode(m_Window, GLFW_CURSOR_DISABLED, GLFW_TRUE);
            glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

            glfwSetWindowUserPointer(m_Window, this);

            glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
                auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
                if (self->m_ResizeCallback)
                {
                    self->m_ResizeCallback(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
                }
            });

            glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
                auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
                if (self->m_InputCallback)
                {
                    self->m_InputCallback(key, scancode, action, mods);
                }
            });

            glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
                auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
                if (self->m_MouseCallback)
                {
                    self->m_MouseCallback(button, action, mods);
                }
            });

            glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xpos, double ypos) {
                auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window));
                if (self->m_CursorPosCallback)
                {
                    self->m_CursorPosCallback(xpos, ypos);
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

        float getDpiScale() const
        {
            float xscale, yscale;
            glfwGetWindowContentScale(m_Window, &xscale, &yscale);
            return xscale; // usually same as the y-scale
        }

        std::pair<float, float> getCursorPos() const
        {
            double xpos, ypos;
            glfwGetCursorPos(m_Window, &xpos, &ypos);
            return {static_cast<float>(xpos), static_cast<float>(ypos)};
        }

        void setResizeCallback(std::function<void(uint32_t, uint32_t)> callback)
        {
            m_ResizeCallback = std::move(callback);
        }

        void setInputCallback(std::function<void(int key, int scancode, int action, int mods)> callback)
        {
            m_InputCallback = std::move(callback);
        }

        void setMouseCallback(std::function<void(int button, int action, int mods)> callback)
        {
            m_MouseCallback = std::move(callback);
        }

        void setCursorPosCallback(std::function<void(double xpos, double ypos)> callback)
        {
            m_CursorPosCallback = std::move(callback);
        }

        void disableCursor() const
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
        void enableCursor() const
        {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetInputMode(m_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
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

        bool isKeyPressed(int key) const
        {
            return glfwGetKey(m_Window, key) == GLFW_PRESS;
        }

    private:
        GLFWwindow* m_Window = nullptr;

        std::function<void(uint32_t, uint32_t)> m_ResizeCallback;
        std::function<void(int key, int scancode, int action, int mods)> m_InputCallback;
        std::function<void(int button, int action, int mods)> m_MouseCallback;
        std::function<void(double xpos, double ypos)> m_CursorPosCallback;
    };
}
