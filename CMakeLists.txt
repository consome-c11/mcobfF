cmake_minimum_required(VERSION 3.21)
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

# dear imgui
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.0
)
FetchContent_MakeAvailable(imgui)

# magic_enum
FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
        GIT_TAG v0.9.7
)
FetchContent_MakeAvailable(magic_enum)

# JNIHook
FetchContent_Declare(
        jnihook
        GIT_REPOSITORY https://github.com/rdbo/jnihook.git
        GIT_TAG 2.2
)
FetchContent_MakeAvailable(jnihook)

set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES}
        main.cpp
        gui/AppState.h
        gui/AppState.cpp
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
        mcobfF/mapping/MappingWriter.h
        mcobfF/mapping/MappingWriter.cpp
        mcobfF/mapping/MojMapParser.h
        mcobfF/mapping/MojMapParser.cpp
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
        mcobfF/network/JreDownloader.h
        mcobfF/network/JreDownloader.cpp
        mcobfF/network/VersionDownloader.h
        mcobfF/network/VersionDownloader.cpp
        mcobfF/zip/ZipArchive.h
        mcobfF/zip/ZipArchive.cpp
        mcobfF/api/api.h
        mcobfF/api/api.cpp
        mcobfF/decompiler/FernflowerDecompiler.h
        mcobfF/decompiler/FernflowerDecompiler.cpp

        mcobfF/decompiler/JavaMethodParser.h
        mcobfF/decompiler/JavaMethodParser.cpp
        mcobfF/obff/OBFFArchive.h
        mcobfF/obff/OBFFArchive.cpp
        gui/config/Settings.h
        gui/config/Settings.cpp)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
else ()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif ()

# Find JNI (requires JAVA_HOME to be set)
set(JAVA_HOME "$ENV{JAVA_HOME}")
if (JAVA_HOME)
    file(TO_CMAKE_PATH "$ENV{JAVA_HOME}" JAVA_HOME)
endif ()

if (JAVA_HOME AND EXISTS "${JAVA_HOME}/include/jni.h")
    set(JAVA_INCLUDES "${JAVA_HOME}/include")
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        list(APPEND JAVA_INCLUDES "${JAVA_HOME}/include/win32")
        set(JAVA_LIB_DIRS "${JAVA_HOME}/lib" "${JAVA_HOME}/lib/server")
    else ()
        list(APPEND JAVA_INCLUDES "${JAVA_HOME}/include/linux")
        set(JAVA_LIB_DIRS "${JAVA_HOME}/lib" "${JAVA_HOME}/lib/server" "${JAVA_HOME}/jre/lib/amd64/server")
    endif ()
    message(STATUS "Found JNI at ${JAVA_HOME}")
else ()
    message(WARNING "JAVA_HOME not set or invalid - JNI/JNIHook will not be available")
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE "/" mcobfF mcobfF/api mcobfF/dumper gui ${miniz_SOURCE_DIR} ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${JAVA_INCLUDES} ${jnihook_SOURCE_DIR}/include)
target_link_directories(${PROJECT_NAME} PRIVATE ${JAVA_LIB_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json miniz magic_enum::magic_enum jnihooksingle winhttp d3d11 d3dcompiler dwmapi comdlg32)
