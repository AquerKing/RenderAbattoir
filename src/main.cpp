#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ui.hpp"
#include "shader_compiler.hpp"

#include <chrono>

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point LastActivity = Clock::now();

void markActivity(GLFWwindow*) { LastActivity = Clock::now(); }
void markCursorActivity(GLFWwindow*, double, double) { LastActivity = Clock::now(); }
void markMouseActivity(GLFWwindow*, int, int, int) { LastActivity = Clock::now(); }
void markScrollActivity(GLFWwindow*, double, double) { LastActivity = Clock::now(); }
void markKeyActivity(GLFWwindow*, int, int, int, int) { LastActivity = Clock::now(); }
void markCharacterActivity(GLFWwindow*, unsigned int) { LastActivity = Clock::now(); }
} // namespace

int main()
{
    if (!glfwInit())
        return 1;

    // OpenGL 4.1 is broadly available on modern desktop GPUs and is the
    // highest core profile supported by macOS, preserving cross-platform use.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Render Abattoir", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    // Install application callbacks first so the ImGui backend can chain them.
    glfwSetWindowRefreshCallback(window, markActivity);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* source, int, int) { markActivity(source); });
    glfwSetWindowPosCallback(window, [](GLFWwindow* source, int, int) { markActivity(source); });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* source, int) { markActivity(source); });
    glfwSetCursorPosCallback(window, markCursorActivity);
    glfwSetMouseButtonCallback(window, markMouseActivity);
    glfwSetScrollCallback(window, markScrollActivity);
    glfwSetKeyCallback(window, markKeyActivity);
    glfwSetCharCallback(window, markCharacterActivity);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    auto& logger = logsys::Logger::instance();
    logger.setWakeCallback([] { glfwPostEmptyEvent(); });
    logger.registerChannel("FileLoading");
    logger.registerChannel("ShaderCompilation");
    logger.registerChannel("ShaderParameters");
    logger.registerChannel("VertexData");
    logger.registerChannel("Rendering");
    logger.info("FileLoading", "Log system initialized; no resources have been loaded yet.");
    logger.warning("ShaderCompilation", "No shader program is selected for compilation.");
    logger.info("ShaderParameters", "No shader parameters have been applied.");
    render::OpenGLShaderCompiler shaderCompiler;
    if (!shaderCompiler.initialize())
        logger.error("ShaderCompilation", "Failed to initialize the OpenGL shader API.");
    render::OpenGLViewportRenderer viewportRenderer;
    if (!viewportRenderer.initialize())
        logger.error("Rendering", "Failed to initialize the OpenGL viewport renderer.");

    int selectedItem = 0;
    std::vector<bool> selectedChannels{true, false, false};
    ui::DockSpace dockSpace;

    ui::Window resourcesWindow("Resources");
    auto& resources = resourcesWindow.add<ui::VerticalLayout>();
    resources.add<ui::Text>("Loaded resources");
    resources.add<ui::Separator>();
    resources.add<ui::TreeView>(std::vector<ui::TreeView::Node>{
        {"Textures", {{"albedo.png", {}}, {"normal.png", {}}}},
        {"Meshes", {{"model.obj", {}}, {"sphere.gltf", {}}}},
    });

    ui::Window settingsWindow("Render Settings");
    settingsWindow.add<ui::RenderSettings>(shaderCompiler, viewportRenderer, logger,
        [](bool enabled) { glfwSwapInterval(enabled ? 1 : 0); });

    ui::Window viewportWindow("Viewport");
    viewportWindow.add<ui::RenderViewport>("Viewport Canvas", viewportRenderer, shaderCompiler);

    ui::Window outputWindow("Output");
    outputWindow.add<ui::LogView>(logger);

    bool defaultLayoutBuilt = false;
    constexpr auto ActiveDuration = std::chrono::milliseconds(250);
    constexpr double ActiveFrameSeconds = 1.0 / 60.0;
    constexpr double IdleWaitSeconds = 0.25;

    while (!glfwWindowShouldClose(window))
    {
        const bool active = Clock::now() - LastActivity < ActiveDuration;
        glfwWaitEventsTimeout(active ? ActiveFrameSeconds : IdleWaitSeconds);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        dockSpace.draw();
        if (!defaultLayoutBuilt) {
            ui::DefaultDockLayout::buildHLayout(dockSpace.id(), "Resources", "Render Settings", "Viewport", "Output");
            defaultLayoutBuilt = true;
        }
        resourcesWindow.draw();
        settingsWindow.draw();
        viewportWindow.draw();
        outputWindow.draw();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        if (displayWidth > 0 && displayHeight > 0) {
            glViewport(0, 0, displayWidth, displayHeight);
            glClearColor(0.10f, 0.12f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    logger.setWakeCallback({});
    viewportRenderer.reset();
    shaderCompiler.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
