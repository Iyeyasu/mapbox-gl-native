#include "model_layer.hpp"

using namespace mbgl::platform;

namespace custom {

static const GLchar* g_vertexShaderSource = R"MBGL_SHADER(
in vec3 in_pos;
in vec3 in_norm;

uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(in_pos + in_norm, 1);
}
)MBGL_SHADER";

static const GLchar* g_fragmentShaderSource = R"MBGL_SHADER(
in vec3 in_norm;

void main() {
    gl_FragColor = vec4(in_norm, 0.5);
}
)MBGL_SHADER";

void ModelLayer::initialize() {
    createProgram();
    createMeshes();
}

void ModelLayer::render(const mbgl::style::CustomLayerRenderParameters& parameters) {
    MBGL_CHECK_ERROR(glUseProgram(m_program));

    for (auto& model : m_models)
    {
        // Calculate MVP matrix
        mbgl::mat4 mvp{};
        mbgl::matrix::multiply(mvp, parameters.projectionMatrix, model.modelMatrix);
        mbgl::matrix::identity(mvp);
        mbgl::gl::bindUniform(m_mpvUniform, mvp);

        // Draw model
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.vertexBuffer));
        //MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.normalBuffer));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.mesh.indexBuffer));
        MBGL_CHECK_ERROR(glDrawElements(GL_TRIANGLES, model.mesh.indices.size(), GL_UNSIGNED_INT, nullptr));
    }
}

void ModelLayer::contextLost() {}

void ModelLayer::deinitialize() {
    destroyMeshes();
    destroyProgram();
}

void ModelLayer::createProgram() {
    m_program = MBGL_CHECK_ERROR(glCreateProgram());
    m_vertexShader = MBGL_CHECK_ERROR(glCreateShader(GL_VERTEX_SHADER));
    m_fragmentShader = MBGL_CHECK_ERROR(glCreateShader(GL_FRAGMENT_SHADER));

    MBGL_CHECK_ERROR(glShaderSource(m_vertexShader, 1, &g_vertexShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(m_vertexShader));
    MBGL_CHECK_ERROR(glAttachShader(m_program, m_vertexShader));
    MBGL_CHECK_ERROR(glShaderSource(m_fragmentShader, 1, &g_fragmentShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(m_fragmentShader));
    MBGL_CHECK_ERROR(glAttachShader(m_program, m_fragmentShader));
    MBGL_CHECK_ERROR(glLinkProgram(m_program));

    m_posAttribute = mbgl::gl::queryLocation(m_program, "in_pos").value();
    m_normAttribute = mbgl::gl::queryLocation(m_program, "in_norm").value();
    m_mpvUniform = mbgl::gl::uniformLocation(m_program, "u_mvp");
}

void ModelLayer::createMeshes() {
    destroyMeshes();

    Model mod = {};
    mod.mesh.vertices = { 0, 0.5, 0, 0.5, -0.5, 0, -0.5, -0.5, 0 };
    mod.mesh.normals = { 0, 1, 0, 0, 1, 0, 0, 1, 0 };
    mod.mesh.indices = { 0, 1, 2 };
    m_models.push_back(mod);

    for (auto& model : m_models) {
        MBGL_CHECK_ERROR(glGenBuffers(1, &model.mesh.indexBuffer));
        MBGL_CHECK_ERROR(glGenBuffers(1, &model.mesh.vertexBuffer));

        const size_t indexBufferSize = model.mesh.indices.size() * sizeof(decltype(model.mesh.indices)::value_type);
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.indexBuffer));
        MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, indexBufferSize, model.mesh.indices.data(), GL_STATIC_DRAW));

        const size_t vertexBufferSize = model.mesh.vertices.size() * sizeof(decltype(model.mesh.vertices)::value_type);
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.vertexBuffer));
        MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, model.mesh.vertices.data(), GL_STATIC_DRAW));
        MBGL_CHECK_ERROR(glEnableVertexAttribArray(m_posAttribute));
        MBGL_CHECK_ERROR(glVertexAttribPointer(m_posAttribute, 3, GL_FLOAT, GL_FALSE, 0, nullptr));

        // const size_t normalBufferSize = model.mesh.normals.size() * sizeof(decltype(model.mesh.normals)::value_type);
        // MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.normalBuffer));
        // MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, normalBufferSize, model.mesh.normals.data(), GL_STATIC_DRAW));
        // MBGL_CHECK_ERROR(glEnableVertexAttribArray(m_normAttribute));
        // MBGL_CHECK_ERROR(glVertexAttribPointer(m_normAttribute, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
    }
}

void ModelLayer::destroyProgram() {
    if (m_program) {
        MBGL_CHECK_ERROR(glDetachShader(m_program, m_vertexShader));
        MBGL_CHECK_ERROR(glDetachShader(m_program, m_fragmentShader));
        MBGL_CHECK_ERROR(glDeleteShader(m_vertexShader));
        MBGL_CHECK_ERROR(glDeleteShader(m_fragmentShader));
        MBGL_CHECK_ERROR(glDeleteProgram(m_program));
    }
}

void ModelLayer::destroyMeshes() {
    for (const auto& model : m_models) {
        if (model.mesh.indexBuffer) {
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &model.mesh.indexBuffer));
        }

        if (model.mesh.vertexBuffer) {
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &model.mesh.vertexBuffer));
        }

        if (model.mesh.normalBuffer) {
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &model.mesh.normalBuffer));
        }
    }

    m_models.clear();
}
}
