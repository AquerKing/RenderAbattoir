#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace render {

struct VertexAttribute {
    int location = 0;
    int componentCount = 3;
};

struct VertexData {
    std::vector<VertexAttribute> attributes;
    std::vector<float> values;
    std::vector<std::uint32_t> indices;
    std::size_t vertexCount = 0;

    std::size_t componentsPerVertex() const;
};

struct VertexDataResult {
    bool success = false;
    std::string message;
    VertexData data;
};

VertexDataResult validateVertexData(VertexData data);
VertexDataResult loadVertexDataFile(const std::string& path);

} // namespace render
