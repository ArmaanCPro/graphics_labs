#include "Window.h"

#include <memory>
#include <vector>
#include <GLFW/glfw3.h>

#include "Logging/Assert.h"

namespace enger
{
    static bool s_GLFWInitialized = false;

    struct Window::Impl
    {
        GLFWwindow* window = nullptr;

        Impl(uint32_t width, uint32_t height, const char* title, bool resizable)
        {
            EASSERT(!s_GLFWInitialized, "GLFW already initialized");
            s_GLFWInitialized = true;
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            glfwWindowHint(GLFW_RESIZABLE, resizable);

            window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        }

        ~Impl()
        {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    };

    Window Window::create(uint32_t width, uint32_t height, const char* title, bool resizable)
    {
        auto impl = std::make_unique<Impl>(width, height, title, resizable);

        glfwSetInputMode(impl->window, GLFW_CURSOR_DISABLED, GLFW_TRUE);
        glfwSetInputMode(impl->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

        auto window = Window(std::move(impl));
        auto handle = window.m_Pimpl->window;

        glfwSetWindowUserPointer(handle, &window);

        glfwSetFramebufferSizeCallback(handle, [](GLFWwindow* window, int width, int height) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (self->m_ResizeCallback)
            {
                self->m_ResizeCallback(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
            }
        });

        glfwSetKeyCallback(handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (self->m_InputCallback)
            {
                self->m_InputCallback(key, scancode, action, mods);
            }
        });

        glfwSetMouseButtonCallback(handle, [](GLFWwindow* window, int button, int action, int mods) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (self->m_MouseCallback)
            {
                self->m_MouseCallback(button, action, mods);
            }
        });

        glfwSetCursorPosCallback(handle, [](GLFWwindow* window, double xpos, double ypos) {
            auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
            if (self->m_CursorPosCallback)
            {
                self->m_CursorPosCallback(xpos, ypos);
            }
        });

        return window;
    }

    Window::~Window() = default;

    Window::Window(Window&& rhs) noexcept
        : m_Pimpl(std::move(rhs.m_Pimpl))
    {}

    Window& Window::operator=(Window&& rhs) noexcept
    {
        m_Pimpl = std::move(rhs.m_Pimpl);
        return *this;
    }

    std::pair<uint32_t, uint32_t> Window::framebufferSize() const
    {
        int width, height;
        glfwGetFramebufferSize(m_Pimpl->window, &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    std::vector<const char*> Window::requiredInstanceExtensions() const
    {
        uint32_t count = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
        return std::vector<const char*>(glfwExtensions, glfwExtensions + count);
    }

    float Window::getDpiScale() const
    {
        float xscale, yscale;
        glfwGetWindowContentScale(m_Pimpl->window, &xscale, &yscale);
        return xscale; // usually same as the y-scale
    }

    std::pair<float, float> Window::getCursorPos() const
    {
        double xpos, ypos;
        glfwGetCursorPos(m_Pimpl->window, &xpos, &ypos);
        return {static_cast<float>(xpos), static_cast<float>(ypos)};
    }

    void Window::disableCursor() const
    {
        glfwSetInputMode(m_Pimpl->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetInputMode(m_Pimpl->window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    void Window::enableCursor() const
    {
        glfwSetInputMode(m_Pimpl->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetInputMode(m_Pimpl->window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }

    void* Window::nativeHandle() const
    {
        return m_Pimpl->window;
    }

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(m_Pimpl->window);
    }

    void Window::poll() const
    {
        glfwPollEvents();
    }

    bool Window::isKeyPressed(KeyCode key) const
    {
        // the KeyCode translates directly to GLFW key codes, so we don't need a translation step here
        return glfwGetKey(m_Pimpl->window, static_cast<int>(key)) == GLFW_PRESS;
    }

    Window::Window(std::unique_ptr<Impl>&& impl) noexcept
        : m_Pimpl(std::move(impl))
    {}
}
