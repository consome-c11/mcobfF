#pragma once

#include "MappingData.h"
#include <string>

namespace mcobfF
{
    class SRGParser
    {
    public:
        [[nodiscard]] static bool parse(const std::string& content, MappingData& mappings);

        [[nodiscard]] static bool parseStandalone(const std::string& content, MappingData& mappings);

    private:
        static void parseDescriptor(const std::string& desc, std::string& returnType,
                                    std::vector<std::string>& paramTypes);
        static void processTokens(const std::vector<std::string>& tokens, MappingEntry*& currentEntry,
                                  MethodMapping*& currentMethod, const MappingData& mappings);
        static std::string trimCR(const std::string& str);

        static bool parseOldSrg(const std::string& content, MappingData& mappings);
        static bool parseTsrg(const std::string& content, MappingData& mappings);
        static bool isOldSrgFormat(const std::string& content);
    };
} // namespace mc