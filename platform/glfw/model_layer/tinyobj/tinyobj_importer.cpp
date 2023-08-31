#include "tinyobj_importer.hpp"

#include <mbgl/util/logging.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <filesystem>

namespace custom
{
    bool TinyobjImporter::importMesh(const std::string& filepath, Mesh& outMesh)
    {
        mbgl::Log::Debug(mbgl::Event::General, "Loading .obj file %s...", filepath.c_str());

        outMesh.indices.clear();
        outMesh.positions.clear();
        outMesh.normals.clear();

        auto readerConfig            = tinyobj::ObjReaderConfig{};
        readerConfig.triangulate     = true;

        auto reader = tinyobj::ObjReader{};
        if (!reader.ParseFromFile(filepath, readerConfig)) {
            mbgl::Log::Error(mbgl::Event::General, "TinyObj error: %s", reader.Error().c_str());
            return false;
        }

        if (!reader.Warning().empty()) {
            mbgl::Log::Warning(mbgl::Event::General, "TinyObj warning: %s", reader.Warning().c_str());
        }

        m_vertexIndexCache.clear();

        const auto& shapes = reader.GetShapes();
        const auto& attrib = reader.GetAttrib();
        for (const auto& shape : shapes) {
            importMesh(shape.mesh, attrib, outMesh);
        }

        // If vertices don't have proper normals, generate them
        if (outMesh.normals.size() != outMesh.positions.size()) {
            calculateNormals(outMesh);
        }

        if (outMesh.indices.empty()) {
            mbgl::Log::Error(mbgl::Event::General, "TinyObj error when importing file %s: index buffer is empty", filepath.c_str());
            return false;
        }

        if (outMesh.positions.empty()) {
            mbgl::Log::Error(mbgl::Event::General, "TinyObj error when importing file %s: position buffer is empty", filepath.c_str());
            return false;
        }

        mbgl::Log::Debug(mbgl::Event::General, "Successfully loaded .obj file %s!", filepath.c_str());
        return true;
    }

    void TinyobjImporter::importMesh(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib, Mesh& outMesh) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            auto triangle = mbgl::vec3i{};
            for (uint32_t j = 0; j < 3; ++j) {
                // Get indices for attributes. If normals shouldn't be exported,
                // ignore the normal index
                const auto objAttrIndices = mesh.indices[i + j];
                const auto attrIndices     = mbgl::vec3i{
                    objAttrIndices.vertex_index,
                    objAttrIndices.normal_index,
                    objAttrIndices.texcoord_index};

                // If vertex is already in the cache, don't add to their buffers again
                const auto cachedTriIndex = m_vertexIndexCache.tryGet(attrIndices);
                if (cachedTriIndex != -1) {
                    triangle[j] = cachedTriIndex;
                    continue;
                }

                // If the vertex is not in the cache, create a new vertex and add
                // the index to the cache
                triangle[j] = static_cast<uint32_t>(outMesh.positions.size());
                m_vertexIndexCache.set(attrIndices, triangle[j]);

                // Add vertex position
                const auto vertexIndex = static_cast<size_t>(objAttrIndices.vertex_index);
                const auto vx           = attrib.vertices[3 * vertexIndex];
                const auto vy           = attrib.vertices[3 * vertexIndex + 1];
                const auto vz           = attrib.vertices[3 * vertexIndex + 2];
                outMesh.positions.emplace_back(mbgl::vec3f{vx, vy, vz});

                // Add (optional) vertex normal
                if (objAttrIndices.normal_index >= 0) {
                    const auto normalIndex = static_cast<size_t>(objAttrIndices.normal_index);
                    const auto nx           = attrib.normals[3 * normalIndex];
                    const auto ny           = attrib.normals[3 * normalIndex + 1];
                    const auto nz           = attrib.normals[3 * normalIndex + 2];
                    outMesh.normals.emplace_back(mbgl::vec3f{nx, ny, nz});
                }
            }
            outMesh.indices.emplace_back(triangle[0]);
            outMesh.indices.emplace_back(triangle[2]);
            outMesh.indices.emplace_back(triangle[1]);
        }
    }

    void TinyobjImporter::calculateNormals(Mesh& outMesh) {
        outMesh.normals.resize(outMesh.positions.size(), {0, 0, 0});

        // Primitives with no triangles do not contribute
        if (outMesh.indices.size() < 3) {
            return;
        }

        for (size_t i = 0; i < outMesh.indices.size() - 2; i += 3) {
            const auto idx1 = outMesh.indices[i];
            const auto idx2 = outMesh.indices[i + 1];
            const auto idx3 = outMesh.indices[i + 2];
            const auto v0   = outMesh.positions[idx1];
            const auto v1   = outMesh.positions[idx2];
            const auto v2   = outMesh.positions[idx3];

            // Calculate cross product
            const auto a = mbgl::vec3f{v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
            const auto b = mbgl::vec3f{v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
            const auto normal = mbgl::vec3f{{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};

            // Add normal to each vertex, weighted by the area of the triangle (length of the normal)
            outMesh.normals[idx1] = {outMesh.normals[idx1][0] + normal[0], outMesh.normals[idx1][1] + normal[1], outMesh.normals[idx1][2] + normal[2]};
            outMesh.normals[idx2] = {outMesh.normals[idx2][0] + normal[0], outMesh.normals[idx2][1] + normal[1], outMesh.normals[idx2][2] + normal[2]};
            outMesh.normals[idx3] = {outMesh.normals[idx3][0] + normal[0], outMesh.normals[idx3][1] + normal[1], outMesh.normals[idx3][2] + normal[2]};
        }

        for (auto& norm : outMesh.normals) {
            const auto length = std::sqrt(norm[0] * norm[0] + norm[1] * norm[1] + norm[2] * norm[2]);
            norm = {norm[0] / length, norm[1] / length, norm[2] / length};
        }
    }

}
