#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "shader_compiler.hpp"

#include <string>
#include <vector>

namespace render {
namespace {

std::string shaderLog(GLuint shader) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
        return {};
    std::vector<char> buffer(static_cast<std::size_t>(length));
    glGetShaderInfoLog(shader, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

std::string programLog(GLuint program) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length <= 1)
        return {};
    std::vector<char> buffer(static_cast<std::size_t>(length));
    glGetProgramInfoLog(program, length, nullptr, buffer.data());
    return std::string(buffer.data());
}

GLuint compileStage(GLenum type, std::string_view source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    const char* sourceData = source.data();
    const GLint sourceLength = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &sourceData, &sourceLength);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE)
        return shader;

    error = shaderLog(shader);
    glDeleteShader(shader);
    return 0;
}

} // namespace

bool OpenGLShaderCompiler::initialize() {
    initialized_ = gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) != 0;
    return initialized_;
}

ShaderCompileResult OpenGLShaderCompiler::compile(
    std::string_view vertexSource, std::string_view fragmentSource) {
    if (!initialized_)
        return {false, "OpenGL shader API is not initialized."};
    std::string error;
    const GLuint vertexShader = compileStage(GL_VERTEX_SHADER, vertexSource, error);
    if (vertexShader == 0)
        return {false, "Vertex shader compilation failed:\n" + error};

    const GLuint fragmentShader = compileStage(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return {false, "Fragment shader compilation failed:\n" + error};
    }

    const GLuint newProgram = glCreateProgram();
    glAttachShader(newProgram, vertexShader);
    glAttachShader(newProgram, fragmentShader);
    glLinkProgram(newProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        error = programLog(newProgram);
        glDeleteProgram(newProgram);
        return {false, "Shader program link failed:\n" + error};
    }

    if (program_ != 0)
        glDeleteProgram(program_);
    program_ = newProgram;
    return {true, "OpenGL shader program compiled and linked successfully."};
}

void OpenGLShaderCompiler::reset() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    initialized_ = false;
}

bool OpenGLViewportRenderer::initialize() {
    return true;
}

VertexDataResult OpenGLViewportRenderer::upload(const VertexData& data) {
    VertexDataResult validation = validateVertexData(data);
    if (!validation.success)
        return validation;

    GLuint newVertexArray = 0;
    GLuint newVertexBuffer = 0;
    GLuint newIndexBuffer = 0;
    glGenVertexArrays(1, &newVertexArray);
    glGenBuffers(1, &newVertexBuffer);
    glGenBuffers(1, &newIndexBuffer);
    if (newVertexArray == 0 || newVertexBuffer == 0 || newIndexBuffer == 0) {
        if (newIndexBuffer != 0) glDeleteBuffers(1, &newIndexBuffer);
        if (newVertexBuffer != 0) glDeleteBuffers(1, &newVertexBuffer);
        if (newVertexArray != 0) glDeleteVertexArrays(1, &newVertexArray);
        return {false, "Failed to allocate OpenGL vertex resources.", {}};
    }

    glBindVertexArray(newVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, newVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(validation.data.values.size() * sizeof(float)),
                 validation.data.values.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(validation.data.componentsPerVertex() * sizeof(float));
    std::size_t componentOffset = 0;
    for (const VertexAttribute& attribute : validation.data.attributes) {
        glEnableVertexAttribArray(static_cast<GLuint>(attribute.location));
        glVertexAttribPointer(static_cast<GLuint>(attribute.location), attribute.componentCount,
                              GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(componentOffset * sizeof(float)));
        componentOffset += static_cast<std::size_t>(attribute.componentCount);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newIndexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(validation.data.indices.size() * sizeof(std::uint32_t)),
                 validation.data.indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    reset();
    vertexArray_ = newVertexArray;
    vertexBuffer_ = newVertexBuffer;
    indexBuffer_ = newIndexBuffer;
    indexCount_ = static_cast<int>(validation.data.indices.size());
    validation.message = "Uploaded " + std::to_string(validation.data.vertexCount) +
                         " vertices, " + std::to_string(indexCount_) + " indices, and " +
                         std::to_string(validation.data.attributes.size()) + " attributes.";
    return validation;
}

void OpenGLViewportRenderer::render(
    unsigned int program, int x, int y, int width, int height) {
    if (width <= 0 || height <= 0)
        return;

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
    glViewport(x, y, width, height);
    glClearColor(0.035f, 0.045f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (program != 0 && vertexArray_ != 0) {
        glUseProgram(program);
        glBindVertexArray(vertexArray_);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        glUseProgram(0);
    }
    glDisable(GL_SCISSOR_TEST);
}

void OpenGLViewportRenderer::reset() {
    if (vertexBuffer_ != 0) {
        glDeleteBuffers(1, &vertexBuffer_);
        vertexBuffer_ = 0;
    }
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
        vertexArray_ = 0;
    }
    if (indexBuffer_ != 0) {
        glDeleteBuffers(1, &indexBuffer_);
        indexBuffer_ = 0;
    }
    indexCount_ = 0;
}

} // namespace render
