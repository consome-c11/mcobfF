# MC-OBF-Find (mcobfF)

A C++20 library for Minecraft obfuscation mapping resolution. Provides tools to load Mojang mappings and SRG mappings, resolve class/method/field names between obfuscated and deobfuscated forms, build class hierarchies from JAR files, and dump mappings.

## Features

- **Mapping Resolution**: Convert between obfuscated (e.g., `abc`) and deobfuscated (e.g., `Block`) names for classes, methods, and fields
- **Multiple Mapping Formats**: Supports Mojang mappings (TSRG) and SRG mappings
- **Inheritance Resolution**: Enhanced mapping resolution using class hierarchy analysis from JAR files
- **Version Management**: Download and cache mappings for specific Minecraft versions
- **JAR Analysis**: Load and analyze JAR files to build class hierarchies
- **Mapping Dump Mappings Dump**: Export mappings to various formats

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

The project uses FetchContent to automatically download:
- [nlohmann/json](https://github.com/nlohmann/json) v3.12.0
- [miniz](https://github.com/richgel999/miniz) v3.1.1

## Usage

### Basic Example

```cpp
#include "mcobfF/api/mcobfF.h"

int main() {
    mcobfF::mcobfF api("./cache");
    
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
api.loadMappingsWithInheritance("1.21.1", "path/to/minecraft_client.jar");

// Now resolution includes inherited methods/fields
```

### Dumping Mappings

```cpp
// Dump mappings for a version to JSON
mcobfF::mcobfF::dumpMappings("1.21.1", "mappings.json");

// Dump mappings from a specific JAR
mcobfF::mcobfF::dumpJarMappings("client.jar", "1.21.1", "jar_mappings.json");
```

### Version Discovery

```cpp
// Get latest release version
auto release = mcobfF::mcobfF::getLatestReleaseVersion();

// Get latest snapshot version
auto snapshot = mcobfF::mcobfF::getLatestSnapshotVersion();
```

## API Reference

### `mcobfF::api`
Main entry point class.

| Method | Description |
|--------|-------------|
| `mcobfF()` | Create with default cache directory |
| `mcobfF(cacheDir)` | Create with custom cache directory |
| `loadMappings(version)` | Load Mojang + SRG mappings for version |
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

## Project Structure

```
mcobfF/
├── api/              # Public API (api.h/cpp)
├── class/            # Class file parsing & hierarchy
├── dumper/           # Mapping export functionality
├── file/             # Filesystem utilities
├── mapping/          # Mapping parsers & resolvers
│   ├── MappingParser     # Base parser
│   ├── TinyMappingParser # Tiny v2 format
│   ├── SRGParser         # SRG format
│   ├── SRGResolver       # SRG resolution
│   ├── MappingResolver   # Unified resolution
│   ├── InheritanceResolver # Inheritance enhancement
│   └── MappingData       # Data structures
├── network/          # Version downloading
├── zip/              # ZIP/JAR handling
├── Types.h           # Common type definitions
└── Logger.h          # Logging utility
```

## License

MIT License - see LICENSE file for details.