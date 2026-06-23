#pragma once
#include <string>
#include <vector>
#include <memory>
#include <future>
#include <set>

struct HWND__;
using HWND = HWND__*;

namespace mcobfF {
    class api;
    struct MappingData;
}

struct TreeNode {
    enum Type { Root, Directory, ClassEntry, Method, Field };
    Type type = Root;
    std::string name;
    std::string intermediaryName;
    std::string srgName;
    std::string displayPath;
    int entryIndex = -1;
    int memberIndex = -1;
    std::vector<TreeNode> children;
};

struct FlatNode {
    const TreeNode* node;
    int depth;
    bool isOpen;
    TreeNode::Type type;
    std::string displayName;
};

struct Selection {
    enum Type { None, Class, Method, Field };
    Type type = None;
    int entryIndex = -1;
    int memberIndex = -1;
};

class AppState {
public:
    AppState();
    ~AppState();

    void setHwnd(HWND hwnd);
    void update();
    void renderGui();

private:
    HWND hwnd_ = nullptr;
    void startFetchManifest();
    void startLoadVersion(const std::string& version);
    void buildTree();
    void updateDisplayedVersions();

    void renderTitleBar();
    void renderVersionSelector();
    void renderSettingsWindow();
    void clearCache();
    void renderLeftPanel();
    void flattenTree(std::vector<FlatNode>& out, const TreeNode& node, int depth);
    static std::string buildDisplayName(const TreeNode& node);
    void renderRightPanel();
    void renderClassDetails(int entryIndex);
    void renderMethodDetails(int entryIndex, int memberIndex);
    void renderFieldDetails(int entryIndex, int memberIndex);
    static bool treeNodeMatchesFilter(const TreeNode& node, const std::string& classFilter, const std::string& memberFilter, bool hasPipe);
    void renderTreeNode(TreeNode& node, const std::string& classFilter, const std::string& memberFilter, bool filtering);

    struct VersionEntry {
        std::string id;
        std::string type;
        std::string releaseTime;
    };

    std::future<void> manifestFuture_;
    std::future<bool> loadFuture_;

    bool manifestFetching_ = false;
    bool manifestLoaded_ = false;
    std::string manifestError_;
    std::vector<VersionEntry> allVersions_;
    char versionFilter_[64] = {};
    bool versionSelectorOpen_ = false;
    bool settingsOpen_ = false;
    std::vector<const VersionEntry*> displayedVersions_;

    bool loading_ = false;
    bool loaded_ = false;
    std::string loadStatus_;
    std::string loadError_;
    std::string selectedVersion_;

    std::unique_ptr<mcobfF::api> api_;
    std::string cacheDir_;

    TreeNode treeRoot_;
    char classFilter_[256] = {};
    Selection selection_;

    std::set<std::string> expandedNodes_;
    std::set<std::string> savedExpandedNodes_;
    bool wasFiltering_ = false;
    bool hasPipe_ = false;

    std::vector<FlatNode> cachedDisplayList_;
    bool displayListDirty_ = true;
    char lastFilter_[256] = {};
};
