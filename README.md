# MC-OBF-Finder (mcobfF)

This tool is an unofficial community project and is not affiliated with or endorsed by Mojang Studios, Microsoft,
Minecraft Forge, or the Fabric Project.

The copyright for each piece of mapping data obtained by this tool belongs to the respective projects (Mojang Studios,
Fabric Project, Forge).

Decompiled source code is owned by Mojang Studios and is provided for educational/reference purposes only.
Redistribution of decompiled source code is strictly prohibited.

Please do not publish or upload the dumped JSON file or decompiled source code online.
Redistributing official Mojang maps or decompiled code constitutes a breach of the ‘Minecraft’ End User Licence Agreement (EULA).
Please use this solely for development in a local environment or for personal development purposes.

## Features

- **Interactive GUI**: Browse mappings in a tree view with live search/filter
- **Multiple Mapping Formats**: Mojang mappings , SRG, and Fabric Intermediary
- **Auto-Download**: Fetches version manifest, client JARs, mappings, and JRE on demand
- **Inheritance Resolution**: Enhanced mapping resolution via class hierarchy analysis
- **Decompilation**: Decompiles classes using Vineflower, with batch decompile support
- **Mapping Detail Panel**: View deobfuscated, obfuscated, intermediary, and SRG names side by side
- **Library API**: Also usable as a standalone library for programmatic access

## Building

### Requirements

- Windows (Win32 API, DirectX 11)
- C++20 compatible compiler (MSVC)
- CMake 3.21+

### Build Steps

```bash
mkdir build && cd build
cmake --build . --config Release
```

### Dependencies (auto-fetched via CMake FetchContent)

| Dependency                                          | Version | Purpose                     |
|-----------------------------------------------------|---------|-----------------------------|
| [nlohmann/json](https://github.com/nlohmann/json)   | v3.12.0 | JSON parsing                |
| [miniz](https://github.com/richgel999/miniz)        | v3.1.1  | ZIP/JAR handling            |
| [Dear ImGui](https://github.com/ocornut/imgui)      | v1.91.0 | GUI framework               |
| [magic_enum](https://github.com/Neargye/magic_enum) | v0.9.7  | Static reflection for enums |
| [JNIHook](https://github.com/rdbo/jnihook)          | v2.2    | JNI function hooking        |

### Runtime Dependencies

- [Vineflower](https://github.com/Vineflower/vineflower) 1.12.0 — auto-downloaded on first use for decompile
- [jarremapper](https://github.com/consome-c11/jarremapper) 0.0.1 — auto-downloaded on first use for JAR remapping
- **JRE 17+** — auto-downloaded from Adoptium Temurin if no Java runtime is detected

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

### With Inheritance Resolution, Remapping & Decompilation

```cpp
api.loadMappingsWithInheritance("1.21.1", "path/to/client.jar");
// This also:
//   - Builds class hierarchy from the JAR
//   - Downloads & runs jarremapper to produce a remapped JAR
//   - Initializes Vineflower decompiler
//   - Starts background batch decompilation of all classes
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

| Method                                             | Description                                                          |
|----------------------------------------------------|----------------------------------------------------------------------|
| `api()`                                            | Create with empty cache directory                                    |
| `api(cacheDir)`                                    | Create with custom cache directory                                   |
| `loadMappings(version)`                            | Load Mojang + SRG + Intermediary mappings                            |
| `loadMappingsWithInheritance(version, jarPath)`    | Load mappings, build hierarchy, remap JAR, and start batch decompile |
| `resolveClass(name, deobfToObf)`                   | Resolve class name                                                   |
| `resolveMethod(class, method, params, deobfToObf)` | Resolve method name                                                  |
| `resolveField(class, field, deobfToObf)`           | Resolve field name                                                   |
| `loadJar(jarPath)`                                 | Load JAR for analysis                                                |
| `buildClassHierarchy()`                            | Build class hierarchy from loaded JAR                                |
| `dumpMappings(version, outputPath)`                | Static: dump version mappings to file                                |
| `dumpJarMappings(jarPath, version, outputPath)`    | Static: dump JAR mappings to file                                    |
| `getMappingData()`                                 | Get raw mapping data                                                 |
| `isMappingLoaded()`                                | Check if mappings are loaded                                         |
| `getCurrentVersion()`                              | Get currently loaded version                                         |
| `getLatestReleaseVersion()`                        | Static: get latest release version string                            |
| `getLatestSnapshotVersion()`                       | Static: get latest snapshot version string                           |
| `initializeDecompiler()`                           | Initialize Vineflower JVM decompiler                                 |
| `decompileClass(className)`                        | Decompile a single class (with caching)                              |
| `decompileAndRemapClass(className)`                | Decompile and remap a class                                          |
| `isDecompilerAvailable()`                          | Check if decompiler is initialized                                   |
| `startDecompileAllAsync()`                         | Start batch decompilation of all JAR classes in background           |
| `cancelDecompileAll()`                             | Cancel running batch decompilation                                   |
| `isDecompilingAll()`                               | Check if batch decompilation is running                              |
| `getDecompileProgress()`                           | Get batch decompilation progress (0.0 – 1.0)                         |
| `getCurrentJarPath()`                              | Get path to currently loaded JAR                                     |
| `getDecompileCacheDir()`                           | Get decompile cache directory path                                   |
| `hasDecompiledCache(className)`                    | Check if a class has been cached after decompilation                 |
| `getMappingFilePath()`                             | Get path to generated Tiny mapping file                              |
| `tryPackObff()`                                   | Pack decompiled cache into OBFF archive (called automatically after batch decompile) |

## Project Structure

```
main.cpp                    # WinMain entry, ImGui + DX11 setup
gui/
├── AppState.h/cpp          # GUI state, tree building, rendering logic
└── config/
    └── Settings.h/cpp      # GUI settings (persistence)

mcobfF/
├── api/
│   └── api.h/cpp           # Public API — mapping, decompilation, remapping orchestration
├── class/                  # Class file parsing & hierarchy
│   ├── ClassFileParser.h/cpp      # .class file binary parser
│   ├── ClassHierarchyBuilder.h/cpp # Build inheritance hierarchy from JAR
│   └── ClassInfo.h                # Data structures (ClassInfo, ClassHierarchy)
├── config/
│   └── ApiConfig.h                # API URLs, user agent, download links
├── decompiler/             # Java decompilation
│   ├── FernflowerDecompiler.h/cpp # Vineflower JVM launcher via JNI
│   └── JavaMethodParser.h/cpp     # Java source method signature extraction
├── dumper/
│   └── JarDumper.h/cpp            # Mapping export to JSON
├── file/
│   └── FileSystem.h/cpp           # Filesystem utilities (cache paths, directory ops)
├── mapping/                # Mapping parsers & resolvers
│   ├── MappingData.h/cpp          # Core data structures & query methods
│   ├── MappingResolver.h/cpp      # Unified resolution orchestration
│   ├── MappingWriter.h/cpp        # Mapping serialization to Tiny v2 format
│   ├── MojMapParser.h/cpp         # Mojang mappings (TSRG) parser
│   ├── TinyMappingParser.h/cpp    # Tiny v2 (Fabric Intermediary) parser
│   ├── SRGParser.h/cpp            # SRG format parser
│   ├── SRGResolver.h/cpp          # SRG download & resolution
│   └── InheritanceResolver.h/cpp  # Inheritance-based mapping enhancement
├── network/                # HTTP client & version discovery
│   ├── HttpsClient.h/cpp          # WinHTTP-based HTTPS client
│   ├── JreDownloader.h/cpp        # JRE auto-download & verification
│   └── VersionDownloader.h/cpp    # Version manifest & JAR downloading
├── obff/
│   └── OBFFArchive.h/cpp          # OBFF archive reading (obfuscated file format)
├── zip/
│   └── ZipArchive.h/cpp           # ZIP/JAR archive reading
├── Types.h                 # Common type definitions
└── Logger.h                # Logging utility
```

## Credits

This project uses data and tools from the following projects:

- **Vineflower** ([vineflower.org](https://vineflower.org)) — Java decompiler engine
- **Mojang Mapping Data** — Official obfuscation mappings (TSRG format)
- **Fabric Intermediary** ([Fabric MC](https://fabricmc.net/)) — Intermediary mappings (Tiny v2)
- **MCP / SRG
  ** ([MinecraftForge / MCPConfig](https://github.com/MinecraftForge/MCPConfig), [NeoForged / MCPConfig](https://github.com/neoforged/MCPConfig), [MCPBot](https://mcpbot.unascribed.com/)) —
  SRG mappings and MCP names
- **[nlohmann/json](https://github.com/nlohmann/json)** — JSON parsing
- **[miniz](https://github.com/richgel999/miniz)** — ZIP/JAR handling
- **[Dear ImGui](https://github.com/ocornut/imgui)** — GUI framework

## License

WTFPL License — see LICENSE file for details.
