#include "klingon/camera.hpp"
#include <cassert>
#include <limits>

namespace klingon {

auto Camera::set_orthographic_projection(
    float left, float right,
    float bottom, float top,
    float near, float far
) -> void {
    m_projection = glm::mat4{1.0f};
    m_projection[0][0] = 2.f / (right - left);
    m_projection[1][1] = 2.f / (bottom - top);
    m_projection[2][2] = 1.f / (far - near);
    m_projection[3][0] = -(right + left) / (right - left);
    m_projection[3][1] = -(bottom + top) / (bottom - top);
    m_projection[3][2] = -near / (far - near);
}

auto Camera::set_perspective_projection(
    float fov_y,
    float aspect_ratio,
    float near,
    float far
) -> void {
    assert(glm::abs(aspect_ratio - std::numeric_limits<float>::epsilon()) > 0.0f);
    const float tan_half_fovy = std::tan(fov_y / 2.f);
    m_projection = glm::mat4{0.0f};
    m_projection[0][0] = 1.f / (aspect_ratio * tan_half_fovy);
    m_projection[1][1] = 1.f / tan_half_fovy;
    m_projection[2][2] = far / (far - near);
    m_projection[2][3] = 1.f;
    m_projection[3][2] = -(far * near) / (far - near);
}

auto Camera::set_view_direction(
    const glm::vec3& position,
    const glm::vec3& direction,
    const glm::vec3& up
) -> void {
    const glm::vec3 w{glm::normalize(direction)};
    const glm::vec3 u{glm::normalize(glm::cross(w, up))};
    const glm::vec3 v{glm::cross(w, u)};

    m_view = glm::mat4{1.f};
    m_view[0][0] = u.x;
    m_view[1][0] = u.y;
    m_view[2][0] = u.z;
    m_view[0][1] = v.x;
    m_view[1][1] = v.y;
    m_view[2][1] = v.z;
    m_view[0][2] = w.x;
    m_view[1][2] = w.y;
    m_view[2][2] = w.z;
    m_view[3][0] = -glm::dot(u, position);
    m_view[3][1] = -glm::dot(v, position);
    m_view[3][2] = -glm::dot(w, position);
}

auto Camera::set_view_target(
    const glm::vec3& position,
    const glm::vec3& target,
    const glm::vec3& up
) -> void {
    set_view_direction(position, target - position, up);
}

auto Camera::set_view_yxz(
    const glm::vec3& position,
    const glm::vec3& rotation
) -> void {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    const glm::vec3 u{(c1 * c3 + s1 * s2 * s3), (c2 * s3), (c1 * s2 * s3 - c3 * s1)};
    const glm::vec3 v{(c3 * s1 * s2 - c1 * s3), (c2 * c3), (c1 * c3 * s2 + s1 * s3)};
    const glm::vec3 w{(c2 * s1), (-s2), (c1 * c2)};
    m_view = glm::mat4{1.f};
    m_view[0][0] = u.x;
    m_view[1][0] = u.y;
    m_view[2][0] = u.z;
    m_view[0][1] = v.x;
    m_view[1][1] = v.y;
    m_view[2][1] = v.z;
    m_view[0][2] = w.x;
    m_view[1][2] = w.y;
    m_view[2][2] = w.z;
    m_view[3][0] = -glm::dot(u, position);
    m_view[3][1] = -glm::dot(v, position);
    m_view[3][2] = -glm::dot(w, position);
}

} // namespace klingon
