#include "model_layer.hpp"

#include <iostream>

using namespace mbgl::platform;

namespace custom {

// Helper functions
namespace {

// Source for vertex shader
static const GLchar* g_vertexShaderSource = R"MBGL_SHADER(
in vec3 in_pos;
in vec3 in_norm;

uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(in_pos, 1);
}
)MBGL_SHADER";

// Source for fragment shader
static const GLchar* g_fragmentShaderSource = R"MBGL_SHADER(
in vec3 in_norm;

void main() {
    gl_FragColor = vec4(in_norm, 0.5);
}
)MBGL_SHADER";

// Prints a matrix for debugging.
void printMatrix(const mbgl::mat4& matrix) {
    std::cout << matrix[0] << " " << matrix[4] << " " << matrix[8] << " " << matrix[12] << std::endl;
    std::cout << matrix[1] << " " << matrix[5] << " " << matrix[9] << " " << matrix[13] << std::endl;
    std::cout << matrix[2] << " " << matrix[6] << " " << matrix[10] << " " << matrix[14] << std::endl;
    std::cout << matrix[3] << " " << matrix[7] << " " << matrix[11] << " " << matrix[15] << std::endl;
}

// Returns a matrix that converts mercator coordinates to clip space coordinates.
mbgl::mat4 getMercatorViewProjectionMatrix(const mbgl::mat4& projectionMatrix, double zoom) {
    // Get the world scale
    const auto scale = std::pow(2.0, zoom);
    const auto worldSize = mbgl::Projection::worldSize(scale);

    // Apply the world scale to the projection matrix
    auto mercatorMatrix = mbgl::mat4{};
    mbgl::matrix::scale(mercatorMatrix, projectionMatrix, worldSize, worldSize, worldSize / scale);
    return mercatorMatrix;
}

// Returns a model matrix that converts object space coordinates to mercator coordinates.
// The given latitude and longitude define the origin of the object.
// The object can also be scaled and rotated along its Z-axis.
mbgl::mat4 getModelMatrix(double latitude, double longitude, double scale, double rotationZ) {
    // Position model in mercator coordinates based on latitude and longitude
    const auto modelAltitude = 0;
    const auto mercator = mbgl::TileCoordinate::fromLatLng(modelAltitude, {latitude, longitude});

    // Calculate how many meters are in one mercator unit. Based on JS meterInMercatorCoordinateUnits()
    const auto mercatorScale = 1 / std::cos(latitude * M_PI / 180);
    const auto meterInMercator = 1.0f / (mbgl::util::EARTH_RADIUS_M * mbgl::util::M2PI) * mercatorScale;
    const auto finalScale = scale * meterInMercator;

    // Create model matrix from translation, rotation, and scale
    auto modelMatrix = mbgl::mat4{};
    mbgl::matrix::identity(modelMatrix);
    mbgl::matrix::translate(modelMatrix, modelMatrix, mercator.p.x, mercator.p.y, mercator.z);
    mbgl::matrix::rotate_z(modelMatrix, modelMatrix, rotationZ);
    mbgl::matrix::scale(modelMatrix, modelMatrix, finalScale, -finalScale, finalScale);
    printMatrix(modelMatrix);
    return modelMatrix;
}
}

void ModelLayer::initialize() {
    createProgram();
    createMeshes();
}

void ModelLayer::render(const mbgl::style::CustomLayerRenderParameters& parameters) {
    MBGL_CHECK_ERROR(glUseProgram(m_program));

    // Create a mercator matrix to transform from mercator space to projection space.
    // Adapted from transform.js
    const auto mercatorViewProjectionMatrix = getMercatorViewProjectionMatrix(parameters.projectionMatrix, parameters.zoom);

    for (auto& model : m_models)
    {
        // Calculate MVP matrix
        auto mvp = mbgl::mat4{};
        mbgl::matrix::multiply(mvp, mercatorViewProjectionMatrix, model.modelMatrix);
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

    m_posAttribute = mbgl::gl::queryLocation(m_program, "in_pos").value_or(0);
    m_normAttribute = mbgl::gl::queryLocation(m_program, "in_norm").value_or(0);
    m_mpvUniform = mbgl::gl::uniformLocation(m_program, "u_mvp");
}

void ModelLayer::createMeshes() {
    destroyMeshes();

    // Load model CPU data
    auto mod = Model{};
    mod.mesh.vertices = { 0, 0.5, 0, 0.5, -0.5, 0, -0.5, -0.5, 0 };
    mod.mesh.normals = { 0, 1, 0, 0, 1, 0, 0, 1, 0 };
    mod.mesh.indices = { 0, 1, 2 };
    mod.modelMatrix = getModelMatrix(60.1714, 24.94415, 100.0, 0.0f); // Place the model in Helsinki Central Railway Station market
    m_models.push_back(mod);

    // Create model GPU vertex and index buffers
    for (auto& model : m_models) {
        MBGL_CHECK_ERROR(glGenBuffers(1, &model.mesh.indexBuffer));
        MBGL_CHECK_ERROR(glGenBuffers(1, &model.mesh.vertexBuffer));

        const auto indexBufferSize = model.mesh.indices.size() * sizeof(decltype(model.mesh.indices)::value_type);
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model.mesh.indexBuffer));
        MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, indexBufferSize, model.mesh.indices.data(), GL_STATIC_DRAW));

        const auto vertexBufferSize = model.mesh.vertices.size() * sizeof(decltype(model.mesh.vertices)::value_type);
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
