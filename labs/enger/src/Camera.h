#pragma once

#include "vulkan/vk.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "GlfwWindow.h"


class Camera
{
public:
    Camera(enger::GlfwWindow& window);
    glm::vec3 velocity_;
    glm::vec3 position_;
    float pitch_ = 0.0f;
    float yaw_ = 0.0f;

    glm::mat4 viewMatrix() const;
    glm::mat4 rotationMatrix() const;

    void update();

private:
    void attachInputToWindow(enger::GlfwWindow& window);
    void attachCursor(enger::GlfwWindow& window);

    bool m_EnableMovement = false;

    double m_LastX = 0.0;
    double m_LastY = 0.0;
};
