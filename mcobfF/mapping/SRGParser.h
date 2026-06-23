#pragma once

#include "MappingData.h"
#include <string>

namespace mcobfF
{
    class SRGParser
    {
    public:
        [[nodiscard]] static bool parse(const std::string& content, MappingData& mappings);

    private:
        static void parseDescriptor(const std::string& desc, std::string& returnType,
                                    std::vector<std::string>& paramTypes);
        static void processTokens(const std::vector<std::string>& tokens, MappingEntry*& currentEntry,
                                  MethodMapping*& currentMethod, const MappingData& mappings);
        static std::string trimCR(const std::string& str);
    };
} // namespace mc
