#pragma once

#include <glm/glm.hpp>

#include "Window.h"


class ENGER_EXPORT Camera
{
public:
    Camera(enger::Window& window);
    glm::vec3 velocity_ = glm::vec3(0.0f);
    glm::vec3 position_ = glm::vec3(0.0f);
    float pitch_ = 0.0f;
    float yaw_ = 0.0f;

    glm::mat4 viewMatrix() const;
    glm::mat4 rotationMatrix() const;

    void update();

private:
    enger::Window& m_Window;

    void attachInputToWindow(enger::Window& window);
    void attachCursor(enger::Window& window);

    bool m_EnableMovement = false;

    double m_LastX = 0.0;
    double m_LastY = 0.0;
};
