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

    int selectedRenderer = 0;
    int selectedItem = 0;
    std::vector<bool> selectedChannels{true, false, false};
    ui::DockSpace dockSpace;

    ui::Window controlsWindow("Controls");
    auto& controls = controlsWindow.add<ui::VerticalLayout>();
    controls.add<ui::Text>("A small reusable ImGui component demo");
    controls.add<ui::Separator>();
    controls.add<ui::InputText>("Shader name", "basic.vert");
    controls.add<ui::Checkbox>("Enable VSync", true);
    controls.add<ui::RadioButton>("OpenGL", selectedRenderer, 0);
    controls.add<ui::RadioButton>("Vulkan", selectedRenderer, 1);
    controls.add<ui::ComboBox>("Renderer", std::vector<std::string>{"OpenGL", "Vulkan", "Software"}, &selectedRenderer);
    controls.add<ui::ComboBox>("Channels", std::vector<std::string>{"Color", "Depth", "Normals"}, &selectedChannels);

    ui::Window sceneWindow("Scene");
    sceneWindow.add<ui::TreeView>(std::vector<ui::TreeView::Node>{
        {"Scene", {{"Camera", {}}, {"Mesh", {}}}},
        {"Shaders", {{"Vertex", {}}, {"Fragment", {}}}},
    });
    sceneWindow.add<ui::ListView>("Render targets", std::vector<std::string>{"Color", "Depth", "Normals"}, &selectedItem);

    ui::Window viewportWindow("Viewport");
    viewportWindow.add<ui::RenderViewport>("Viewport Canvas", [](ImVec2) {
        ImGui::TextUnformatted("Render viewport placeholder");
    });

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        dockSpace.draw();
        controlsWindow.setInitialDock(dockSpace.id()).draw();
        sceneWindow.setInitialDock(dockSpace.id()).draw();
        viewportWindow.setInitialDock(dockSpace.id()).draw();

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
