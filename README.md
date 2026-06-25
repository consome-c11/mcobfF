# MC-OBF-Finder (mcobfF)

This tool is an unofficial community project and is not affiliated with or endorsed by Mojang Studios, Microsoft, Minecraft Forge, or the Fabric Project.

The copyright for each piece of mapping data obtained by this tool belongs to the respective projects (Mojang Studios, Fabric Project, Forge).

Please do not publish or upload the dumped JSON file online.
Redistributing official Mojang maps constitutes a breach of the ‘Minecraft’ End User Licence Agreement (EULA).
Please use this solely for development in a local environment or for personal development purposes.

## Features

- **Interactive GUI**: Browse mappings in a tree view with live search/filter
- **Multiple Mapping Formats**: Mojang mappings (TSRG), SRG, and Fabric Intermediary (Tiny)
- **Auto-Download**: Fetches version manifest, client JARs, and mappings on demand
- **Inheritance Resolution**: Enhanced mapping resolution via class hierarchy analysis
- **Mapping Detail Panel**: View deobfuscated, obfuscated, intermediary, and SRG names side by side
- **Cache Management**: Caches downloaded JARs and mappings locally
- **Library API**: Also usable as a standalone library for programmatic access

## Building

### Requirements
- Windows (Win32 API, DirectX 11)
- C++20 compatible compiler (MSVC)
- CMake 3.20+

### Build Steps

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Dependencies (auto-fetched via CMake FetchContent):
- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0
- [miniz](https://github.com/richgel999/miniz) v3.1.1
- [dear imgui](https://github.com/ocornut/imgui) v1.91.0

## Usage

Launch the executable to open the GUI window:

1. **Select Version**: Click `File > Select Version...` to fetch the version manifest, then choose a release or snapshot
2. **Browse Tree**: The left panel shows a hierarchical class tree; expand classes to see methods and fields
3. **Search**: Type in the filter box to search (use `|` to separate class and member filters, e.g. `block|getState`)
4. **Details**: Click a class/method/field to view all mapping names in the right panel

## Library Usage

```cpp
#include "mcobfF/api/api.h"

int main() {
    mcobfF::api api("./cache");
    
    // Load mappings for a specific version
    if (!api.loadMappings("1.21.1")) {
        return 1;
    }
    
    // Resolve class name (deobfuscated -> obfuscated)
    auto obfClass = api.resolveClass("net.minecraft.world.level.block.Block", true);
    if (obfClass) {
        // ...
    }
    
    // Resolve method (deobfuscated -> obfuscated)
    auto obfMethod = api.resolveMethod(
        "net.minecraft.world.level.block.Block",
        "getStateDefinition",
        {"net.minecraft.world.level.block.state.BlockBehaviour$Properties"},
        true
    );
    
    // Resolve field (obfuscated -> deobfuscated)
    auto deobfField = api.resolveField("abc", "field_1234", false);
    
    return 0;
}
```

### With Inheritance Resolution

```cpp
api.loadMappingsWithInheritance("1.21.1", "path/to/client.jar");
```

### Dumping Mappings

```cpp
// Dump mappings for a version to JSON
mcobfF::api::dumpMappings("1.21.1", "mappings.json");

// Dump mappings from a specific JAR
mcobfF::api::dumpJarMappings("client.jar", "1.21.1", "jar_mappings.json");
```

### Version Discovery

```cpp
auto release = mcobfF::api::getLatestReleaseVersion();
auto snapshot = mcobfF::api::getLatestSnapshotVersion();
```

## API Reference

### `mcobfF::api`

| Method | Description |
|--------|-------------|
| `api()` | Create with empty cache directory |
| `api(cacheDir)` | Create with custom cache directory |
| `loadMappings(version)` | Load Mojang + SRG + Intermediary mappings |
| `loadMappingsWithInheritance(version, jarPath)` | Load mappings + enhance with JAR analysis |
| `resolveClass(name, deobfToObf)` | Resolve class name |
| `resolveMethod(class, method, params, deobfToObf)` | Resolve method name |
| `resolveField(class, field, deobfToObf)` | Resolve field name |
| `loadJar(jarPath)` | Load JAR for analysis |
| `buildClassHierarchy()` | Build class hierarchy from loaded JAR |
| `dumpMappings(version, outputPath)` | Static: dump version mappings to file |
| `dumpJarMappings(jarPath, version, outputPath)` | Static: dump JAR mappings to file |
| `getMappingData()` | Get raw mapping data |
| `isMappingLoaded()` | Check if mappings are loaded |
| `getCurrentVersion()` | Get currently loaded version |
| `getLatestReleaseVersion()` | Static: get latest release version string |
| `getLatestSnapshotVersion()` | Static: get latest snapshot version string |

## Project Structure

```
main.cpp             # WinMain entry, ImGui + DX11 setup
gui/
├── AppState.h/cpp   # GUI state, tree building, rendering logic

mcobfF/
├── api/              # Public API (api.h/cpp)
├── class/            # Class file parsing & hierarchy
│   ├── ClassFileParser     # .class file binary parser
│   ├── ClassHierarchyBuilder # Build hierarchy from JAR
│   └── ClassInfo           # Data structures
├── config/           # Configuration (API URLs, user agent)
├── dumper/           # Mapping export (JarDumper)
├── file/             # Filesystem utilities
├── mapping/          # Mapping parsers & resolvers
│   ├── MappingData        # Core data structures & queries
│   ├── MappingParser      # Mojang mappings parser
│   ├── TinyMappingParser  # Tiny v2 (Fabric Intermediary) parser
│   ├── SRGParser          # SRG format parser
│   ├── SRGResolver        # SRG download & resolution
│   ├── MappingResolver    # Unified resolution orchestration
│   └── InheritanceResolver # Inheritance-based enhancement
├── network/          # HTTP client & version discovery
├── zip/              # ZIP/JAR archive handling
├── Types.h           # Common type definitions
└── Logger.h          # Logging utility
```

## License

WTFPL License - see LICENSE file for details.
