#include <iostream>
#include <fstream>
#include <format>
#include <filesystem>

#include "Wavefront.hpp"
#include "GLTF.hpp"
#include "Mod3DS.hpp"

int main(const int argc, const char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./M3DSModelConverter <Input File> <Output File>" << std::endl;
        return 1;
    }

    std::filesystem::path fileName = argv[1];
    std::filesystem::path outPath = argc > 2 ? argv[2] : std::filesystem::path { fileName }.replace_extension(Mod3DS::extension);
    const std::filesystem::path fileExtension = fileName.extension();

    try {
        if (fileName.extension() == ".obj") {
            std::cout << "Loading Wavefront " << fileName << "...\n";

            Wavefront{ fileName }.toMod3ds().save(outPath);
        } else if (fileName.extension() == ".glb") {
            std::cout << "Loading GLB " << fileName << "...\n";

            GLTF{ fileName }.toMod3ds().save(outPath);
        } else {
            throw std::runtime_error{std::format("Unknown file type! ({})", fileExtension.string())};
        }
    } catch (const std::exception& e) {
        std::cerr << std::format("Conversion failed! {}", e.what()) << std::endl;
    }
}
