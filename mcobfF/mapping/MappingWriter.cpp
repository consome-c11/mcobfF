#include "MappingWriter.h"
#include <fstream>
#include <sstream>

namespace mcobfF
{
    std::string MappingWriter::generate(const MappingData& mappings)
    {
        std::ostringstream out;

        out << "v1\tofficial\tnamed\n";

        for (const auto& entry : mappings.entries)
        {
            const auto& ci = entry.classInfo;

            out << "CLASS\t" << ci.obfClass << "\t" << ci.deobfClass << "\n";

            for (const auto& field : entry.fields)
            {
                if (field.obfName == field.deobfName) continue;
                std::string fieldType = field.type.empty() ? "I" : field.type;
                out << "FIELD\t" << ci.obfClass << "\t" << fieldType << "\t" << field.obfName
                    << "\t" << field.deobfName << "\n";
            }

            for (const auto& method : entry.methods)
            {
                if (method.obfName == method.deobfName) continue;
                if (method.obfName == "<init>" || method.obfName == "<clinit>") continue;
                out << "METHOD\t" << ci.obfClass << "\t" << method.jvmDescriptor
                    << "\t" << method.obfName
                    << "\t" << method.deobfName << "\n";
            }
        }

        return out.str();
    }

    bool MappingWriter::writeToFile(const MappingData& mappings, const std::string& filePath)
    {
        std::string content = generate(mappings);
        if (content.empty()) return false;

        std::ofstream ofs(filePath);
        if (!ofs) return false;

        ofs << content;
        return ofs.good();
    }
}
