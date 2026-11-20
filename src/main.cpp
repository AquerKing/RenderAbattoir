#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "ui.hpp"

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
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    auto& logger = logsys::Logger::instance();
    logger.registerChannel("FileLoading");
    logger.registerChannel("ShaderCompilation");
    logger.registerChannel("ShaderParameters");
    logger.info("FileLoading", "Log system initialized; no resources have been loaded yet.");
    logger.warning("ShaderCompilation", "No shader program is selected for compilation.");
    logger.info("ShaderParameters", "No shader parameters have been applied.");

    int selectedRenderer = 0;
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
    auto& settings = settingsWindow.add<ui::VerticalLayout>();
    settings.add<ui::Text>("Renderer and shader parameters");
    settings.add<ui::Separator>();
    settings.add<ui::InputText>("Shader name", "basic.vert");
    settings.add<ui::Checkbox>("Enable VSync", true);
    settings.add<ui::RadioButton>("OpenGL", selectedRenderer, 0);
    settings.add<ui::RadioButton>("Vulkan", selectedRenderer, 1);
    settings.add<ui::ComboBox>("Renderer", std::vector<std::string>{"OpenGL", "Vulkan", "Software"}, &selectedRenderer);
    settings.add<ui::ComboBox>("Channels", std::vector<std::string>{"Color", "Depth", "Normals"}, &selectedChannels);

    ui::Window viewportWindow("Viewport");
    viewportWindow.add<ui::RenderViewport>("Viewport Canvas", [](ImVec2) {
        ImGui::TextUnformatted("Render viewport placeholder");
    });

    ui::Window outputWindow("Output");
    outputWindow.add<ui::LogView>(logger);

    bool defaultLayoutBuilt = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

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
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.10f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
