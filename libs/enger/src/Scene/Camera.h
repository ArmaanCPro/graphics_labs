#pragma once

#include <glm/glm.hpp>

#include "GlfwWindow.h"


class ENGER_EXPORT Camera
{
public:
    Camera(enger::GlfwWindow& window);
    glm::vec3 velocity_ = glm::vec3(0.0f);
    glm::vec3 position_ = glm::vec3(0.0f);
    float pitch_ = 0.0f;
    float yaw_ = 0.0f;

    glm::mat4 viewMatrix() const;
    glm::mat4 rotationMatrix() const;

    void update();

private:
    enger::GlfwWindow& m_Window;

    void attachInputToWindow(enger::GlfwWindow& window);
    void attachCursor(enger::GlfwWindow& window);

    bool m_EnableMovement = false;

    double m_LastX = 0.0;
    double m_LastY = 0.0;
};
