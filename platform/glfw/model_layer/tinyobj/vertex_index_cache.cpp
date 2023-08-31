#include "vertex_index_cache.hpp"

namespace custom
{
    void VertexIndexCache::clear()
    {
        m_cache.clear();
    }

    int32_t VertexIndexCache::tryGet(const mbgl::vec3i& key) const
    {
        const auto& it = m_cache.find(key);
        if (it == m_cache.end())
        {
            return -1;
        }
        return it->second;
    }

    void VertexIndexCache::set(mbgl::vec3i key, int32_t value)
    {
        m_cache[key] = value;
    }
}
