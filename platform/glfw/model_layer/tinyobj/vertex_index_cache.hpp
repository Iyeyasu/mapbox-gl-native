#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include <mbgl/util/vectors.hpp>

template <>
struct std::hash<mbgl::vec3i>
{
  std::size_t operator()(const mbgl::vec3i& vec) const
  {
    return ((std::hash<int>()(vec[0])
             ^ (std::hash<int>()(vec[1]) << 1)) >> 1)
             ^ (std::hash<int>()(vec[2]) << 1);
  }
};


namespace custom
{
    class VertexIndexCache final
    {
    public:
        void clear();

        int32_t tryGet(const mbgl::vec3i& key) const;

        void set(mbgl::vec3i key, int32_t value);

    private:
        std::unordered_map<mbgl::vec3i, int32_t> m_cache{};
    };
}
