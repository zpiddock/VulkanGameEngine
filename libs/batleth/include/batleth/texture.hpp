#pragma once

#include "image.hpp"
#include <memory>
#include <string>

#ifdef _WIN32
#ifdef BATLETH_EXPORTS
#define BATLETH_API __declspec(dllexport)
#else
#define BATLETH_API __declspec(dllimport)
#endif
#else
#define BATLETH_API
#endif

namespace batleth {
    enum class TextureType {
        Albedo,
        Normal,
        MetallicRoughness,
        Opacity,
        Unknown
    };

    /**
     * High-level texture combining Image + metadata
     */
    class BATLETH_API Texture {
    public:
        Texture(std::unique_ptr<Image> image, TextureType type, std::string filepath)
            : m_image(std::move(image))
            , m_type(type)
            , m_filepath(std::move(filepath)) {}

        [[nodiscard]] auto get_image() const -> const Image& { return *m_image; }
        [[nodiscard]] auto get_type() const -> TextureType { return m_type; }
        [[nodiscard]] auto get_filepath() const -> const std::string& { return m_filepath; }

    private:
        std::unique_ptr<Image> m_image;
        TextureType m_type;
        std::string m_filepath;
    };
} // namespace batleth
