#pragma once

#include "imgui.h"
#include "imgui_internal.h" // DockBuilder currently lives in Dear ImGui's docking internals.
#include "log.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
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

// Builds a persistent default docking layout the first time an application is
// started. Existing user layouts are left untouched by the caller.
class DefaultDockLayout {
public:
    static void buildHLayout(ImGuiID rootId, const char* leftWindow, const char* rightWindow,
                             const char* upperCenterWindow, const char* lowerCenterWindow) {
        ImGui::DockBuilderRemoveNode(rootId);
        ImGui::DockBuilderAddNode(rootId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(rootId, ImGui::GetMainViewport()->Size);

        ImGuiID centerId = rootId;
        ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.22f, nullptr, &centerId);
        ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.25f, nullptr, &centerId);
        ImGuiID lowerId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.30f, nullptr, &centerId);

        ImGui::DockBuilderDockWindow(leftWindow, leftId);
        ImGui::DockBuilderDockWindow(rightWindow, rightId);
        ImGui::DockBuilderDockWindow(upperCenterWindow, centerId);
        ImGui::DockBuilderDockWindow(lowerCenterWindow, lowerId);
        ImGui::DockBuilderFinish(rootId);
    }
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

// A read-only, scrollable rich text view. All entries share one child window,
// while each entry can still provide its own text color and multiline content.
class LogView final : public Component {
public:
    explicit LogView(logsys::Logger& logger, float height = 0.0f)
        : logger_(logger), height_(height) {}

    void draw() override {
        const auto channels = logger_.channels();
        for (const auto& channel : channels)
            visibleChannels_.try_emplace(channel, true);

        drawFilter(channels);
        ImGui::Separator();

        const float viewHeight = height_ > 0.0f ? height_ : ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##log-editor", ImVec2(0.0f, viewHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        const auto entries = logger_.entries();
        visibleText_.clear();
        styledLines_.clear();
        for (const auto& entry : entries) {
            if (!visibleChannels_[entry.channel])
                continue;

            const std::string line = "[" + entry.channel + "] " + entry.message;
            appendStyledText(line, colorFor(entry.level));
        }
        drawSelectableText();
        if (atBottom)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

private:
    struct StyledLine {
        std::string text;
        ImVec4 color;
    };

    void appendStyledText(const std::string& text, const ImVec4& color) {
        std::size_t lineStart = 0;
        while (lineStart <= text.size()) {
            const std::size_t lineEnd = text.find('\n', lineStart);
            styledLines_.push_back({text.substr(lineStart, lineEnd - lineStart), color});
            if (!visibleText_.empty())
                visibleText_ += '\n';
            visibleText_ += styledLines_.back().text;
            if (lineEnd == std::string::npos)
                break;
            lineStart = lineEnd + 1;
        }
    }

    void drawSelectableText() {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float lineHeight = ImGui::GetTextLineHeight();
        float textWidth = 0.0f;
        for (const auto& line : styledLines_)
            textWidth = std::max(textWidth, ImGui::CalcTextSize(line.text.c_str()).x);

        const ImVec2 textSize(
            std::max(ImGui::GetContentRegionAvail().x, textWidth + style.FramePadding.x * 2.0f),
            std::max(lineHeight + style.FramePadding.y * 2.0f,
                     lineHeight * static_cast<float>(styledLines_.size()) + style.FramePadding.y * 2.0f));
        const ImVec2 widgetPosition = ImGui::GetCursorScreenPos();

        const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, transparent);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, transparent);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::InputTextMultiline("##selectable-log-text", visibleText_.data(), visibleText_.size() + 1,
                                  textSize, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 linePosition(widgetPosition.x + style.FramePadding.x,
                            widgetPosition.y + style.FramePadding.y);
        for (const auto& line : styledLines_) {
            drawList->AddText(linePosition, ImGui::ColorConvertFloat4ToU32(line.color),
                              line.text.data(), line.text.data() + line.text.size());
            linePosition.y += lineHeight;
        }
    }

    void drawFilter(const std::vector<std::string>& channels) {
        const auto visibleCount = static_cast<std::size_t>(std::count_if(
            channels.begin(), channels.end(),
            [this](const std::string& channel) { return visibleChannels_[channel]; }));
        const bool allVisible = !channels.empty() && visibleCount == channels.size();
        const bool someVisible = visibleCount > 0 && !allVisible;
        const std::string preview = allVisible
            ? "All"
            : std::to_string(visibleCount) + " of " + std::to_string(channels.size());

        ImGui::SetNextItemWidth(180.0f);
        if (!ImGui::BeginCombo("Filter", preview.c_str()))
            return;

        bool toggleAll = allVisible;
        if (someVisible)
            ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        if (ImGui::Checkbox("All", &toggleAll))
            for (const auto& channel : channels)
                visibleChannels_[channel] = toggleAll;
        if (someVisible)
            ImGui::PopItemFlag();

        ImGui::Separator();
        for (const auto& channel : channels)
            ImGui::Checkbox(channel.c_str(), &visibleChannels_[channel]);
        ImGui::EndCombo();
    }

    static ImVec4 colorFor(logsys::Level level) {
        switch (level) {
        case logsys::Level::Warning: return ImVec4(1.0f, 0.82f, 0.22f, 1.0f);
        case logsys::Level::Error: return ImVec4(1.0f, 0.32f, 0.32f, 1.0f);
        case logsys::Level::Info: return ImVec4(0.82f, 0.84f, 0.88f, 1.0f);
        }
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    logsys::Logger& logger_;
    float height_ = 0.0f;
    std::unordered_map<std::string, bool> visibleChannels_;
    std::string visibleText_;
    std::vector<StyledLine> styledLines_;
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
