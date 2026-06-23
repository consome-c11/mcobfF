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
    };
} // namespace mc
