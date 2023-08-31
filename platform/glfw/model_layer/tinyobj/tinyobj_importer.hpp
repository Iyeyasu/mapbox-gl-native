#pragma once

#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/vectors.hpp>

#include "tiny_obj_loader.h"
#include "vertex_index_cache.hpp"

namespace custom
{
    struct Mesh final {
        std::vector<uint32_t> indices{};
        std::vector<mbgl::vec3f> positions{};
        std::vector<mbgl::vec3f> normals{};
    };

    class TinyobjImporter final {
    public:
        bool importMesh(const std::string& filepath, Mesh& outMesh);

    private:
        void importMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib, Mesh& outMesh);

        void calculateNormals(Mesh& outMesh);

        VertexIndexCache m_vertexIndexCache{};
    };
}  // namespace valo::renderer
