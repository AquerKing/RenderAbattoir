#pragma once

#include "imgui.h"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {

class Component {
public:
    virtual ~Component() = default;
    virtual void draw() = 0;
};

// A reusable child container. Nested containers let a composite control own
// arbitrary widgets without exposing raw ImGui calls at its call site.
class VerticalLayout final : public Component {
public:
    template <typename T, typename... Args>
    T& add(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *component;
        children_.push_back(std::move(component));
        return result;
    }

    void draw() override {
        for (const auto& child : children_)
            child->draw();
    }

private:
    std::vector<std::unique_ptr<Component>> children_;
};

// Creates the application's root dockspace. Call draw() once per frame before
// drawing dockable Window objects.
class DockSpace final : public Component {
public:
    explicit DockSpace(ImGuiDockNodeFlags flags = ImGuiDockNodeFlags_PassthruCentralNode)
        : flags_(flags) {}

    void draw() override {
        id_ = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), flags_);
    }

    ImGuiID id() const { return id_; }

private:
    ImGuiDockNodeFlags flags_;
    ImGuiID id_ = 0;
};

class Text final : public Component {
public:
    explicit Text(std::string value) : value_(std::move(value)) {}
    void draw() override { ImGui::TextUnformatted(value_.c_str()); }
private:
    std::string value_;
};

class Separator final : public Component {
public:
    void draw() override { ImGui::Separator(); }
};

class Button final : public Component {
public:
    Button(std::string label, std::function<void()> onClick = {})
        : label_(std::move(label)), onClick_(std::move(onClick)) {}
    void draw() override { if (ImGui::Button(label_.c_str()) && onClick_) onClick_(); }
private:
    std::string label_;
    std::function<void()> onClick_;
};

class InputText final : public Component {
public:
    InputText(std::string label, std::string initial = {}) : label_(std::move(label)) {
        const auto count = initial.copy(value_.data(), value_.size() - 1);
        value_[count] = '\0';
    }
    void draw() override { ImGui::InputText(label_.c_str(), value_.data(), value_.size()); }
    const char* value() const { return value_.data(); }
private:
    std::string label_;
    std::array<char, 256> value_{};
};

class Checkbox final : public Component {
public:
    Checkbox(std::string label, bool checked = false) : label_(std::move(label)), checked_(checked) {}
    void draw() override { ImGui::Checkbox(label_.c_str(), &checked_); }
    bool checked() const { return checked_; }
private:
    std::string label_;
    bool checked_ = false;
};

class RadioButton final : public Component {
public:
    RadioButton(std::string label, int& value, int id) : label_(std::move(label)), value_(value), id_(id) {}
    void draw() override { if (ImGui::RadioButton(label_.c_str(), value_ == id_)) value_ = id_; }
private:
    std::string label_;
    int& value_;
    int id_ = 0;
};

class ComboBox final : public Component {
public:
    ComboBox(std::string label, std::vector<std::string> options, int* selected)
        : label_(std::move(label)), options_(std::move(options)), selected_(selected) {}

    ComboBox(std::string label, std::vector<std::string> options, std::vector<bool>* selected)
        : label_(std::move(label)), options_(std::move(options)), multiSelected_(selected) {}

    void draw() override {
        std::string preview;
        if (selected_) {
            if (*selected_ >= 0 && *selected_ < static_cast<int>(options_.size()))
                preview = options_[*selected_];
        } else if (multiSelected_) {
            for (int i = 0; i < static_cast<int>(options_.size()); ++i)
                if (i < static_cast<int>(multiSelected_->size()) && (*multiSelected_)[i]) {
                    if (!preview.empty()) preview += ", ";
                    preview += options_[i];
                }
            if (preview.empty()) preview = "(none)";
        }

        if (!ImGui::BeginCombo(label_.c_str(), preview.c_str()))
            return;
        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            if (selected_) {
                const bool isSelected = *selected_ == i;
                if (ImGui::Selectable(options_[i].c_str(), isSelected)) *selected_ = i;
                if (isSelected) ImGui::SetItemDefaultFocus();
            } else if (multiSelected_) {
                bool value = i < static_cast<int>(multiSelected_->size()) && (*multiSelected_)[i];
                if (ImGui::Checkbox(options_[i].c_str(), &value)) {
                    if (i >= static_cast<int>(multiSelected_->size())) multiSelected_->resize(i + 1);
                    (*multiSelected_)[i] = value;
                }
            }
        }
        ImGui::EndCombo();
    }

private:
    std::string label_;
    std::vector<std::string> options_;
    int* selected_ = nullptr;
    std::vector<bool>* multiSelected_ = nullptr;
};

class ListView final : public Component {
public:
    ListView(std::string label, std::vector<std::string> items, int* selected = nullptr)
        : label_(std::move(label)), items_(std::move(items)), selected_(selected) {}
    void draw() override {
        ImGui::TextUnformatted(label_.c_str());
        if (ImGui::BeginListBox(("##" + label_).c_str())) {
            for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
                const bool selected = selected_ && *selected_ == i;
                if (ImGui::Selectable(items_[i].c_str(), selected) && selected_) *selected_ = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndListBox();
        }
    }
private:
    std::string label_;
    std::vector<std::string> items_;
    int* selected_ = nullptr;
};

class TreeView final : public Component {
public:
    struct Node { std::string label; std::vector<Node> children; };
    explicit TreeView(std::vector<Node> roots) : roots_(std::move(roots)) {}
    void draw() override { for (const auto& node : roots_) drawNode(node); }
private:
    static void drawNode(const Node& node) {
        if (node.children.empty()) { ImGui::BulletText("%s", node.label.c_str()); return; }
        if (ImGui::TreeNode(node.label.c_str())) {
            for (const auto& child : node.children) drawNode(child);
            ImGui::TreePop();
        }
    }
    std::vector<Node> roots_;
};

class RenderViewport final : public Component {
public:
    using DrawCallback = std::function<void(ImVec2)>;
    RenderViewport(std::string label, DrawCallback callback) : label_(std::move(label)), callback_(std::move(callback)) {}
    void draw() override {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        ImGui::BeginChild(label_.c_str(), ImVec2(0.0f, 220.0f), true, ImGuiWindowFlags_NoScrollbar);
        if (callback_) callback_(size);
        ImGui::EndChild();
    }
private:
    std::string label_;
    DrawCallback callback_;
};

class Window final : public Component {
public:
    explicit Window(std::string title) : title_(std::move(title)) {}

    Window& setInitialDock(ImGuiID dockId) {
        initialDockId_ = dockId;
        return *this;
    }

    Window& setOpen(bool* open) {
        open_ = open;
        return *this;
    }

    template <typename T, typename... Args> T& add(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& result = *component;
        children_.push_back(std::move(component));
        return result;
    }
    void draw() override {
        if (initialDockId_ != 0)
            ImGui::SetNextWindowDockID(initialDockId_, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title_.c_str(), open_)) { ImGui::End(); return; }
        for (const auto& child : children_) child->draw();
        ImGui::End();
    }
private:
    std::string title_;
    std::vector<std::unique_ptr<Component>> children_;
    ImGuiID initialDockId_ = 0;
    bool* open_ = nullptr;
};

} // namespace ui
