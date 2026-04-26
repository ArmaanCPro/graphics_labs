#pragma once

#include <memory>
#include <functional>

#include <Utils/KeyCodes.h>
#include "enger_export.h"

namespace enger
{
    class ENGER_EXPORT Window
    {
    public:
        static Window create(uint32_t width, uint32_t height, const char* title, bool resizable = true);

        Window() = delete;
        ~Window();
        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        std::pair<uint32_t, uint32_t> framebufferSize() const;

        std::vector<const char*> requiredInstanceExtensions() const;

        float getDpiScale() const;

        std::pair<float, float> getCursorPos() const;

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

        void disableCursor() const;

        void enableCursor() const;

        void* nativeHandle() const;

        bool shouldClose() const;

        void poll() const;

        bool isKeyPressed(KeyCode key) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Pimpl;
        explicit Window(std::unique_ptr<Impl>&& impl) noexcept;

        std::function<void(uint32_t, uint32_t)> m_ResizeCallback;
        std::function<void(int key, int scancode, int action, int mods)> m_InputCallback;
        std::function<void(int button, int action, int mods)> m_MouseCallback;
        std::function<void(double xpos, double ypos)> m_CursorPosCallback;
    };
}
