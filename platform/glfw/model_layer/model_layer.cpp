#include "model_layer.hpp"

#include <iostream>

#include <mbgl/util/logging.hpp>

using namespace mbgl::platform;

namespace custom {

// Helper functions
namespace {
// Source for vertex shader
static const GLchar* g_vertexShaderSource = R"MBGL_SHADER(
in vec3 in_pos;
in vec3 in_norm;

uniform mat4 u_mvpMatrix;

varying vec3 normal;

void main() {
    normal = in_norm;
    gl_Position = u_mvpMatrix * vec4(in_pos, 1);
}
)MBGL_SHADER";

// Source for fragment shader
static const GLchar* g_fragmentShaderSource = R"MBGL_SHADER(
in vec3 normal;

void main() {
    const vec3 lightDir = vec3(0, 1, 0);
    const vec3 color = vec3(0.5, 0.9, 1);
    const float colorIntensity = 0.3;
    const float alpha = 0.5;

    // Attenuate based on normal and light dir. But ensure that all surfaces get some light
    float attenuation = max(dot(-lightDir, normal), 0.4);

    vec3 finalColor = color * colorIntensity * attenuation;
    gl_FragColor = vec4(finalColor, alpha);
}
)MBGL_SHADER";

// Calculate how many meters are in one mercator unit (based on JS meterInMercatorCoordinateUnits())
double metersInMercator(double latitude) {
    return 1.0f / (mbgl::util::EARTH_RADIUS_M * mbgl::util::M2PI * std::cos(latitude * M_PI / 180));
}

// Returns a matrix that converts mercator coordinates to clip space coordinates.
mbgl::mat4 getMercatorViewProjectionMatrix(const mbgl::style::CustomLayerRenderParameters& parameters) {
    // Get the world scale
    const auto scale = std::pow(2.0, parameters.zoom);
    const auto worldSize = mbgl::Projection::worldSize(scale);

    // Apply the world scale to the projection matrix
    auto mercatorMatrix = mbgl::mat4{};
    mbgl::matrix::scale(mercatorMatrix, parameters.projectionMatrix, worldSize, worldSize, 1.0f / metersInMercator(parameters.latitude));
    return mercatorMatrix;
}

// Returns a model matrix that converts object space coordinates to mercator coordinates.
// The given latitude and longitude define the origin of the object.
// The object can also be scaled and rotated along its X-axis.
mbgl::mat4 getModelMatrix(double latitude, double longitude, double scale, double rotationX) {
    // Position model in mercator coordinates based on latitude and longitude
    const auto modelAltitude = 0;
    const auto mercator = mbgl::TileCoordinate::fromLatLng(modelAltitude, {latitude, longitude});
    const auto finalScale = scale * metersInMercator(latitude);

    // Create model matrix from translation, rotation, and scale
    auto modelMatrix = mbgl::mat4{};
    mbgl::matrix::identity(modelMatrix);
    mbgl::matrix::translate(modelMatrix, modelMatrix, mercator.p.x, mercator.p.y, mercator.z);
    mbgl::matrix::rotate_x(modelMatrix, modelMatrix, rotationX);
    mbgl::matrix::scale(modelMatrix, modelMatrix, finalScale, finalScale, finalScale);
    return modelMatrix;
}

// Checks whether a shader program was correctly linked
// Taken from example_custom_layer.cpp
bool checkLinkStatus(GLuint program) {
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        GLchar infoLog[maxLength];
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
        mbgl::Log::Error(mbgl::Event::General, "Failed to link shader program: %s", &infoLog[0]);
    }
    return isLinked;
}

// Checks whether a shader was successfully compiled
// Taken from example_custom_layer.cpp
bool checkCompileStatus(GLuint shader) {
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        GLchar errorLog[maxLength];
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
        mbgl::Log::Error(mbgl::Event::General, "Failed to link shader: %s", &errorLog[0]);
    }
    return isCompiled;
}
}

ModelLayer::ModelLayer(const std::string& objectFile) :
    m_objectFile(objectFile)
{
}

void ModelLayer::initialize() {
    createProgram();
    createModel();
}

void ModelLayer::render(const mbgl::style::CustomLayerRenderParameters& parameters) {
    // Ensure transparency is on
    MBGL_CHECK_ERROR(glEnable(GL_BLEND));

    // Ensure depth testing but no writes
    MBGL_CHECK_ERROR(glEnable(GL_DEPTH_TEST));
    MBGL_CHECK_ERROR(glDepthMask(GL_FALSE));
    MBGL_CHECK_ERROR(glDepthFunc(GL_LEQUAL));

    // Ensure correct culling
    MBGL_CHECK_ERROR(glDisable(GL_CULL_FACE));
    MBGL_CHECK_ERROR(glCullFace(GL_BACK));
    MBGL_CHECK_ERROR(glFrontFace(GL_CCW));

    // Create a mercator matrix to transform from mercator space to projection space.
    // Adapted from transform.js
    const auto mercatorViewProjectionMatrix = getMercatorViewProjectionMatrix(parameters);

    // Render model
    MBGL_CHECK_ERROR(glUseProgram(m_program));
    // Calculate MVP matrix
    auto mvp = mbgl::mat4{};
    mbgl::matrix::multiply(mvp, mercatorViewProjectionMatrix, m_model.transform);
    mbgl::gl::bindUniform(m_mvpMatrixUniform, mvp);

    // Draw model
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, m_model.vertexBuffer));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_model.indexBuffer));
    MBGL_CHECK_ERROR(glDrawElements(GL_TRIANGLES, m_model.mesh.indices.size(), GL_UNSIGNED_INT, nullptr));
}

void ModelLayer::contextLost() {}

void ModelLayer::deinitialize() {
    destroyModel();
    destroyProgram();
}

void ModelLayer::createProgram() {
    destroyProgram();

    m_program = MBGL_CHECK_ERROR(glCreateProgram());

    // Compile shaders
    m_vertexShader = MBGL_CHECK_ERROR(glCreateShader(GL_VERTEX_SHADER));
    MBGL_CHECK_ERROR(glShaderSource(m_vertexShader, 1, &g_vertexShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(m_vertexShader));
    checkCompileStatus(m_vertexShader);
    MBGL_CHECK_ERROR(glAttachShader(m_program, m_vertexShader));

    m_fragmentShader = MBGL_CHECK_ERROR(glCreateShader(GL_FRAGMENT_SHADER));
    MBGL_CHECK_ERROR(glShaderSource(m_fragmentShader, 1, &g_fragmentShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(m_fragmentShader));
    checkCompileStatus(m_fragmentShader);
    MBGL_CHECK_ERROR(glAttachShader(m_program, m_fragmentShader));

    // Bind attribute locations
    m_posAttribute = 0;
    m_normAttribute = 1;
    MBGL_CHECK_ERROR(glBindAttribLocation(m_program, 0, "in_pos"));
    MBGL_CHECK_ERROR(glBindAttribLocation(m_program, 1, "in_norm"));

    // Link program and get uniform locations
    MBGL_CHECK_ERROR(glLinkProgram(m_program));
    checkLinkStatus(m_program);

    m_mvpMatrixUniform = glGetUniformLocation(m_program, "u_mvpMatrix");
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

void ModelLayer::createModel() {
    destroyModel();

    // Import the mesh for the model
    if (!m_objImporter.importMesh(m_objectFile, m_model.mesh))
    {
        // If importing fails, draw a humble triangle
        m_model.mesh.indices = { 0, 1, 2 };
        m_model.mesh.positions = { {0, 1, 0}, {1, -1, 0}, {-1, -1, 0} };
        m_model.mesh.normals = { {0, 1, 0}, {0, 1, 0}, {0, 1, 0} };
    }

    // Place the model in Helsinki Central Railway Station market
    m_model.transform = getModelMatrix(60.1712, 24.9441, 10.0, 0.5f * M_PI);

    // Create model GPU vertex and index buffers
    // Combine positions and normals to a single buffer
    std::vector<mbgl::vec3f> vertexData{};
    vertexData.clear();
    vertexData.reserve(m_model.mesh.positions.size() + m_model.mesh.normals.size());

    for (const auto& position : m_model.mesh.positions) {
        vertexData.push_back(position);
    }

    for (const auto& normal : m_model.mesh.normals) {
        vertexData.push_back(normal);
    }

    // Create and initialize index buffer
    const auto indexBufferSize = m_model.mesh.indices.size() * sizeof(decltype(m_model.mesh.indices)::value_type);
    MBGL_CHECK_ERROR(glGenBuffers(1, &m_model.indexBuffer));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, m_model.indexBuffer));
    MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, indexBufferSize, m_model.mesh.indices.data(), GL_STATIC_DRAW));

    // Create and initialize vertex buffer
    const auto positionBufferSize = m_model.mesh.positions.size() * sizeof(decltype(m_model.mesh.positions)::value_type);
    const auto normalBufferSize = m_model.mesh.normals.size() * sizeof(decltype(m_model.mesh.normals)::value_type);
    const auto vertexBufferSize = positionBufferSize + normalBufferSize;
    MBGL_CHECK_ERROR(glGenBuffers(1, &m_model.vertexBuffer));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, m_model.vertexBuffer));
    MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, vertexData.data(), GL_STATIC_DRAW));

    // Set up attribute pointers
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(m_posAttribute));
    MBGL_CHECK_ERROR(glVertexAttribPointer(m_posAttribute, 3, GL_FLOAT, GL_FALSE, 0, nullptr));
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(m_normAttribute));
    MBGL_CHECK_ERROR(glVertexAttribPointer(m_normAttribute, 3, GL_FLOAT, GL_FALSE, 0, (void*)positionBufferSize));
}

void ModelLayer::destroyModel() {
    if (m_model.indexBuffer) {
        MBGL_CHECK_ERROR(glDeleteBuffers(1, &m_model.indexBuffer));
    }

    if (m_model.vertexBuffer) {
        MBGL_CHECK_ERROR(glDeleteBuffers(1, &m_model.vertexBuffer));
    }

    m_model.mesh.indices.clear();
    m_model.mesh.positions.clear();
}
}
