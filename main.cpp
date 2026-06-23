#include "JarDumper.h"
#include "mcobfF/network/VersionDownloader.h"
#include <iostream>

namespace {
    void printUsage(const char* programName) {
        std::cerr << "Usage:" << std::endl;
        std::cerr << "  " << programName << " <client.jar> <version> <output.json>" << std::endl;
        std::cerr << "  " << programName << " --version <version> <output.json>" << std::endl;
        std::cerr << "  " << programName << " --latest-release <output.json>" << std::endl;
        std::cerr << "  " << programName << " --latest-snapshot <output.json>" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--version" || arg1 == "-v") {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " --version <version> <output.json>" << std::endl;
            return 1;
        }
        std::string version = argv[2];
        std::string outputPath = argv[3];
        if (!mcobfF::JarDumper::dumpFromVersion(version, outputPath)) {
            return 1;
        }
        std::cout << "Successfully dumped mappings to " << outputPath << std::endl;
        return 0;
    }

    if (arg1 == "--latest-release" || arg1 == "-r") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " --latest-release <output.json>" << std::endl;
            return 1;
        }
        auto version = mcobfF::VersionDownloader::getLatestRelease();
        if (!version) {
            std::cerr << "Failed to determine latest release version." << std::endl;
            return 1;
        }
        std::cout << "[Main] Latest release: " << *version << std::endl;
        std::string outputPath = argv[2];
        if (!mcobfF::JarDumper::dumpFromVersion(*version, outputPath)) {
            return 1;
        }
        std::cout << "Successfully dumped mappings to " << outputPath << std::endl;
        return 0;
    }

    if (arg1 == "--latest-snapshot" || arg1 == "-s") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " --latest-snapshot <output.json>" << std::endl;
            return 1;
        }
        auto version = mcobfF::VersionDownloader::getLatestSnapshot();
        if (!version) {
            std::cerr << "Failed to determine latest snapshot version." << std::endl;
            return 1;
        }
        std::cout << "[Main] Latest snapshot: " << *version << std::endl;
        std::string outputPath = argv[2];
        if (!mcobfF::JarDumper::dumpFromVersion(*version, outputPath)) {
            return 1;
        }
        std::cout << "Successfully dumped mappings to " << outputPath << std::endl;
        return 0;
    }

    //Legacy mode: direct jar path
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string jarPath = arg1;
    std::string version = argv[2];
    std::string outputPath = argv[3];

    mcobfF::JarDumper dumper;
    if (!dumper.dump(jarPath, version, outputPath)) {
        std::cerr << "Failed to dump mappings." << std::endl;
        return 1;
    }

    std::cout << "Successfully dumped mappings to " << outputPath << std::endl;
    return 0;
}
