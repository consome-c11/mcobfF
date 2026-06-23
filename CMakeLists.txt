cmake_minimum_required(VERSION 3.20)
project(mcobfF)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# miniz
FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG 3.1.1
)
FetchContent_MakeAvailable(miniz)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES}
        main.cpp
        mcobfF/class/ClassFileParser.h
        mcobfF/class/ClassFileParser.cpp
        mcobfF/class/ClassHierarchyBuilder.h
        mcobfF/class/ClassHierarchyBuilder.cpp
        mcobfF/class/ClassInfo.h
        mcobfF/dumper/JarDumper.h
        mcobfF/dumper/JarDumper.cpp
        mcobfF/file/FileSystem.h
        mcobfF/file/FileSystem.cpp
        mcobfF/mapping/InheritanceResolver.h
        mcobfF/mapping/InheritanceResolver.cpp
        mcobfF/mapping/MappingData.h
        mcobfF/mapping/MappingData.cpp
        mcobfF/mapping/MappingParser.h
        mcobfF/mapping/MappingParser.cpp
        mcobfF/mapping/MappingResolver.h
        mcobfF/mapping/MappingResolver.cpp
        mcobfF/mapping/SRGParser.h
        mcobfF/mapping/SRGParser.cpp
        mcobfF/mapping/SRGResolver.h
        mcobfF/mapping/SRGResolver.cpp
        mcobfF/mapping/TinyMappingParser.h
        mcobfF/mapping/TinyMappingParser.cpp
        mcobfF/network/HttpsClient.h
        mcobfF/network/HttpsClient.cpp
        mcobfF/network/VersionDownloader.h
        mcobfF/network/VersionDownloader.cpp
        mcobfF/zip/ZipArchive.h
        mcobfF/zip/ZipArchive.cpp
        mcobfF/api/api.h
        mcobfF/api/api.cpp)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "/" mcobfF mcobfF/api mcobfF/dumper ${miniz_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json miniz winhttp)
