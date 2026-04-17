#include "Camera.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

Camera::Camera(enger::GlfwWindow& window)
{
    attachInputToWindow(window);
    attachCursor(window);
}

glm::mat4 Camera::viewMatrix() const
{
    // We need to move the world in opposite direction as camera, so we create the camera model matrix & invert it
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position_);
    glm::mat4 cameraRotation = rotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::rotationMatrix() const
{
    // typical FPS style camera where we join the pitch & yaw rotations
    glm::quat pitchRotation = glm::angleAxis(glm::radians(pitch_), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yawRotation = glm::angleAxis(glm::radians(yaw_), glm::vec3(0.0f, -1.0f, 0.0f));

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::update()
{
    glm::mat4 cameraRotation = rotationMatrix();
    position_ += glm::vec3(cameraRotation * glm::vec4(velocity_ * 0.05f, 0.0f));
}

void Camera::attachInputToWindow(enger::GlfwWindow& window)
{
    window.setInputCallback([&](int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
        if (!m_EnableMovement)
            return;

        if (action == GLFW_PRESS)
        {
            if (key == GLFW_KEY_W)
                velocity_.z = -1.0f;
            if (key == GLFW_KEY_S)
                velocity_.z = 1.0f;
            if (key == GLFW_KEY_A)
                velocity_.x = -1.0f;
            if (key == GLFW_KEY_D)
                velocity_.x = 1.0f;

            if (mods == GLFW_MOD_SHIFT)
            {
                static constexpr float kSpeedBoost = 3.0f;
                velocity_ *= kSpeedBoost;
                velocity_ = glm::clamp(velocity_, -kSpeedBoost, kSpeedBoost);
            }
            else if (mods == GLFW_MOD_CONTROL)
            {
                static constexpr float kSpeedSlow = 0.1f;
                velocity_ *= kSpeedSlow;
                velocity_ = glm::clamp(velocity_, -kSpeedSlow, kSpeedSlow);
            }
        }

        if (action == GLFW_RELEASE)
        {
            if (key == GLFW_KEY_W || key == GLFW_KEY_S)
                velocity_.z = 0.0f;
            if (key == GLFW_KEY_A || key == GLFW_KEY_D)
                velocity_.x = 0.0f;

            if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
                velocity_ = glm::clamp(velocity_, -1.0f, 1.0f);
            if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
                velocity_ = glm::clamp(velocity_ * 10.0f, -1.0f, 1.0f);
        }
    });

    window.setMouseCallback([&](int key, int action, [[maybe_unused]] int mods) {
        if (key == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            window.disableCursor();
            m_EnableMovement = true;
        }
        else if (key == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            window.enableCursor();
            m_EnableMovement = false;
            velocity_ = glm::vec3(0.0f);
        }
    });
}

void Camera::attachCursor(enger::GlfwWindow& window)
{
    window.setCursorPosCallback([&](double xpos, double ypos) {
        if (m_EnableMovement)
        {
            yaw_ += static_cast<float>(xpos - m_LastX) / 5.0f;
            pitch_ -= static_cast<float>(ypos - m_LastY) / 5.0f;
        }

        m_LastX = xpos;
        m_LastY = ypos;
    });
}
