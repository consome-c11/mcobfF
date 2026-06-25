#include "AppState.h"
#include "mcobfF/api/api.h"
#include "mcobfF/network/VersionDownloader.h"
#include "mcobfF/mapping/MappingData.h"
#include "mcobfF/file/FileSystem.h"
#include "mcobfF/dumper/JarDumper.h"
#include "config/Settings.h"

#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <future>
#include <functional>
#include <sstream>
#include <cmath>

AppState::AppState() {
    cacheDir_ = mcobfF::JarDumper::getDefaultCacheDir();
    mcobfF::JarDumper::setCacheDir(cacheDir_);
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        configPath_ = std::string(localAppData) + "\\mcobfF\\config.json";
    }
    mcobfF::Settings::instance().load(configPath_);
    startFetchManifest();
}

AppState::~AppState() {
    mcobfF::Settings::instance().save(configPath_);
}

void AppState::setHwnd(HWND hwnd) {
    hwnd_ = hwnd;
}

void AppState::update() {
    mcobfF::Settings::instance().applyThemeIfNeeded();

    if (mcobfF::Settings::instance().isDirty()) {
        displayListDirty_ = true;
        mcobfF::Settings::instance().clearDirty();
    }

    if (manifestFuture_.valid()) {
        if (manifestFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            manifestFuture_.get();
            manifestFetching_ = false;
        }
    }
    if (loadFuture_.valid()) {
        if (loadFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            bool success = loadFuture_.get();
            loading_ = false;
            if (success) {
                loaded_ = true;
                loaded_ = api_->isMappingLoaded();
                if (loaded_) buildTree();
                else loadError_ = "API reports no mappings loaded";
            } else {
                loadError_ = loadError_.empty() ? "Failed to load version" : loadError_;
            }
        }
    }
    if (dumpFuture_.valid()) {
        if (dumpFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            bool success = dumpFuture_.get();
            dumping_ = false;
            if (success) {
                dumpSuccess_ = true;
            } else if (dumpError_.empty()) {
                dumpError_ = "Dump failed";
            }
        }
    }

    // Update animations
    float deltaTime = ImGui::GetIO().DeltaTime;
    updateAnimations(deltaTime);
}



float AppState::easeOutCubic(float t) {
    // Smooth easing function: starts fast, slows down at the end
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

float AppState::easeInOutCubic(float t) {
    // Smooth easing for both directions
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float f = 1.0f - t;
        return 1.0f - 4.0f * f * f * f;
    }
}

void AppState::updateAnimations(float deltaTime) {
    const float animDuration = mcobfF::Settings::instance().get<float>(mcobfF::SettingKey::AnimationDuration);
    const float animSpeed = 1.0f / animDuration;
    
    // Helper lambda for smooth linear time-based animation
    // Easing is applied during rendering, not during time progression
    auto animateValue = [deltaTime, animSpeed](float& value, float target) {
        if (std::abs(value - target) < 0.001f) {
            value = target;
            return;
        }
        float step = deltaTime * animSpeed;
        if (target > value) {
            value += step;
            if (value > target) value = target;
        } else {
            value -= step;
            if (value < target) value = target;
        }
    };

    // Check if selection changed
    bool selectionChanged = (selection_.type != lastSelectionType_) || 
                            (selection_.entryIndex != lastEntryIndex_);
    
    if (selectionChanged) {
        // Reset animations when selection changes
        if (selection_.type == Selection::None) {
            classInfoAnim_ = 0.0f;
            methodsAnim_ = 0.0f;
            fieldsAnim_ = 0.0f;
            methodInfoAnim_ = 0.0f;
            fieldInfoAnim_ = 0.0f;
        } else if (selection_.type == Selection::Class) {
            classInfoAnim_ = 0.0f;
            methodsAnim_ = 0.0f;
            fieldsAnim_ = 0.0f;
            methodInfoAnim_ = 0.0f;
            fieldInfoAnim_ = 0.0f;
        } else if (selection_.type == Selection::Method) {
            methodInfoAnim_ = 0.0f;
        } else if (selection_.type == Selection::Field) {
            fieldInfoAnim_ = 0.0f;
        }
        lastSelectionType_ = selection_.type;
        lastEntryIndex_ = selection_.entryIndex;
    }

    // Animate based on current selection
    if (selection_.type == Selection::Class) {
        animateValue(classInfoAnim_, 1.0f);
        animateValue(methodsAnim_, 1.0f);
        animateValue(fieldsAnim_, 1.0f);
    } else if (selection_.type == Selection::Method) {
        animateValue(methodInfoAnim_, 1.0f);
    } else if (selection_.type == Selection::Field) {
        animateValue(fieldInfoAnim_, 1.0f);
    }

    // Animate panels when loaded
    if (loaded_) {
        animateValue(leftPanelAnim_, 1.0f);
        animateValue(rightPanelAnim_, 1.0f);
    } else {
        leftPanelAnim_ = 0.0f;
        rightPanelAnim_ = 0.0f;
    }

    // Update tree node animations
    for (auto& pair : nodeAnimStates_) {
        const std::string& path = pair.first;
        float& current = pair.second;
        float target = 0.0f;
        
        auto targetIt = nodeAnimTargets_.find(path);
        if (targetIt != nodeAnimTargets_.end()) {
            target = targetIt->second;
        }
        
        animateValue(current, target);
    }
    
    // Remove completed animations (value reached target and is 0.0)
    std::vector<std::string> toRemove;
    for (const auto& pair : nodeAnimStates_) {
        if (pair.second < 0.001f) {
            auto targetIt = nodeAnimTargets_.find(pair.first);
            if (targetIt != nodeAnimTargets_.end() && targetIt->second < 0.001f) {
                toRemove.push_back(pair.first);
            }
        }
    }
    if (!toRemove.empty()) {
        for (const auto& path : toRemove) {
            nodeAnimStates_.erase(path);
            nodeAnimTargets_.erase(path);
            closingDescendantCount_.erase(path);
        }
        displayListDirty_ = true; // Rebuild display list when animation completes
    }
}

void AppState::startFetchManifest() {
    manifestFetching_ = true;
    manifestLoaded_ = false;
    manifestError_.clear();
    manifestFuture_ = std::async(std::launch::async, [this]() {
        auto manifest = mcobfF::VersionDownloader::fetchManifest();
        if (!manifest) {
            manifestError_ = "Failed to fetch version manifest";
            return;
        }
        auto& versions = (*manifest)["versions"];
        for (auto& v : versions) {
            VersionEntry ve;
            ve.id = v["id"];
            ve.type = v["type"];
            ve.releaseTime = v["releaseTime"];
            allVersions_.push_back(std::move(ve));
        }
        std::sort(allVersions_.begin(), allVersions_.end(),
            [](const VersionEntry& a, const VersionEntry& b) {
                return a.releaseTime > b.releaseTime;
            });
        manifestLoaded_ = true;
    });
}

void AppState::updateDisplayedVersions() {
    displayedVersions_.clear();
    std::string filter = versionFilter_;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
    for (const auto& ve : allVersions_) {
        if (ve.type != "release" && ve.type != "snapshot") continue;
        if (!filter.empty()) {
            std::string id = ve.id;
            std::transform(id.begin(), id.end(), id.begin(), ::tolower);
            if (id.find(filter) == std::string::npos) continue;
        }
        displayedVersions_.push_back(&ve);
    }
}

void AppState::startLoadVersion(const std::string& version) {
    selectedVersion_ = version;
    loading_ = true;
    loaded_ = false;
    loadError_.clear();
    treeRoot_.children.clear();
    selection_ = {};
    displayListDirty_ = true;

    loadFuture_ = std::async(std::launch::async, [this, version]() -> bool {
        try {
            auto jarPath = cacheDir_ + "\\" + version + "\\client.jar";
            (void)mcobfF::FileSystem::createDirectories(cacheDir_ + "\\" + version);
            if (!mcobfF::VersionDownloader::downloadClientJar(version, jarPath)) {
                loadError_ = "Failed to download client JAR for " + version;
                return false;
            }
            api_ = std::make_unique<mcobfF::api>(cacheDir_);
            if (!api_->loadMappingsWithInheritance(version, jarPath)) {
                loadError_ = "Failed to load mappings for " + version;
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            loadError_ = e.what();
            return false;
        }
    });
}

void AppState::buildTree() {
    treeRoot_.children.clear();
    treeRoot_.type = TreeNode::Root;
    displayListDirty_ = true;

    const auto& data = api_->getMappingData();

    for (int i = 0; i < (int)data.entries.size(); i++) {
        const auto& entry = data.entries[i];
        std::string deobf = entry.classInfo.deobfClass;

        std::vector<std::string> parts;
        size_t start = 0, end;
        while ((end = deobf.find('/', start)) != std::string::npos) {
            parts.push_back(deobf.substr(start, end - start));
            start = end + 1;
        }
        parts.push_back(deobf.substr(start));

        std::string intermediaryShort;
        if (entry.classInfo.intermediaryClass.has_value()) {
            const std::string& interClass = entry.classInfo.intermediaryClass.value();
            if (!interClass.empty()) {
                size_t lastSlash = interClass.rfind('/');
                if (lastSlash != std::string::npos) {
                    intermediaryShort = interClass.substr(lastSlash + 1);
                } else {
                    intermediaryShort = interClass;
                }
            }
        }

        TreeNode* current = &treeRoot_;
        std::string path;
        for (size_t j = 0; j < parts.size(); j++) {
            if (!path.empty()) path += "/";
            path += parts[j];

            auto it = std::find_if(current->children.begin(), current->children.end(),
                [&](const TreeNode& n) { return n.name == parts[j]; });

            if (it != current->children.end()) {
                current = &*it;
            } else {
                TreeNode newNode;
                newNode.name = parts[j];
                newNode.displayPath = path;
                newNode.entryIndex = (j == parts.size() - 1) ? i : -1;
                newNode.type = (j == parts.size() - 1) ? TreeNode::ClassEntry : TreeNode::Directory;
                if (j == parts.size() - 1 && !intermediaryShort.empty()) {
                    newNode.intermediaryName = intermediaryShort;
                }
                if (j == parts.size() - 1 && entry.classInfo.srgClass.has_value()) {
                    const std::string& srg = entry.classInfo.srgClass.value();
                    if (!srg.empty()) {
                        size_t lastSlash = srg.rfind('/');
                        newNode.srgName = (lastSlash != std::string::npos) ? srg.substr(lastSlash + 1) : srg;
                    }
                }
                current->children.push_back(std::move(newNode));
                current = &current->children.back();
            }
        }

        for (size_t m = 0; m < entry.methods.size(); m++) {
            TreeNode mn;
            mn.name = entry.methods[m].deobfName + "(...)";
            mn.displayPath = current->displayPath + "/m" + std::to_string(m);
            mn.type = TreeNode::Method;
            mn.entryIndex = i;
            mn.memberIndex = (int)m;
            if (entry.methods[m].intermediaryName.has_value()) {
                const std::string& interName = entry.methods[m].intermediaryName.value();
                if (!interName.empty()) {
                    mn.intermediaryName = interName;
                }
            }
            if (entry.methods[m].srgName.has_value()) {
                const std::string& srg = entry.methods[m].srgName.value();
                if (!srg.empty()) {
                    mn.srgName = srg;
                }
            }
            current->children.push_back(std::move(mn));
        }

        for (size_t f = 0; f < entry.fields.size(); f++) {
            TreeNode fn;
            fn.name = entry.fields[f].deobfName;
            fn.displayPath = current->displayPath + "/f" + std::to_string(f);
            fn.type = TreeNode::Field;
            fn.entryIndex = i;
            fn.memberIndex = (int)f;
            if (entry.fields[f].intermediaryName.has_value()) {
                const std::string& interName = entry.fields[f].intermediaryName.value();
                if (!interName.empty()) {
                    fn.intermediaryName = interName;
                }
            }
            if (entry.fields[f].srgName.has_value()) {
                const std::string& srg = entry.fields[f].srgName.value();
                if (!srg.empty()) {
                    fn.srgName = srg;
                }
            }
            current->children.push_back(std::move(fn));
        }
    }

    std::function<void(TreeNode&)> sortTree = [&](TreeNode& node) {
        std::stable_sort(node.children.begin(), node.children.end(),
            [](const TreeNode& a, const TreeNode& b) {
                auto order = [](TreeNode::Type t) -> int {
                    switch (t) {
                        case TreeNode::Directory: return 0;
                        case TreeNode::ClassEntry: return 1;
                        case TreeNode::Field: return 0;
                        case TreeNode::Method: return 1;
                        default: return 2;
                    }
                };
                return order(a.type) < order(b.type);
            });
        for (auto& child : node.children) {
            sortTree(child);
        }
    };
    sortTree(treeRoot_);
}

void AppState::renderGui() {
    update();
    renderTitleBar();

    if (loading_) {
        ImGui::OpenPopup("Loading");
    }

    if (ImGui::BeginPopupModal("Loading", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!loading_) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("Loading %s...", selectedVersion_.c_str());
            ImGui::Separator();
            ImGui::Text("Downloading and parsing mappings. Please wait...");
        }
        ImGui::EndPopup();
    }

    if (dumping_) {
        ImGui::OpenPopup("Dumping");
    }

    if (ImGui::BeginPopupModal("Dumping", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!dumping_) {
            ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("Dumping mappings for %s...", selectedVersion_.c_str());
            ImGui::Separator();
            ImGui::Text("Writing to %s", dumpOutputPath_.c_str());
        }
        ImGui::EndPopup();
    }

    if (!loadError_.empty()) {
        ImGui::OpenPopup("Error");
    }
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Error: %s", loadError_.c_str());
        if (ImGui::Button("OK")) {
            loadError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (dumpSuccess_) {
        ImGui::OpenPopup("Dump Complete");
    }
    if (ImGui::BeginPopupModal("Dump Complete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Successfully dumped mappings for %s", selectedVersion_.c_str());
        ImGui::Text("Output: %s", dumpOutputPath_.c_str());
        if (ImGui::Button("OK")) {
            dumpSuccess_ = false;
            dumpOutputPath_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!dumpError_.empty()) {
        ImGui::OpenPopup("Dump Error");
    }
    if (ImGui::BeginPopupModal("Dump Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Dump failed:");
        ImGui::TextWrapped("%s", dumpError_.c_str());
        if (ImGui::Button("OK")) {
            dumpError_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (versionSelectorOpen_) {
        renderVersionSelector();
        return;
    }
    const float titleBarHeight = 36.0f;
    ImGui::SetNextWindowPos(ImVec2(0, titleBarHeight));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y - titleBarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("MainContent", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
    if (ImGui::Button("File")) {
        ImGui::OpenPopup("##FileMenuBar");
    }
    ImGui::SameLine();
    if (ImGui::Button("Settings")) {
        ImGui::OpenPopup("##SettingsMenuBar");
    }
    ImGui::PopStyleColor(3);
    if (ImGui::BeginPopup("##FileMenuBar")) {
        if (ImGui::MenuItem("Select Version...", nullptr, false, !loading_)) {
            versionSelectorOpen_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Dump Mappings...", nullptr, false, loaded_ && !dumping_)) {
            dumpRequested();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Cache")) {
            clearCache();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            PostQuitMessage(0);
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##SettingsMenuBar")) {
        if (ImGui::MenuItem("Open Settings...")) {
            settingsOpen_ = true;
        }
        ImGui::EndPopup();
    }
    ImGui::Separator();

    if (settingsOpen_) {
        renderSettingsWindow();
    } else if (!loaded_) {
        float winW = ImGui::GetContentRegionAvail().x;
        float winH = ImGui::GetContentRegionAvail().y;
        ImGui::SetCursorPosX((winW - 200) * 0.5f);
        ImGui::SetCursorPosY((winH - 40) * 0.5f);
        if (ImGui::Button("Select Version", ImVec2(200, 40))) {
            versionSelectorOpen_ = true;
        }
    } else {
        if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("LeftPanel", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("RightPanel", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            renderLeftPanel();

            ImGui::TableSetColumnIndex(1);
            renderRightPanel();

            ImGui::EndTable();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void AppState::renderTitleBar() {
    const float titleBarHeight = 36.0f;
    const float btnW = 46.0f;
    const ImU32 col = IM_COL32(235, 235, 235, 255);
    const ImU32 colHover = IM_COL32(100, 125, 160, 255);

    const bool isLightTheme = mcobfF::Settings::instance().getEffectiveTheme() == mcobfF::Theme::Light;
    const ImVec4 titleBarBg = isLightTheme ? ImVec4(0.82f, 0.83f, 0.86f, 1.0f) : ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    const ImVec4 titleBarText = isLightTheme ? ImVec4(0.10f, 0.10f, 0.10f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, titleBarBg);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, titleBarHeight));

    ImGui::Begin("##TitleBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

    float w = ImGui::GetWindowWidth();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::SetCursorPos(ImVec2(w - btnW, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.05f, 0.05f, 1.0f));
    if (ImGui::Button("##Close", ImVec2(btnW, titleBarHeight))) {
        PostQuitMessage(0);
    }
    ImGui::PopStyleColor(3);

    ImVec2 rectMin = ImGui::GetItemRectMin();
    ImVec2 rectMax = ImGui::GetItemRectMax();
    ImVec2 center = ImVec2((rectMin.x + rectMax.x) * 0.5f, (rectMin.y + rectMax.y) * 0.5f);

    float crossLen = ImGui::GetFontSize() * 0.4f;
    float thickness = 1.5f;
    ImU32 crossColor = ImGui::IsItemHovered() ? colHover : col;

    dl->AddLine(ImVec2(center.x - crossLen, center.y - crossLen),
                ImVec2(center.x + crossLen, center.y + crossLen), crossColor, thickness);
    dl->AddLine(ImVec2(center.x + crossLen, center.y - crossLen),
                ImVec2(center.x - crossLen, center.y + crossLen), crossColor, thickness);

    ImGui::SetCursorPos(ImVec2(w - btnW * 2, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
    if (ImGui::Button("##Max", ImVec2(btnW, titleBarHeight))) {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd_, &wp);
        if (wp.showCmd == SW_MAXIMIZE)
            ShowWindow(hwnd_, SW_RESTORE);
        else
            ShowWindow(hwnd_, SW_MAXIMIZE);
    }
    ImGui::PopStyleColor(3);
    {
        float bx = w - btnW * 2;
        ImVec2 bc(bx + btnW * 0.5f, titleBarHeight * 0.5f);
        float s = 5.0f;
        float t = 1.5f;
        ImU32 fc = ImGui::IsItemHovered() ? colHover : col;
        dl->AddRect(ImVec2(bc.x - s - 1, bc.y - s), ImVec2(bc.x + s - 1, bc.y + s), fc, 0.0f, 0, t);
    }

    ImGui::SetCursorPos(ImVec2(w - btnW * 3, 0));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
    if (ImGui::Button("##Min", ImVec2(btnW, titleBarHeight))) {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
    ImGui::PopStyleColor(3);
    {
        float bx = w - btnW * 3;
        ImVec2 bc(bx + btnW * 0.5f, titleBarHeight * 0.5f);
        float lineLen = 10.0f;
        ImU32 fc = ImGui::IsItemHovered() ? colHover : col;
        dl->AddLine(ImVec2(bc.x - lineLen * 0.5f, bc.y), ImVec2(bc.x + lineLen * 0.5f, bc.y), fc, 1.5f);
    }

    ImGui::SetCursorPos(ImVec2(8, 0));
    ImGui::SetCursorPosY((titleBarHeight - ImGui::GetFontSize()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, titleBarText);
    ImGui::Text("MC-OBF-Find");

    if (loaded_) {
        ImGui::SameLine();
        ImGui::SetCursorPosY((titleBarHeight - ImGui::GetFontSize()) * 0.5f);
        ImGui::TextDisabled("  |  %s", selectedVersion_.c_str());
    }
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void AppState::renderVersionSelector() {
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Select Version", &versionSelectorOpen_)) {
        ImGui::End();
        return;
    }

    if (manifestFetching_) {
        ImGui::Text("Fetching version manifest...");
        ImGui::End();
        return;
    }

    if (!manifestError_.empty()) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", manifestError_.c_str());
        if (ImGui::Button("Retry")) {
            manifestError_.clear();
            startFetchManifest();
        }
        ImGui::End();
        return;
    }

    if (!manifestLoaded_) {
        ImGui::Text("Waiting for manifest...");
        ImGui::End();
        return;
    }

    ImGui::InputText("Filter", versionFilter_, sizeof(versionFilter_));
    updateDisplayedVersions();

    ImGui::Separator();
    ImGui::BeginChild("VersionList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 10));

    for (const auto* ve : displayedVersions_) {
        bool isRelease = ve->type == "release";
        std::string label = ve->id;
        if (isRelease) {
            label += " [Release]";
        } else {
            label += " [Snapshot]";
        }

        ImVec4 color = isRelease ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (ImGui::Selectable(label.c_str(), ve->id == selectedVersion_)) {
            startLoadVersion(ve->id);
            versionSelectorOpen_ = false;
        }
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::End();
}

void AppState::renderSettingsWindow() {
    ImGui::Text("Settings");
    ImGui::Separator();
    ImGui::Spacing();

    mcobfF::Settings::instance().renderAll();

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Reset to Defaults")) {
        mcobfF::Settings::instance().resetToDefaults();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back")) {
        mcobfF::Settings::instance().save(configPath_);
        settingsOpen_ = false;
    }
}

void AppState::clearCache() {
    std::error_code ec;
    std::filesystem::remove_all(cacheDir_, ec);
    cacheDir_ = mcobfF::JarDumper::getDefaultCacheDir();
    mcobfF::JarDumper::setCacheDir(cacheDir_);
    loaded_ = false;
    treeRoot_.children.clear();
    selection_ = {};
    displayListDirty_ = true;
    startFetchManifest();
}

void AppState::dumpRequested() {
    wchar_t filename[MAX_PATH] = {};
    std::wstring defaultName = std::wstring(selectedVersion_.begin(), selectedVersion_.end()) + L"_mappings.json";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrDefExt = L"json";
    ofn.lpstrFileTitle = defaultName.data();
    ofn.nMaxFileTitle = (DWORD)defaultName.size() + 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    if (GetSaveFileNameW(&ofn)) {
        dumpOutputPath_ = std::filesystem::path(filename).string();
        dumping_ = true;
        dumpError_.clear();
        dumpFuture_ = std::async(std::launch::async, [this]() -> bool {
            try {
                return mcobfF::JarDumper::dumpFromVersion(selectedVersion_, dumpOutputPath_);
            } catch (const std::exception& e) {
                dumpError_ = e.what();
                return false;
            }
        });
    }
}

void AppState::renderLeftPanel() {
    ImGui::BeginChild("LeftPanel", ImVec2(0, 0), false);

    float alpha = easeOutCubic(leftPanelAnim_);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    ImGui::InputText("Search", classFilter_, sizeof(classFilter_));

    if (strcmp(classFilter_, lastFilter_) != 0) {
        memcpy(lastFilter_, classFilter_, sizeof(lastFilter_));
        displayListDirty_ = true;
    }

    ImGui::Separator();

    std::string rawFilter = classFilter_;
    std::transform(rawFilter.begin(), rawFilter.end(), rawFilter.begin(), ::tolower);

    std::string classFilter, memberFilter;
    bool hasPipe = false;
    auto pipePos = rawFilter.find('|');
    if (pipePos != std::string::npos) {
        classFilter = rawFilter.substr(0, pipePos);
        memberFilter = rawFilter.substr(pipePos + 1);
        hasPipe = true;
    } else {
        classFilter = rawFilter;
        memberFilter = rawFilter;
    }
    std::replace(classFilter.begin(), classFilter.end(), '.', '/');
    std::replace(memberFilter.begin(), memberFilter.end(), '.', '/');
    bool filtering = !rawFilter.empty();

    if (filtering && !wasFiltering_) {
        savedExpandedNodes_ = expandedNodes_;
    }
    if (!filtering && wasFiltering_) {
        expandedNodes_ = savedExpandedNodes_;
        displayListDirty_ = true;
    }
    wasFiltering_ = filtering;
    hasPipe_ = hasPipe;

    auto matchesFilter = [](const std::string& str, const std::string& filter) -> bool {
        if (filter.empty()) return true;
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.find(filter) != std::string::npos;
    };

    if (displayListDirty_) {
        cachedDisplayList_.clear();
        // Don't clear parentBottomY_ here - it will be updated when nodes are rendered

        auto filterPass = [&](const TreeNode& node) -> bool {
            if (!filtering) return true;
            if (node.type == TreeNode::ClassEntry || node.type == TreeNode::Directory) {
                return treeNodeMatchesFilter(node, classFilter, memberFilter, hasPipe_);
            }
            if (hasPipe_) {
                return matchesFilter(node.name, memberFilter)
                    || matchesFilter(node.intermediaryName, memberFilter)
                    || matchesFilter(node.srgName, memberFilter);
            }
            return matchesFilter(node.name, rawFilter)
                || matchesFilter(node.intermediaryName, rawFilter)
                || matchesFilter(node.srgName, rawFilter);
        };

        std::function<void(const TreeNode&, int)> flattenFiltered;
        flattenFiltered = [&](const TreeNode& node, int depth) {
            if (!filterPass(node)) return;
            FlatNode fn;
            fn.node = &node;
            fn.depth = depth;
            fn.type = node.type;
            fn.displayName = buildDisplayName(node);
            std::string path = node.displayPath.empty() ? node.name : node.displayPath;
            bool isExpanded = expandedNodes_.count(path) > 0;
            bool isAnimating = nodeAnimStates_.count(path) > 0;
            fn.isOpen = filtering || isExpanded || isAnimating;
            cachedDisplayList_.push_back(fn);
            if (fn.isOpen && (node.type == TreeNode::Directory || node.type == TreeNode::ClassEntry)) {
                for (const auto& child : node.children) {
                    flattenFiltered(child, depth + 1);
                }
            }
        };
        for (const auto& child : treeRoot_.children) {
            flattenFiltered(child, 0);
        }
        displayListDirty_ = false;
    }

    std::set<std::string> expandedBefore = expandedNodes_;

    const float baseX = ImGui::GetWindowContentRegionMin().x;
    const float indentWidth = ImGui::GetTreeNodeToLabelSpacing();

    // Collect closing node infos before the clipper loop
    // So we know which nodes are below closing nodes and can apply collapse offset
    struct ClosingNodeInfo {
        int displayIndex;
        int depth;
        int descendantCount;
        float rawProgress;
        std::string path;
    };
    std::vector<ClosingNodeInfo> closingNodeInfos;

    bool hasClosingAnimation = false;
    bool hasAnyAnimation = !nodeAnimStates_.empty();
    for (auto& [closePath, rawProgress] : nodeAnimStates_) {
        auto targetIt = nodeAnimTargets_.find(closePath);
        if (targetIt != nodeAnimTargets_.end() && targetIt->second < 0.5f) {
            // This is a closing animation
            hasClosingAnimation = true;
            auto countIt = closingDescendantCount_.find(closePath);
            if (countIt != closingDescendantCount_.end() && countIt->second > 0) {
                for (int j = 0; j < (int)cachedDisplayList_.size(); j++) {
                    const std::string& nodePath = cachedDisplayList_[j].node->displayPath;
                    if (nodePath == closePath) {
                        closingNodeInfos.push_back({
                            j,
                            cachedDisplayList_[j].depth,
                            countIt->second,
                            rawProgress,
                            closePath
                        });
                        break;
                    }
                }
            }
        }
    }

    // Lambda to render a range of nodes
    auto renderNodeRange = [&](int startIdx, int endIdx) {
        for (int i = startIdx; i < endIdx; i++) {
            const FlatNode& fn = cachedDisplayList_[i];

            float targetX = baseX + fn.depth * indentWidth;
            ImGui::SetCursorPosX(targetX);
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            float cursorYBefore = ImGui::GetCursorPosY();

            // Check if this node is a child of an animating node
            float parentAlpha = 1.0f;
            float yOffset = 0.0f;
            bool needsClip = false;
            float clipTopY = 0.0f;
            bool pushedAlpha = false;
            bool pushedClip = false;

            if (fn.depth > 0) {
                std::string path = fn.node->displayPath.empty() ? fn.node->name : fn.node->displayPath;

                // Check all ancestor paths for animations
                std::string checkPath = path;
                while (true) {
                    size_t lastSlash = checkPath.rfind('/');
                    if (lastSlash == std::string::npos) break;
                    std::string ancestorPath = checkPath.substr(0, lastSlash);

                    auto animIt = nodeAnimStates_.find(ancestorPath);
                    if (animIt != nodeAnimStates_.end()) {
                        float rawProgress = animIt->second;
                        auto targetIt = nodeAnimTargets_.find(ancestorPath);
                        bool isClosing = (targetIt != nodeAnimTargets_.end() && targetIt->second < 0.5f);

                        if (isClosing) {
                            // Closing: upward slide + fixed clip at parent bottom
                            float progress = easeInOutCubic(rawProgress); // 1->0
                            
                            auto countIt = closingDescendantCount_.find(ancestorPath);
                            auto parentYIt = parentBottomY_.find(ancestorPath);
                            if (countIt != closingDescendantCount_.end() && parentYIt != parentBottomY_.end()) {
                                float localLineHeight = ImGui::GetTextLineHeightWithSpacing();
                                float totalHeight = (countIt->second + 1) * localLineHeight;
                                // Slide up by total height so all children hide under parent
                                yOffset += (1.0f - progress) * -totalHeight;
                                
                                // Fixed clip at parent bottom
                                float thisClipTop = parentYIt->second;
                                if (!needsClip || thisClipTop > clipTopY) {
                                    clipTopY = thisClipTop;
                                    needsClip = true;
                                }
                            }
                        } else {
                            // Opening: fade in + slide down
                            float progress = easeInOutCubic(rawProgress); // 0->1
                            parentAlpha *= progress;
                            yOffset += (1.0f - progress) * -15.0f;

                            auto parentYIt = parentBottomY_.find(ancestorPath);
                            if (parentYIt != parentBottomY_.end()) {
                                float thisClipTop = parentYIt->second;
                                if (!needsClip || thisClipTop > clipTopY) {
                                    clipTopY = thisClipTop;
                                    needsClip = true;
                                }
                            }
                        }
                    }

                    checkPath = ancestorPath;
                }
            }

            // Skip rendering if completely faded out (only during opening)
            if (parentAlpha < 0.01f) {
                ImGui::SetCursorPosY(cursorYBefore + lineHeight);
                continue;
            }

            // Apply collapse offset: nodes BELOW a closing node (not descendants)
            // slide up smoothly as the closing node's children disappear
            float collapseOffset = 0.0f;
            for (const auto& cni : closingNodeInfos) {
                if (i > cni.displayIndex) {
                    // Check if this node is NOT a descendant of the closing node
                    std::string myPath = fn.node->displayPath.empty() ? fn.node->name : fn.node->displayPath;
                    bool isDescendant = (myPath.find(cni.path + "/") == 0);
                    if (!isDescendant) {
                        float progress = easeInOutCubic(cni.rawProgress);
                        float collapseHeight = cni.descendantCount * lineHeight;
                        collapseOffset += (1.0f - progress) * -collapseHeight;
                    }
                }
            }
            yOffset += collapseOffset;

            // Apply alpha
            if (parentAlpha < 0.999f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, parentAlpha);
                pushedAlpha = true;
            }

            // Apply clip rect for opening animation
            if (needsClip) {
                ImVec2 windowPos = ImGui::GetWindowPos();
                float contentRegionMinY = ImGui::GetWindowContentRegionMin().y;
                ImGui::PushClipRect(
                    ImVec2(windowPos.x, windowPos.y + contentRegionMinY + clipTopY),
                    ImVec2(windowPos.x + ImGui::GetWindowSize().x, windowPos.y + ImGui::GetWindowHeight()),
                    true
                );
                pushedClip = true;
            }

            if (std::abs(yOffset) > 0.01f) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
            }

            switch (fn.type) {
            case TreeNode::Directory: {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.65f, 0.85f, 1.0f));

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
                    if (fn.node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

                    ImGui::SetNextItemOpen(fn.isOpen, ImGuiCond_Always);
                    bool open = ImGui::TreeNodeEx(fn.displayName.c_str(), flags);

                    std::string path = fn.node->displayPath;
                    parentBottomY_[path] = cursorYBefore + yOffset + lineHeight;

                    if (open != fn.isOpen) {
                        if (open) {
                            expandedNodes_.insert(path);
                            nodeAnimTargets_[path] = 1.0f;
                            nodeAnimStates_[path] = 0.0f;
                        } else {
                            expandedNodes_.erase(path);
                            nodeAnimTargets_[path] = 0.0f;
                            int count = 0;
                            for (int j = i + 1; j < (int)cachedDisplayList_.size(); j++) {
                                if (cachedDisplayList_[j].depth <= fn.depth) break;
                                count++;
                            }
                            closingDescendantCount_[path] = count;
                        }
                    }

                    if (open) ImGui::TreePop();
                    ImGui::PopStyleColor();
                    break;
                }
            case TreeNode::ClassEntry: {
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                    if (fn.node->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

                    if (fn.type == TreeNode::ClassEntry) {
                        if (selection_.type == Selection::Class && selection_.entryIndex == fn.node->entryIndex)
                            flags |= ImGuiTreeNodeFlags_Selected;
                    }

                    ImGui::SetNextItemOpen(fn.isOpen, ImGuiCond_Always);
                    bool open = ImGui::TreeNodeEx(fn.displayName.c_str(), flags);

                    std::string path = fn.node->displayPath;
                    parentBottomY_[path] = cursorYBefore + yOffset + lineHeight;

                    if (open != fn.isOpen) {
                        if (open) {
                            expandedNodes_.insert(path);
                            nodeAnimTargets_[path] = 1.0f;
                            nodeAnimStates_[path] = 0.0f;
                        } else {
                            expandedNodes_.erase(path);
                            nodeAnimTargets_[path] = 0.0f;
                            int count = 0;
                            for (int j = i + 1; j < (int)cachedDisplayList_.size(); j++) {
                                if (cachedDisplayList_[j].depth <= fn.depth) break;
                                count++;
                            }
                            closingDescendantCount_[path] = count;
                        }
                    }

                    if (fn.type == TreeNode::ClassEntry && ImGui::IsItemClicked()) {
                        selection_.type = Selection::Class;
                        selection_.entryIndex = fn.node->entryIndex;
                        selection_.memberIndex = -1;
                    }

                    if (open) ImGui::TreePop();
                    break;
            }
            case TreeNode::Method:
            case TreeNode::Field: {
                bool sel = false;
                if (fn.type == TreeNode::Method)
                    sel = (selection_.type == Selection::Method &&
                           selection_.entryIndex == fn.node->entryIndex &&
                           selection_.memberIndex == fn.node->memberIndex);
                else
                    sel = (selection_.type == Selection::Field &&
                           selection_.entryIndex == fn.node->entryIndex &&
                           selection_.memberIndex == fn.node->memberIndex);

                if (ImGui::Selectable(fn.displayName.c_str(), sel)) {
                    if (fn.type == TreeNode::Method) {
                        selection_.type = Selection::Method;
                    } else {
                        selection_.type = Selection::Field;
                    }
                    selection_.entryIndex = fn.node->entryIndex;
                    selection_.memberIndex = fn.node->memberIndex;
                }
                break;
            }
            default: break;
            }

            // Reset cursor to the expected position so that subsequent items
            // (unrelated to the animation) are NOT displaced upward.
            ImGui::SetCursorPosY(cursorYBefore + lineHeight);

            if (pushedClip) {
                ImGui::PopClipRect();
            }
            if (pushedAlpha) {
                ImGui::PopStyleVar();
            }
        }
    };

    // When any animation is active, render ALL nodes (skip clipper)
    // to ensure animating children are not culled by the list clipper
    if (hasAnyAnimation) {
        renderNodeRange(0, (int)cachedDisplayList_.size());
    } else {
        ImGuiListClipper clipper;
        clipper.Begin((int)cachedDisplayList_.size(), ImGui::GetTextLineHeightWithSpacing());
        while (clipper.Step()) {
            renderNodeRange(clipper.DisplayStart, clipper.DisplayEnd);
        }
    }

    if (expandedBefore != expandedNodes_) {
        displayListDirty_ = true;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void AppState::flattenTree(std::vector<FlatNode>& out, const TreeNode& node, int depth) {
    if (&node == &treeRoot_) {
        for (const auto& child : node.children) {
            flattenTree(out, child, 0);
        }
        return;
    }

    FlatNode fn;
    fn.node = &node;
    fn.depth = depth;
    fn.type = node.type;

    fn.displayName = buildDisplayName(node);

    std::string path = node.displayPath.empty() ? node.name : node.displayPath;
    fn.isOpen = expandedNodes_.count(path) > 0;

    out.push_back(fn);

    if (fn.isOpen && (node.type == TreeNode::Directory || node.type == TreeNode::ClassEntry)) {
        for (const auto& child : node.children) {
            flattenTree(out, child, depth + 1);
        }
    }
}

std::string AppState::buildDisplayName(const TreeNode& node) {
    const bool showIntermediary = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowIntermediaryNames);
    const bool showSrg = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowSrgNames);

    std::string name = node.name;
    if (node.type == TreeNode::ClassEntry) {
        if (showIntermediary && !node.intermediaryName.empty())
            name += " [" + node.intermediaryName + "]";
        if (showSrg && !node.srgName.empty())
            name += " [" + node.srgName + "]";
    } else if (node.type == TreeNode::Method) {
        std::string suffix;
        if (showIntermediary && !node.intermediaryName.empty()) suffix += " [" + node.intermediaryName + "]";
        if (showSrg && !node.srgName.empty()) suffix += " [" + node.srgName + "]";
        if (!suffix.empty()) {
            size_t pos = name.rfind("(...)");
            if (pos != std::string::npos)
                name = name.substr(0, pos) + suffix + name.substr(pos);
            else
                name += suffix;
        }
    } else if (node.type == TreeNode::Field) {
        if (showIntermediary && !node.intermediaryName.empty()) name += " [" + node.intermediaryName + "]";
        if (showSrg && !node.srgName.empty()) name += " [" + node.srgName + "]";
    }
    return name;
}

bool AppState::treeNodeMatchesFilter(const TreeNode& node, const std::string& classFilter, const std::string& memberFilter, bool hasPipe)
{
    auto matchesFilter = [](const std::string& str, const std::string& filter) -> bool {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.find(filter) != std::string::npos;
    };

    std::function<bool(const TreeNode&, const std::string&)> anyDescendantMatches;
    anyDescendantMatches = [&](const TreeNode& n, const std::string& filter) -> bool {
        if (filter.empty()) return true;
        for (const auto& child : n.children) {
            if (matchesFilter(child.name, filter)) return true;
            if (!child.intermediaryName.empty() && matchesFilter(child.intermediaryName, filter)) return true;
            if (!child.srgName.empty() && matchesFilter(child.srgName, filter)) return true;
            if (anyDescendantMatches(child, filter)) return true;
        }
        return false;
    };

    bool classMatch = classFilter.empty()
        || matchesFilter(node.name, classFilter)
        || matchesFilter(node.intermediaryName, classFilter)
        || matchesFilter(node.srgName, classFilter)
        || matchesFilter(node.displayPath, classFilter);

    if (!hasPipe) {
        if (classMatch) return true;
        return anyDescendantMatches(node, classFilter);
    }

    if (classMatch && anyDescendantMatches(node, memberFilter)) return true;
    for (const auto& child : node.children) {
        if (treeNodeMatchesFilter(child, classFilter, memberFilter, hasPipe)) return true;
    }
    return false;
}

void AppState::renderTreeNode(TreeNode& node, const std::string& classFilter, const std::string& memberFilter, bool filtering) {
    const bool showIntermediary = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowIntermediaryNames);
    const bool showSrg = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowSrgNames);

    auto matchesFilter = [](const std::string& str, const std::string& filter) -> bool {
        if (filter.empty()) return true;
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.find(filter) != std::string::npos;
    };

    if (node.type == TreeNode::Directory || node.type == TreeNode::Root) {
        if (filtering && node.children.empty()) return;

        if (filtering) {
            std::function<bool(const TreeNode&)> hasVisible = [&](const TreeNode& n) -> bool {
                for (auto& c : n.children) {
                    if (c.type == TreeNode::ClassEntry) {
                        if (treeNodeMatchesFilter(c, classFilter, memberFilter, hasPipe_)) return true;
                    } else if (c.type == TreeNode::Directory) {
                        if (hasVisible(c)) return true;
                    }
                }
                return false;
            };
            if (!hasVisible(node)) return;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        std::string nodePath = node.displayPath.empty() ? node.name : node.displayPath;
        if (filtering) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        } else {
            bool shouldBeOpen = expandedNodes_.count(nodePath) > 0;
            ImGui::SetNextItemOpen(shouldBeOpen, ImGuiCond_Always);
        }

        bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);

        if (!filtering) {
            if (open) {
                expandedNodes_.insert(nodePath);
            } else {
                expandedNodes_.erase(nodePath);
            }
        }

        if (open) {
            for (auto& child : node.children) {
                renderTreeNode(child, classFilter, memberFilter, filtering);
            }
            ImGui::TreePop();
        }
    } else if (node.type == TreeNode::ClassEntry) {
        std::string childFilter = hasPipe_ ? memberFilter : classFilter;
        bool hasActiveFilter = filtering && (!classFilter.empty() || !memberFilter.empty());

        bool classMatches = matchesFilter(node.name, classFilter)
            || matchesFilter(node.intermediaryName, classFilter)
            || matchesFilter(node.srgName, classFilter)
            || matchesFilter(node.displayPath, classFilter);
        bool anyChildMatchesFilter = false;
        if (!childFilter.empty()) {
            for (auto& child : node.children) {
                if (matchesFilter(child.name, childFilter)
                    || matchesFilter(child.intermediaryName, childFilter)
                    || matchesFilter(child.srgName, childFilter))
                {
                    anyChildMatchesFilter = true;
                    break;
                }
            }
        }

        bool visible;
        if (!hasActiveFilter) {
            visible = true;
        } else if (!hasPipe_) {
            visible = classMatches || anyChildMatchesFilter;
        } else {
            visible = (classFilter.empty() || classMatches) && (memberFilter.empty() || anyChildMatchesFilter);
        }
        if (!visible) return;

        std::string displayName = node.name;
        if (showIntermediary && !node.intermediaryName.empty()) {
            displayName += " [" + node.intermediaryName + "]";
        }
        if (showSrg && !node.srgName.empty()) {
            displayName += " [" + node.srgName + "]";
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selection_.type == Selection::Class && selection_.entryIndex == node.entryIndex) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string nodePath = node.displayPath;
        if (filtering && !node.children.empty()) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        } else {
            bool shouldBeOpen = expandedNodes_.count(nodePath) > 0;
            ImGui::SetNextItemOpen(shouldBeOpen, ImGuiCond_Always);
        }

        bool open = ImGui::TreeNodeEx(displayName.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            selection_.type = Selection::Class;
            selection_.entryIndex = node.entryIndex;
            selection_.memberIndex = -1;
        }

        if (!filtering) {
            if (open) {
                expandedNodes_.insert(nodePath);
            } else {
                expandedNodes_.erase(nodePath);
            }
        }

        if (open) {
            for (auto& child : node.children) {
                if (hasActiveFilter && !matchesFilter(child.name, childFilter)
                    && !matchesFilter(child.intermediaryName, childFilter)
                    && !matchesFilter(child.srgName, childFilter))
                {
                    continue;
                }
                if (child.type == TreeNode::Method) {
                    std::string childDisplay = child.name;
                    std::string suffix;
                    if (showIntermediary && !child.intermediaryName.empty()) {
                        suffix += " [" + child.intermediaryName + "]";
                    }
                    if (showSrg && !child.srgName.empty()) {
                        suffix += " [" + child.srgName + "]";
                    }
                    if (!suffix.empty()) {
                        size_t parenPos = childDisplay.rfind("(...)");
                        if (parenPos != std::string::npos) {
                            childDisplay = childDisplay.substr(0, parenPos) +
                                suffix + childDisplay.substr(parenPos);
                        } else {
                            childDisplay += suffix;
                        }
                    }
                    bool sel = selection_.type == Selection::Method &&
                               selection_.entryIndex == child.entryIndex &&
                               selection_.memberIndex == child.memberIndex;
                    if (ImGui::Selectable(("  " + childDisplay).c_str(), sel)) {
                        selection_.type = Selection::Method;
                        selection_.entryIndex = child.entryIndex;
                        selection_.memberIndex = child.memberIndex;
                    }
                } else if (child.type == TreeNode::Field) {
                    std::string childDisplay = child.name;
                    if (showIntermediary && !child.intermediaryName.empty()) {
                        childDisplay += " [" + child.intermediaryName + "]";
                    }
                    if (showSrg && !child.srgName.empty()) {
                        childDisplay += " [" + child.srgName + "]";
                    }
                    bool sel = selection_.type == Selection::Field &&
                               selection_.entryIndex == child.entryIndex &&
                               selection_.memberIndex == child.memberIndex;
                    if (ImGui::Selectable(("  " + childDisplay).c_str(), sel)) {
                        selection_.type = Selection::Field;
                        selection_.entryIndex = child.entryIndex;
                        selection_.memberIndex = child.memberIndex;
                    }
                }
            }
            ImGui::TreePop();
        }
    }
}

void AppState::renderRightPanel() {
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);

    float alpha = easeOutCubic(rightPanelAnim_);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    if (selection_.type == Selection::None) {
        ImGui::Text("Select a class, method, or field from the left panel.");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return;
    }

    switch (selection_.type) {
        case Selection::None:
            ImGui::Text("Select a class, method, or field from the left panel.");
            break;
        case Selection::Class:
            renderClassDetails(selection_.entryIndex);
            break;
        case Selection::Method:
            renderMethodDetails(selection_.entryIndex, selection_.memberIndex);
            break;
        case Selection::Field:
            renderFieldDetails(selection_.entryIndex, selection_.memberIndex);
            break;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

static void CopyableText(const char* text) {
    ImGui::PushID(static_cast<const void*>(text));
    ImGui::Text("%s", text);
    if (ImGui::BeginPopupContextItem("copy")) {
        if (ImGui::MenuItem("Copy")) {
            ImGui::SetClipboardText(text);
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void AppState::renderClassDetails(int entryIndex) {
    if (entryIndex < 0 || entryIndex >= (int)api_->getMappingData().entries.size()) return;
    const auto& entry = api_->getMappingData().entries[entryIndex];
    const auto& ci = entry.classInfo;

    const bool showIntermediaryRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowIntermediaryNamesRight);
    const bool showSrgRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowSrgNamesRight);

    // Class Info Table with animation
    float alpha = classInfoAnim_;
    if (alpha > 0.001f) {
        float easedAlpha = easeOutCubic(alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, easedAlpha);
        
        // Animated height offset
        float yOffset = (1.0f - easedAlpha) * 15.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

        ImGui::Text("Class Details");
        ImGui::Separator();

        if (ImGui::BeginTable("ClassInfo", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Deobfuscated");
            ImGui::TableNextColumn(); CopyableText(ci.deobfClass.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Official");
            ImGui::TableNextColumn(); CopyableText(ci.obfClass.c_str());

            if (showSrgRight && ci.srgClass) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("SRG");
                ImGui::TableNextColumn(); CopyableText(ci.srgClass->c_str());
            }

            if (showIntermediaryRight && ci.intermediaryClass) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Intermediary");
                ImGui::TableNextColumn(); CopyableText(ci.intermediaryClass->c_str());
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    ImGui::Spacing();

    // Methods Table with animation
    alpha = methodsAnim_;
    if (alpha > 0.001f) {
        float easedAlpha = easeOutCubic(alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, easedAlpha);
        
        float yOffset = (1.0f - easedAlpha) * 15.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

        ImGui::Separator();
        ImGui::Text("Methods (%zu)", entry.methods.size());
        ImGui::Separator();

        if (ImGui::BeginTable("Methods", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 150)))
        {
            ImGui::TableSetupColumn("Deobf");
            ImGui::TableSetupColumn("Official");
            if (showIntermediaryRight) ImGui::TableSetupColumn("Intermediary");
            if (showSrgRight) ImGui::TableSetupColumn("SRG");
            ImGui::TableSetupColumn("Descriptor");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < entry.methods.size(); i++) {
                const auto& m = entry.methods[i];
                ImGui::TableNextRow();
                auto navToMethod = [&]() {
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        selection_.type = Selection::Method;
                        selection_.memberIndex = (int)i;
                    }
                };
                ImGui::TableNextColumn(); CopyableText(m.deobfName.c_str()); navToMethod();
                ImGui::TableNextColumn(); CopyableText(m.obfName.c_str()); navToMethod();
                if (showIntermediaryRight) {
                    ImGui::TableNextColumn();
                    if (m.intermediaryName) {
                        CopyableText(m.intermediaryName->c_str());
                    } else {
                        ImGui::TextDisabled("-");
                    }
                    navToMethod();
                }
                if (showSrgRight) {
                    ImGui::TableNextColumn();
                    if (m.srgName) {
                        CopyableText(m.srgName->c_str());
                    } else {
                        ImGui::TextDisabled("-");
                    }
                    navToMethod();
                }
                ImGui::TableNextColumn(); CopyableText(m.jvmDescriptor.c_str()); navToMethod();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    ImGui::Spacing();

    // Fields Table with animation
    alpha = fieldsAnim_;
    if (alpha > 0.001f) {
        float easedAlpha = easeOutCubic(alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, easedAlpha);
        
        float yOffset = (1.0f - easedAlpha) * 15.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

        ImGui::Separator();
        ImGui::Text("Fields (%zu)", entry.fields.size());
        ImGui::Separator();

        if (ImGui::BeginTable("Fields", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                              ImVec2(0, 150)))
        {
            ImGui::TableSetupColumn("Deobf");
            ImGui::TableSetupColumn("Official");
            if (showIntermediaryRight) ImGui::TableSetupColumn("Intermediary");
            if (showSrgRight) ImGui::TableSetupColumn("SRG");
            ImGui::TableSetupColumn("Type");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < entry.fields.size(); i++) {
                const auto& f = entry.fields[i];
                ImGui::TableNextRow();
                auto navToField = [&]() {
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                        selection_.type = Selection::Field;
                        selection_.memberIndex = (int)i;
                    }
                };
                ImGui::TableNextColumn(); CopyableText(f.deobfName.c_str()); navToField();
                ImGui::TableNextColumn(); CopyableText(f.obfName.c_str()); navToField();
                if (showIntermediaryRight) {
                    ImGui::TableNextColumn();
                    if (f.intermediaryName) {
                        CopyableText(f.intermediaryName->c_str());
                    } else {
                        ImGui::TextDisabled("-");
                    }
                    navToField();
                }
                if (showSrgRight) {
                    ImGui::TableNextColumn();
                    if (f.srgName) {
                        CopyableText(f.srgName->c_str());
                    } else {
                        ImGui::TextDisabled("-");
                    }
                    navToField();
                }
                ImGui::TableNextColumn(); CopyableText(f.type.c_str()); navToField();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
}

void AppState::renderMethodDetails(int entryIndex, int memberIndex) {
    if (entryIndex < 0 || entryIndex >= (int)api_->getMappingData().entries.size()) return;
    const auto& entry = api_->getMappingData().entries[entryIndex];
    if (memberIndex < 0 || memberIndex >= (int)entry.methods.size()) return;
    const auto& m = entry.methods[memberIndex];

    const bool showIntermediaryRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowIntermediaryNamesRight);
    const bool showSrgRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowSrgNamesRight);

    float alpha = methodInfoAnim_;
    if (alpha > 0.001f) {
        float easedAlpha = easeOutCubic(alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, easedAlpha);
        
        float yOffset = (1.0f - easedAlpha) * 15.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

        ImGui::Text("Method Details");
        ImGui::Separator();

        if (ImGui::BeginTable("MethodInfo", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Deobfuscated");
            ImGui::TableNextColumn(); CopyableText(m.deobfName.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Official");
            ImGui::TableNextColumn(); CopyableText(m.obfName.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Descriptor");
            ImGui::TableNextColumn(); CopyableText(m.jvmDescriptor.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Return Type");
            ImGui::TableNextColumn(); CopyableText(m.returnType.c_str());

            if (!m.paramTypes.empty()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Parameters");
                std::string params;
                for (size_t i = 0; i < m.paramTypes.size(); i++) {
                    if (i > 0) params += ", ";
                    params += m.paramTypes[i];
                }
                ImGui::TableNextColumn(); CopyableText(params.c_str());
            }

            if (showSrgRight && m.srgName) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("SRG");
                ImGui::TableNextColumn(); CopyableText(m.srgName->c_str());
            }

            if (showIntermediaryRight && m.intermediaryName) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Intermediary");
                ImGui::TableNextColumn(); CopyableText(m.intermediaryName->c_str());
            }

            if (m.startLine) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Line Range");
                if (m.endLine) {
                    std::string lineRange = std::to_string(*m.startLine) + " - " + std::to_string(*m.endLine);
                    ImGui::TableNextColumn(); CopyableText(lineRange.c_str());
                } else {
                    std::string lineRange = std::to_string(*m.startLine);
                    ImGui::TableNextColumn(); CopyableText(lineRange.c_str());
                }
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Text("Class: %s", entry.classInfo.deobfClass.c_str());
        if (ImGui::Button("Show Class")) {
            selection_.type = Selection::Class;
            selection_.entryIndex = entryIndex;
            selection_.memberIndex = -1;
        }
        
        ImGui::PopStyleVar();
    }
}

void AppState::renderFieldDetails(int entryIndex, int memberIndex) {
    if (entryIndex < 0 || entryIndex >= (int)api_->getMappingData().entries.size()) return;
    const auto& entry = api_->getMappingData().entries[entryIndex];
    if (memberIndex < 0 || memberIndex >= (int)entry.fields.size()) return;
    const auto& f = entry.fields[memberIndex];

    const bool showIntermediaryRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowIntermediaryNamesRight);
    const bool showSrgRight = mcobfF::Settings::instance().get<bool>(mcobfF::SettingKey::ShowSrgNamesRight);

    float alpha = fieldInfoAnim_;
    if (alpha > 0.001f) {
        float easedAlpha = easeOutCubic(alpha);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, easedAlpha);
        
        float yOffset = (1.0f - easedAlpha) * 15.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);

        ImGui::Text("Field Details");
        ImGui::Separator();

        if (ImGui::BeginTable("FieldInfo", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Deobfuscated");
            ImGui::TableNextColumn(); CopyableText(f.deobfName.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Official");
            ImGui::TableNextColumn(); CopyableText(f.obfName.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Type");
            ImGui::TableNextColumn(); CopyableText(f.type.c_str());

            if (showSrgRight && f.srgName) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("SRG");
                ImGui::TableNextColumn(); CopyableText(f.srgName->c_str());
            }

            if (showIntermediaryRight && f.intermediaryName) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Intermediary");
                ImGui::TableNextColumn(); CopyableText(f.intermediaryName->c_str());
            }

            if (f.lineNumber) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Line");
                ImGui::TableNextColumn(); CopyableText(std::to_string(*f.lineNumber).c_str());
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::Text("Class: %s", entry.classInfo.deobfClass.c_str());
        if (ImGui::Button("Show Class")) {
            selection_.type = Selection::Class;
            selection_.entryIndex = entryIndex;
            selection_.memberIndex = -1;
        }
        
        ImGui::PopStyleVar();
    }
}
