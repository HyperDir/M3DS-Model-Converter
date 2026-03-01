#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <Mod3DS.hpp>

struct Wavefront {
    struct Material {
        std::string name {};
        std::array<float, 3> ambient  { 1.0f, 1.0f, 1.0f };
        std::array<float, 3> diffuse  { 0.8f, 0.8f, 0.8f };
        std::array<float, 3> specular { 0.5f, 0.5f, 0.5f };
        float highlights              { 250.0f }; // 0-1000 for some reason
        float opticalDensity          { 1.5f };
        float dissolve                { 1.0f };
        int illuminationModel         {};

        std::filesystem::path texture {};
    };
    struct Vertex {
        float x {};
        float y {};
        float z {};
        float w { 1.0 };
    };
    struct TexVertex {
        float u {};
        float v {};
        float w {};
    };
    struct VertexNormal {
        float x {};
        float y {};
        float z {};
    };
    // struct PolyLine {
    //     std::vector<int32_t> vertices {};
    // };
    // struct Curve {
    //     double u0 {};
    //     double u1 {};
    //     std::vector<int32_t> vertices {};
    // };
    using Triangle = std::array<std::tuple<Vertex, TexVertex, VertexNormal>, 3>;
    struct Surface {
        Material material {};
        std::vector<Triangle> triangles {};
    };

    // std::vector<PolyLine> polyLines {};
    // std::vector<Curve> curves {};

    std::vector<Surface> surfaces { Surface {} };

    std::filesystem::path path {};

    explicit Wavefront(std::filesystem::path path);
    
    Mod3DS toMod3ds() const;
};

