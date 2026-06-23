# MC-OBF-Find (mcobfF)

A C++20 CLI tool and library for Minecraft obfuscation mapping resolution. Provides mapping dump from Mojang mappings, SRG mappings, and Fabric Intermediary, with support for class/method/field name resolution and class hierarchy analysis from JAR files.

## Features

- **Mapping Dump**: Export class/method/field mappings from a Minecraft JAR or directly by version
- **Multiple Mapping Formats**: Supports Mojang mappings (TSRG), SRG, and Fabric Intermediary (Tiny v2)
- **Inheritance Resolution**: Enhanced mapping resolution using class hierarchy analysis from JAR files
- **Version Management**: Download and cache mappings for specific Minecraft versions
- **JAR Analysis**: Load and analyze JAR files to build class hierarchies
- **CLI & Library**: Usable as a standalone command-line tool or integrated as a library

## Building

### Requirements
- C++20 compatible compiler (MSVC, GCC, Clang)
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

## CLI Usage

```bash
# Dump mappings from a local JAR
mcobfF client.jar 1.21.1 mappings.json

# Dump mappings by version (auto-downloads JAR)
mcobfF --version 1.21.1 mappings.json
mcobfF -v 1.21.1 mappings.json

# Dump mappings for the latest release
mcobfF --latest-release mappings.json
mcobfF -r mappings.json

# Dump mappings for the latest snapshot
mcobfF --latest-snapshot mappings.json
mcobfF -s mappings.json
```

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
        std::cout << "Block -> " << *obfClass << std::endl;
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
// Load mappings and enhance with inheritance analysis from a JAR
api.loadMappingsWithInheritance("1.21.1", "path/to/client.jar");
```

### Dumping Mappings (Library)

```cpp
// Dump mappings for a version to JSON
mcobfF::api::dumpMappings("1.21.1", "mappings.json");

// Dump mappings from a specific JAR
mcobfF::api::dumpJarMappings("client.jar", "1.21.1", "jar_mappings.json");
```

### Version Discovery

```cpp
// Get latest release version
auto release = mcobfF::api::getLatestReleaseVersion();

// Get latest snapshot version
auto snapshot = mcobfF::api::getLatestSnapshotVersion();
```

## API Reference

### `mcobfF::api`
Main entry point class.

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

MIT License - see LICENSE file for details.