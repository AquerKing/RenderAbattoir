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
    constexpr float vertices[] = {
         0.0f,  0.72f, 1.0f, 0.25f, 0.20f,
        -0.72f, -0.62f, 0.20f, 0.75f, 1.0f,
         0.72f, -0.62f, 0.35f, 1.0f, 0.40f,
    };

    glGenVertexArrays(1, &vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    if (vertexArray_ == 0 || vertexBuffer_ == 0)
        return false;

    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<const void*>(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return true;
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
        glDrawArrays(GL_TRIANGLES, 0, 3);
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
}

} // namespace render
