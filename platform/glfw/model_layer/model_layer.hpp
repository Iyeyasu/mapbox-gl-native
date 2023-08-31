#pragma once

#include "tinyobj/tinyobj_importer.hpp"

#include <mbgl/gl/attribute.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/gl/defines.hpp>
#include <mbgl/gl/types.hpp>
#include <mbgl/gl/uniform.hpp>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/tile_coordinate.hpp>

#include <vector>

namespace custom {

// A custom layer for rendering a 3D model
class ModelLayer final : public mbgl::style::CustomLayerHost {
public:
    ModelLayer(const std::string& objectFile);

    // mbgl::style::CustomLayerHost overrides
    void initialize() override;
    void render(const mbgl::style::CustomLayerRenderParameters&) override;
    void contextLost() override;
    void deinitialize() override;

private:
    // A struct for holding model transform, CPU mesh data, and the GPU mesh buffers
    struct Model final {
        mbgl::mat4 transform{};
        Mesh mesh{};

        // GPU data
        mbgl::platform::GLuint indexBuffer{};
        mbgl::platform::GLuint vertexBuffer{};
    };

    void createProgram();
    void destroyProgram();
    void createModel();
    void destroyModel();

    // Model importing
    TinyobjImporter m_objImporter{};
    std::string m_objectFile{};
    Model m_model{};

    // Shader program
    mbgl::gl::ProgramID m_program{};
    mbgl::gl::ShaderID m_vertexShader{};
    mbgl::gl::ShaderID m_fragmentShader{};

    // Attributes and uniforms
    mbgl::gl::AttributeLocation m_posAttribute{};
    mbgl::gl::AttributeLocation m_normAttribute{};
    mbgl::gl::UniformLocation m_mvpMatrixUniform{};
};
}
