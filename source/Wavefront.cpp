#include <Wavefront.hpp>

Wavefront::Wavefront(std::filesystem::path p)
    : path(std::move(p))
{
    std::ifstream file { path };
    if (!file.is_open()) throw std::invalid_argument(std::format("Failed to open file '{}'", path.string()));

    size_t currentSurface {};

    std::vector<Vertex> vertices {};
    std::vector<TexVertex> texVertices {};
    std::vector<VertexNormal> normals {};

    std::string l {};
    while (std::getline(file, l)) {
        if (auto pos = l.find('#'); pos != std::string::npos) {
            l.resize(pos);
        }

        std::istringstream line { l };

        std::string prefix {};
        if (!(line >> prefix)) continue;

        if (prefix == "mtllib") {
            std::filesystem::path mtlPath {};
            line >> mtlPath;

            if (!mtlPath.has_root_directory()) mtlPath = path.parent_path() / mtlPath;
            std::ifstream mtlFile {mtlPath};

            if (!mtlFile) {
                std::cerr << "Unable to load mtllib " << mtlPath << std::endl;
                continue;
            }

            std::string mtlLine {};

            Material* currentMaterial {};

            while (std::getline(mtlFile, mtlLine)) {
                std::istringstream mtlLineStream { mtlLine };

                std::string mtlPrefix {};
                mtlLineStream >> mtlPrefix;

                if (mtlPrefix == "newmtl") {
                    currentMaterial = &surfaces.emplace_back().material;
                    mtlLineStream >> currentMaterial->name;
                    continue;
                }

                if (!currentMaterial) continue; // Rest of the operations require a material, quick and dirty impl

                const auto fillArr = [&](std::array<float, 3>& arr) {
                    mtlLineStream >> arr[0] >> arr[1] >> arr[2];
                };


                if (mtlPrefix == "Ka") fillArr(currentMaterial->ambient);
                else if (mtlPrefix == "Kd") fillArr(currentMaterial->diffuse);
                else if (mtlPrefix == "Ks") fillArr(currentMaterial->specular);
                else if (mtlPrefix == "Ns") mtlLineStream >> currentMaterial->highlights;
                else if (mtlPrefix == "Ni") mtlLineStream >> currentMaterial->opticalDensity;
                else if (mtlPrefix == "d") mtlLineStream >> currentMaterial->dissolve;
                else if (mtlPrefix == "map_Kd") mtlLineStream >> currentMaterial->texture;
            }
        } else if (prefix == "usemtl") {
            // currentMaterialIndex = materials.size();
            std::string mtlName {};
            line >> mtlName;

            currentSurface = std::ranges::find_if(surfaces, [&mtlName](const Surface& surface) { return surface.material.name == mtlName; }) - surfaces.begin();
            if (currentSurface == surfaces.size()) throw std::invalid_argument(std::format("Material '{}' not found!", mtlName));
        } else if (prefix == "v") {
            Vertex& v = vertices.emplace_back();
            line >> v.x >> v.y >> v.z >> v.w;
        } else if (prefix == "vt") {
            TexVertex& vt = texVertices.emplace_back();
            line >> vt.u >> vt.v >> vt.w;
        } else if (prefix == "vn") {
            VertexNormal& vn = normals.emplace_back();
            line >> vn.x >> vn.y >> vn.z;
        } else if (prefix == "f") {
            std::vector<std::tuple<Vertex, TexVertex, VertexNormal>> face {};
            // TODO: Handle material?

            for (std::string str; line >> str;) {
                auto& [vertex, texVertex, normalVertex] = face.emplace_back();

                char* begin = str.data();
                char* end = begin;
                while (*end >= '0' && *end <= '9') ++end;

                int32_t tmp {-1};
                std::from_chars(begin, end, tmp);

                if (tmp < 0) tmp += static_cast<int32_t>(vertices.size());
                else --tmp;

                if (tmp >= 0) vertex = vertices[tmp];

                if (*end != '/') continue;

                begin = end + 1;
                end = begin;
                while (*end >= '0' && *end <= '9') ++end;
                tmp = -1;
                std::from_chars(begin, end, tmp);

                if (tmp < 0) tmp += static_cast<int32_t>(texVertices.size());
                else --tmp;
                if (tmp >= 0) texVertex = texVertices[tmp];

                if (*end != '/') continue;

                begin = end + 1;
                end = begin;
                while (*end >= '0' && *end <= '9') ++end;
                tmp = -1;
                std::from_chars(begin, end, tmp);

                if (tmp < 0) tmp += static_cast<int32_t>(normals.size());
                else --tmp;
                if (tmp >= 0) normalVertex = normals[tmp];
            }

            if (face.size() < 3) throw std::invalid_argument(std::format("Face not found!"));

            for (size_t i{}; i<face.size() - 2; ++i) {
                surfaces[currentSurface].triangles.emplace_back(Triangle {face[0], face[i + 1], face[i + 2]});
            }
        }
    }

    std::erase_if(surfaces, [](const Surface& surface) { return surface.triangles.empty(); });
}

Mod3DS Wavefront::toMod3ds() const {
    Mod3DS model {};
        
    auto normalisePath = [&](const std::filesystem::path& path) {
        return path.is_absolute() ? path : std::filesystem::path(path).parent_path() / path;
    };

    for (const auto& [material, triangles] : surfaces) {
        if (!material.texture.empty()) {
            const std::filesystem::path& texPath = normalisePath(material.texture);
            if (!std::ranges::contains(
                model.mTextures,
                true,
                [&texPath](const Mod3DS::T3X& tex) { return texPath == tex.mPath; }
            )) {
                model.mTextures.emplace_back(texPath);
            }
        }
    }

    for (const auto& [material, triangles] : surfaces) {
        Mod3DS::Surface& surface = model.mSurfaces.emplace_back();

        surface.material = {
            .properties = {
                .ambient    { material.diffuse[2] / 4, material.diffuse[1] / 4, material.diffuse[0] / 4 },
                .diffuse    { material.diffuse[2], material.diffuse[1], material.diffuse[0] },
                .specular0  { material.specular[2], material.specular[1], material.specular[0] },
                .specular1  { material.specular[2], material.specular[1], material.specular[0] },
            },
            .textureIndex = [&textures=model.mTextures, &material, &normalisePath] -> std::optional<std::uint16_t> {
                const auto it = std::ranges::find_if(textures, [path=normalisePath(material.texture)](const Mod3DS::T3X& tex) { return path == tex.mPath; });
                if (it == textures.end()) return {};
                return static_cast<std::uint16_t>(it - textures.begin());
            }()
        };

        surface.triangles.reserve(triangles.size());
        for (const auto& triangle : triangles) {
            auto vertex = [](const std::tuple<Wavefront::Vertex, Wavefront::TexVertex, Wavefront::VertexNormal>& tuple) {
                const auto& [v, t, n] = tuple;
                Mod3DS::Vertex vtx {};

                vtx.coords.x = v.x;
                vtx.coords.y = v.y;
                vtx.coords.z = v.z;

                vtx.texCoords.x = t.u;
                vtx.texCoords.y = t.v;
                while (vtx.texCoords.x > 1) --vtx.texCoords.x;
                while (vtx.texCoords.y > 1) --vtx.texCoords.y;

                vtx.normal.x = n.x;
                vtx.normal.y = n.y;
                vtx.normal.z = n.z;

                return vtx;
            };
            surface.triangles.emplace_back(Mod3DS::Triangle {vertex(triangle[0]), vertex(triangle[1]), vertex(triangle[2])});
        }
    }

    return model;
}
