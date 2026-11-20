#include "vertex_data.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace render {

std::size_t VertexData::componentsPerVertex() const {
    std::size_t result = 0;
    for (const VertexAttribute& attribute : attributes)
        result += static_cast<std::size_t>(std::max(attribute.componentCount, 0));
    return result;
}

VertexDataResult validateVertexData(VertexData data) {
    if (data.attributes.empty())
        return {false, "At least one vertex attribute is required.", {}};

    std::unordered_set<int> locations;
    for (const VertexAttribute& attribute : data.attributes) {
        if (attribute.location < 0)
            return {false, "Attribute locations must be non-negative.", {}};
        if (attribute.componentCount < 1 || attribute.componentCount > 4)
            return {false, "Attribute component counts must be between 1 and 4.", {}};
        if (!locations.insert(attribute.location).second)
            return {false, "Attribute locations must be unique.", {}};
    }

    if (data.vertexCount == 0)
        return {false, "At least one vertex is required.", {}};
    const std::size_t expectedValues = data.vertexCount * data.componentsPerVertex();
    if (data.values.size() != expectedValues)
        return {false, "Vertex value count does not match the attribute layout.", {}};
    return {true, "Vertex data is valid.", std::move(data)};
}

VertexDataResult loadVertexDataFile(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        return {false, "Unable to open vertex data file: " + path, {}};

    VertexData data;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        std::replace(line.begin(), line.end(), ',', ' ');

        std::istringstream stream(line);
        std::string keyword;
        if (!(stream >> keyword))
            continue;

        if (keyword == "attribute") {
            if (data.vertexCount != 0)
                return {false, "Attributes must appear before vertices at line " +
                                   std::to_string(lineNumber) + ".", {}};
            VertexAttribute attribute;
            if (!(stream >> attribute.location >> attribute.componentCount))
                return {false, "Invalid attribute at line " + std::to_string(lineNumber) + ".", {}};
            data.attributes.push_back(attribute);
        } else if (keyword == "vertex") {
            if (data.attributes.empty())
                return {false, "Declare attributes before vertices at line " +
                                   std::to_string(lineNumber) + ".", {}};
            const std::size_t valuesBefore = data.values.size();
            float value = 0.0f;
            while (stream >> value)
                data.values.push_back(value);
            if (data.values.size() - valuesBefore != data.componentsPerVertex())
                return {false, "Vertex component count mismatch at line " +
                                   std::to_string(lineNumber) + ".", {}};
            ++data.vertexCount;
        } else {
            return {false, "Unknown vertex data directive at line " +
                               std::to_string(lineNumber) + ": " + keyword, {}};
        }
    }

    VertexDataResult result = validateVertexData(std::move(data));
    if (result.success)
        result.message = "Loaded vertex data from " + path + ".";
    return result;
}

} // namespace render
