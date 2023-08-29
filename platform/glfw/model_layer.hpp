#pragma once

#include <mbgl/gl/attribute.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/gl/types.hpp>
#include <mbgl/gl/uniform.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/mat4.hpp>

#include <vector>

namespace custom {

using GLuint = mbgl::platform::GLuint;
using GLfloat = mbgl::platform::GLfloat;

class ModelLayer final : public mbgl::style::CustomLayerHost {
public:
    void initialize() override;
    void render(const mbgl::style::CustomLayerRenderParameters&) override;
    void contextLost() override;
    void deinitialize() override;

private:
    struct Mesh final {
        std::vector<uint32_t> indices{};
        std::vector<GLfloat> vertices{};
        std::vector<GLfloat> normals{};
        GLuint indexBuffer{};
        GLuint vertexBuffer{};
        GLuint normalBuffer{};
    };

    struct Model final {
        Mesh mesh{};
        mbgl::mat4 modelMatrix{};
    };

    void createProgram();
    void createMeshes();
    void destroyProgram();
    void destroyMeshes();

    // Shader program
    mbgl::gl::ProgramID m_program{};
    mbgl::gl::ShaderID m_vertexShader{};
    mbgl::gl::ShaderID m_fragmentShader{};

    // Attributes and uniforms
    mbgl::gl::AttributeLocation m_posAttribute{};
    mbgl::gl::AttributeLocation m_normAttribute{};
    mbgl::gl::UniformLocation m_mpvUniform{};

    // Data
    std::vector<Model> m_models{};
};
}
