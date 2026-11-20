#pragma once

#include <string>
#include <string_view>

#include "vertex_data.hpp"

namespace render {

struct ShaderCompileResult {
    bool success = false;
    std::string message;
};

class OpenGLShaderCompiler {
public:
    bool initialize();
    ShaderCompileResult compile(std::string_view vertexSource, std::string_view fragmentSource);
    void reset();

    unsigned int program() const { return program_; }
    bool initialized() const { return initialized_; }

private:
    unsigned int program_ = 0;
    bool initialized_ = false;
};

class OpenGLViewportRenderer {
public:
    bool initialize();
    VertexDataResult upload(const VertexData& data);
    void render(unsigned int program, int x, int y, int width, int height);
    void reset();

private:
    unsigned int vertexArray_ = 0;
    unsigned int vertexBuffer_ = 0;
    int vertexCount_ = 0;
};

} // namespace render
