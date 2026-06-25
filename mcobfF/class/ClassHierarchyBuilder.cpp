#include "ClassHierarchyBuilder.h"
#include "ClassFileParser.h"
#include <iostream>
#include <span>

namespace mcobfF
{
    ClassHierarchy ClassHierarchyBuilder::buildFromJar(ZipArchive& zip)
    {
        ClassHierarchy hierarchy;
        mz_uint fileCount = zip.getFileCount();

        for (mz_uint i = 0; i < fileCount; ++i)
        {
            mz_zip_archive_file_stat stat{};
            if (!zip.getFileStat(i, stat)) continue;

            std::string filename = stat.m_filename;
            if (filename.size() < 7 || filename.substr(filename.size() - 6) != ".class")
            {
                continue;
            }

            auto dataOpt = zip.extractToMemory(i);
            if (!dataOpt) continue;

            ClassInfo info;
            if (ClassFileParser::parse(
                std::span(reinterpret_cast<const uint8_t*>(dataOpt->data()), dataOpt->size()),
                info) && !info.name.empty())
            {
                ClassNode node;
                if (!info.superName.empty() && info.superName != "java/lang/Object")
                {
                    node.superClass = info.superName;
                }
                node.interfaces = info.interfaces;
                hierarchy[info.name] = std::move(node);
            }
        }
        return hierarchy;
    }
} // namespace mc