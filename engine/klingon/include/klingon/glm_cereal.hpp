#pragma once
#include <cereal/cereal.hpp>
#include <glm/glm.hpp>

namespace cereal {

    template <class Archive>
    void serialize(Archive& ar, glm::vec2& v) {
        ar(v.x, v.y);
    }

    template <class Archive>
    void serialize(Archive& ar, glm::vec3& v) {
        ar(v.x, v.y, v.z);
    }

    template <class Archive>
    void serialize(Archive& ar, glm::vec4& v) {
        ar(v.x, v.y, v.z, v.w);
    }

    template <class Archive>
    void serialize(Archive& ar, glm::mat4& m) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                ar(m[i][j]);
    }

} // namespace cereal