#pragma once

namespace mcobfF
{
    struct ApiConfig
    {
        static constexpr const char* USER_AGENT = "mcobfF/1.0";

        static constexpr const char* MANIFEST_URL =
            "https://launchermeta.mojang.com/mc/game/version_manifest.json";

        static constexpr const char* FABRIC_INTERMEDIARY_MAVEN =
            "https://maven.fabricmc.net/net/fabricmc/intermediary";

        static constexpr const char* FORGE_MCPCONFIG_RAW =
            "https://raw.githubusercontent.com/MinecraftForge/MCPConfig/master/versions/release";

        static constexpr const char* NEOFORGED_MCPCONFIG_RAW =
            "https://raw.githubusercontent.com/neoforged/MCPConfig/master/versions/release";

        static constexpr const char* FORGE_MAVEN =
            "https://files.minecraftforge.net/maven/de/oceanlabs/mcp/mcp";

        static constexpr const char* MCPBOT_BASE =
            "https://mcpbot.unascribed.com/mcp";

        static constexpr const char* DECOMPILER_JAR_URL =
            "https://repo1.maven.org/maven2/org/vineflower/vineflower/1.12.0/vineflower-1.12.0.jar";

        static constexpr const char* JAR_REMAPPER_JAR_URL =
            "https://github.com/consome-c11/jarremapper/releases/download/0.0.1/jarremapper.jar";
    };
} // namespace mc
