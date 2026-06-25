#include "InheritanceResolver.h"
#include <iostream>
#include <ranges>

namespace mcobfF
{
    InheritanceResolver::InheritanceResolver(MappingData& mappings, const ClassHierarchy& hierarchy)
        : mappings_(mappings), hierarchy_(hierarchy)
    {
    }

    void InheritanceResolver::resolveAll()
    {
        for (const auto& obfClassName : mappings_.obfClassIndex | std::views::keys)
        {
            resolveClass(obfClassName);
        }
    }

    void InheritanceResolver::resolveClass(const std::string& obfClassName)
    {
        if (resolved_.contains(obfClassName)) return;
        if (visiting_.contains(obfClassName)) return;

        visiting_.insert(obfClassName);

        auto hierarchyIt = hierarchy_.find(obfClassName);
        if (hierarchyIt != hierarchy_.end())
        {
            const auto& node = hierarchyIt->second;

            if (!node.superClass.empty())
            {
                resolveClass(node.superClass);
            }
            for (const auto& iface : node.interfaces)
            {
                resolveClass(iface);
            }

            auto myIndexIt = mappings_.obfClassIndex.find(obfClassName);
            if (myIndexIt != mappings_.obfClassIndex.end())
            {
                if (!node.superClass.empty())
                {
                    inheritFrom(obfClassName, node.superClass);
                }
                for (const auto& iface : node.interfaces)
                {
                    inheritFrom(obfClassName, iface);
                }
            }
        }

        visiting_.erase(obfClassName);
        resolved_.insert(obfClassName);
    }

    void InheritanceResolver::inheritFrom(const std::string& childObf, const std::string& parentObf) const
    {
        auto parentIndexIt = mappings_.obfClassIndex.find(parentObf);
        if (parentIndexIt == mappings_.obfClassIndex.end()) return;

        auto childIndexIt = mappings_.obfClassIndex.find(childObf);
        if (childIndexIt == mappings_.obfClassIndex.end()) return;

        const auto& parentEntry = mappings_.entries[parentIndexIt->second];
        auto& childEntry = mappings_.entries[childIndexIt->second];

        for (const auto& parentMethod : parentEntry.methods)
        {
            for (auto& childMethod : childEntry.methods)
            {
                if (childMethod.obfName != parentMethod.obfName ||
                    childMethod.jvmDescriptor != parentMethod.jvmDescriptor)
                    continue;

                if (!childMethod.intermediaryName.has_value() && parentMethod.intermediaryName.has_value())
                {
                    childMethod.intermediaryName = parentMethod.intermediaryName;
                }
                if (!childMethod.srgName.has_value() && parentMethod.srgName.has_value())
                {
                    childMethod.srgName = parentMethod.srgName;
                }
            }
        }

        for (const auto& parentField : parentEntry.fields)
        {
            for (auto& childField : childEntry.fields)
            {
                if (childField.obfName != parentField.obfName ||
                    childField.type != parentField.type)
                    continue;

                if (!childField.intermediaryName.has_value() && parentField.intermediaryName.has_value())
                {
                    childField.intermediaryName = parentField.intermediaryName;
                }
                if (!childField.srgName.has_value() && parentField.srgName.has_value())
                {
                    childField.srgName = parentField.srgName;
                }
            }
        }
    }
} // namespace mc