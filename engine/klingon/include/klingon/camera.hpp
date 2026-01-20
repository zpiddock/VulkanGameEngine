#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <ser20/ser20.hpp>

#ifdef _WIN32
#ifdef KLINGON_EXPORTS
#define KLINGON_API __declspec(dllexport)
#else
#define KLINGON_API __declspec(dllimport)
#endif
#else
#define KLINGON_API
#endif

namespace klingon {
    /**
     * Camera class for handling view and projection transformations.
     * Supports both orthographic and perspective projections.
     */
    class KLINGON_API Camera {
    public:
        Camera() = default;

        ~Camera() = default;

        Camera(const Camera &) = default;

        Camera &operator=(const Camera &) = default;

        Camera(Camera &&) = default;

        Camera &operator=(Camera &&) = default;

        /**
         * Set orthographic projection matrix
         * @param left Left clipping plane
         * @param right Right clipping plane
         * @param bottom Bottom clipping plane
         * @param top Top clipping plane
         * @param near Near clipping plane
         * @param far Far clipping plane
         */
        auto set_orthographic_projection(
            float left, float right,
            float bottom, float top,
            float near, float far
        ) -> void;

        /**
         * Set perspective projection matrix
         * @param fov_y Field of view in radians (vertical)
         * @param aspect_ratio Aspect ratio (width/height)
         * @param near Near clipping plane
         * @param far Far clipping plane
         */
        auto set_perspective_projection(
            float fov_y,
            float aspect_ratio,
            float near,
            float far
        ) -> void;

        /**
         * Set view matrix from position and direction
         * @param position Camera position
         * @param direction Look direction (normalized)
         * @param up Up vector (default: -Y axis for Vulkan)
         */
        auto set_view_direction(
            const glm::vec3 &position,
            const glm::vec3 &direction,
            const glm::vec3 &up = glm::vec3(0.f, -1.f, 0.f)
        ) -> void;

        /**
         * Set view matrix from position looking at target
         * @param position Camera position
         * @param target Point to look at
         * @param up Up vector (default: -Y axis for Vulkan)
         */
        auto set_view_target(
            const glm::vec3 &position,
            const glm::vec3 &target,
            const glm::vec3 &up = glm::vec3(0.f, -1.f, 0.f)
        ) -> void;

        /**
         * Set view matrix from position and Euler angles (YXZ rotation order)
         * @param position Camera position
         * @param rotation Euler angles (pitch, yaw, roll) in radians
         */
        auto set_view_yxz(
            const glm::vec3 &position,
            const glm::vec3 &rotation
        ) -> void;

        [[nodiscard]] auto get_projection() const -> const glm::mat4 & { return m_projection; }
        [[nodiscard]] auto get_view() const -> const glm::mat4 & { return m_view; }
        [[nodiscard]] auto get_view_projection() const -> glm::mat4 { return m_projection * m_view; }
        [[nodiscard]] auto get_inverse_view() const -> const glm::mat4 & { return m_inverse_view; }
        [[nodiscard]] auto get_position() const -> glm::vec3 { return glm::vec3{m_inverse_view[3]}; }

        template <class Archive>
        void serialize(Archive& ar) {
            ar(m_projection, m_view, m_inverse_view);
        }

    private:
        glm::mat4 m_projection{1.f};
        glm::mat4 m_view{1.f};
        glm::mat4 m_inverse_view{1.f};
    };
} // namespace klingon
