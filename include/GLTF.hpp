#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <array>

#include <format>
#include <iostream>
#include <optional>

#include <nlohmann-json.hpp>

#include <Mod3DS.hpp>
#include <HeapArray.hpp>

template <typename T>
void from_json(const nlohmann::json& json, HeapArray<T>& heapArray) {
    if (!json.is_array()) throw std::invalid_argument("json: HeapArray expects an array!");

    size_t size = json.size();
    HeapArray<T> tmp(size);
    for (size_t i = 0; i < size; ++i) {
        tmp[i] = json[i];
    }
    heapArray = std::move(tmp);
}

class GLTF {
public:
    struct LoadException final : std::runtime_error {
        explicit LoadException(const auto& x) : std::runtime_error(x) {}
    };
    struct ActiveException final : std::runtime_error {
        explicit ActiveException(const auto& x) : std::runtime_error(x) {}
    };

    struct Vector2 { float x, y; };
    struct Vector3 { float x, y, z; };
    struct Vector4 { float x, y, z, w; };
    using Mat2 = std::array<float, 4>;
    using Mat3 = std::array<float, 9>;
    using Mat4 = std::array<float, 16>;

    using JSON = nlohmann::json;

    struct Accessor {
        enum class ComponentType {
            eByte               = 5120,
            eUnsignedByte       = 5121,
            eShort              = 5122,
            eUnsignedShort      = 5123,
            eInt                = 5124,
            eUnsignedInt        = 5125,
            eFloat              = 5126
        };

        enum class Type {
            scalar,
            vec2,
            vec3,
            vec4,
            mat2,
            mat3,
            mat4
        };

        static constexpr std::array<std::string_view, 7> typeNames {
            "SCALAR",
            "VEC2",
            "VEC3",
            "VEC4",
            "MAT2",
            "MAT3",
            "MAT4"
        };

        struct Sparse {
            struct Indices {
                enum class ComponentType {
                    e_unsigned_byte = 5121,
                    e_unsigned_short = 5123,
                    e_unsigned_int = 5125
                };

                size_t bufferView {};
                size_t byteOffset {};
                ComponentType componentType {};
                std::vector<JSON> extensions {};
                JSON extras {};

                friend void from_json(const JSON& json, Indices& i) {
                    struct Included {
                        bool bufferView : 1 {};
                        bool componentType : 1 {};
                    } included {};

                    for (const auto& [key, value] : json.items()) {
                        if (key == "bufferView") {
                            included.bufferView = true;
                            i.bufferView = value;
                        } else if (key == "componentType") {
                            included.componentType = true;
                            i.componentType = value;

                            if (
                                i.componentType != ComponentType::e_unsigned_byte &&
                                i.componentType != ComponentType::e_unsigned_short &&
                                i.componentType != ComponentType::e_unsigned_int
                            ) {
                                throw LoadException(std::format("Invalid componentType on Accessor Sparse Indices"));
                            }
                        }
                        else if (key == "byteOffset") i.byteOffset = value;
                        else if (key == "extensions") i.extensions = value;
                        else if (key == "extras") i.extras = value;
                    }

                    if (!included.bufferView) throw LoadException("Accessor Sparce Indices missing bufferView!");
                    if (!included.componentType) throw LoadException("Accessor Sparce Indices missing componentType!");
                }
            };

            struct Values {
                size_t bufferView {};
                size_t byteOffset {};
                std::vector<JSON> extensions {};
                JSON extras {};

                friend void from_json(const JSON& json, Values& i) {
                    struct Included {
                        bool bufferView : 1 {};
                    } included {};

                    for (const auto& [key, value] : json.items()) {
                        if (key == "bufferView") {
                            included.bufferView = true;
                            i.bufferView = value;
                        }
                        else if (key == "byteOffset") i.byteOffset = value;
                        else if (key == "extensions") i.extensions = value;
                        else if (key == "extras") i.extras = value;
                    }

                    if (!included.bufferView) throw LoadException("Accessor Sparce Values missing bufferView!");
                }
            };

            size_t count {};
            Indices indices {};
            Values values {};
            std::vector<JSON> extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, Sparse& s) {
                struct Included {
                    bool count : 1 {};
                    bool indices : 1 {};
                    bool values : 1 {};
                } included {};

                for (const auto& [key, value]: json.items()) {
                    if (key == "count") {
                        included.count = true;
                        s.count = value;
                    } else if (key == "indices") {
                        included.indices = true;
                        s.indices = value;
                    } else if (key == "values") {
                        included.values = true;
                        s.values = value;
                    }
                    else if (key == "extensions") s.extensions = value;
                    else if (key == "extras") s.extras = value;
                }

                if (!included.count) throw LoadException("Accessor Sparce missing count!");
                if (!included.indices) throw LoadException("Accessor Sparce missing indices!");
                if (!included.values) throw LoadException("Accessor Sparce missing values!");
            }
        };

        template <typename T>
        struct MinMax {
            T min {}, max {};
        };

        std::optional<size_t> bufferView {};
        size_t byteOffset {}; // MUST be multiple of size of component datatype; must not be defined when bufferView is undefined
        ComponentType componentType {};
        bool normalized {}; // true for unsigned [0, 1] or signed [-1, 1]; MUST NOT be true for FLOAT or UNSIGNED_INT type
        size_t count {}; // >= 1
        Type type {};
        std::variant<
            MinMax<float>,
            MinMax<Vector2>,
            MinMax<Vector3>,
            MinMax<Vector4>,
            MinMax<Mat2>,
            MinMax<Mat3>,
            MinMax<Mat4>
        > minMax {};
        std::optional<Sparse> sparse {};
        std::string name {};
        JSON extensions {};
        JSON extras {}; // Likely JSON

        friend void from_json(const JSON& json, Accessor& accessor) {
            struct Included {
                bool componentType : 1 {};
                bool count : 1 {};
                bool type : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "bufferView") accessor.bufferView = value;
                else if (key == "byteOffset") accessor.byteOffset = value;
                else if (key == "componentType") {
                    included.componentType = true;
                    accessor.componentType = value;
                }
                else if (key == "normalized") accessor.normalized = value;
                else if (key == "count") {
                    included.count = true;
                    accessor.count = value;
                }
                else if (key == "type") {
                    included.type = true;
                    std::string tmp = value;

                    const auto iter = std::ranges::find(typeNames, tmp);
                    if (iter == typeNames.end()) throw LoadException(std::format("Invalid type ({})!", tmp));
                    accessor.type = static_cast<Type>(std::distance(typeNames.begin(), iter));

                    const auto min = json.find("min");
                    const auto max = json.find("max");

                    const bool defined = min != json.end();

                    if (defined != (max != json.end())) throw LoadException("Min and Max must both be defined or undefined!");
                    if (defined) {
                        HeapArray<float> minValues = *min;
                        HeapArray<float> maxValues = *max;
                        if (minValues.size() != maxValues.size()) throw LoadException("Min and Max must both be the same size!");

                        if (accessor.type == Type::scalar) {
                            if (minValues.size() != 1) throw LoadException("Min and Max must both be of size 1!");
                            accessor.minMax = MinMax(minValues[0], maxValues[0]);
                        } else if (accessor.type == Type::vec2) {
                            if (minValues.size() != 2) throw LoadException("Min and Max must both be of size 2!");
                            accessor.minMax = MinMax<Vector2>(
                                {minValues[0], minValues[1]},
                                {maxValues[0], maxValues[1]}
                            );
                        } else if (accessor.type == Type::vec3) {
                            if (minValues.size() != 3) throw LoadException("Min and Max must both be of size 3!");
                            accessor.minMax = MinMax<Vector3>(
                                {minValues[0], minValues[1], minValues[2]},
                                {maxValues[0], maxValues[1], maxValues[2]}
                            );
                        } else if (accessor.type == Type::vec4) {
                            if (minValues.size() != 4) throw LoadException("Min and Max must both be of size 4!");
                            accessor.minMax = MinMax<Vector4>(
                                {minValues[0], minValues[1], minValues[2], minValues[3]},
                                {maxValues[0], maxValues[1], maxValues[2], maxValues[3]}
                            );
                        } else if (accessor.type == Type::mat2) {
                            if (minValues.size() != 4) throw LoadException("Min and Max must both be of size 4!");
                            accessor.minMax = MinMax<Mat2>(
                                {{minValues[0], minValues[1], minValues[2], minValues[3]}},
                                {{maxValues[0], maxValues[1], maxValues[2], maxValues[3]}}
                            );
                        } else if (accessor.type == Type::mat3) {
                            if (minValues.size() != 9) throw LoadException("Min and Max must both be of size 9!");
                            accessor.minMax = MinMax<Mat3>(
                                {{minValues[0], minValues[1], minValues[2], minValues[3], minValues[4], minValues[5], minValues[6], minValues[7], minValues[8]}},
                                {{maxValues[0], maxValues[1], maxValues[2], maxValues[3], maxValues[4], maxValues[5], maxValues[6], maxValues[7], maxValues[8]}}
                            );
                        }  else if (accessor.type == Type::mat4) {
                            if (minValues.size() != 16) throw LoadException("Min and Max must both be of size 16!");
                            auto mat = MinMax<Mat4>();
                            std::ranges::copy(minValues, mat.min.begin());
                            std::ranges::copy(maxValues, mat.max.begin());
                            accessor.minMax = mat;
                        }
                    }

                }
                else if (key == "sparse") accessor.sparse = value;
                else if (key == "name") accessor.name = value;
                else if (key == "extensions") accessor.extensions = value;
                else if (key == "extras") accessor.extras = value;
            }

            if (!included.componentType) throw LoadException("Accessor missing componentType!");
            if (!included.count) throw LoadException("Accessor missing count!");
            if (!included.type) throw LoadException("Accessor missing type!");
        }
    };

    struct Animation {
        struct Channel {
            struct Target {
                size_t node {};
                std::string path {};
                JSON extensions {};
                JSON extras {};

                friend void from_json(const JSON& json, Target& t) {
                    bool pathIncluded {};

                    for (const auto& [key, value] : json.items()) {
                        if (key == "node") t.node = value;
                        else if (key == "path") {
                            pathIncluded = true;
                            t.path = value;
                        }
                        else if (key == "extensions") t.extensions = value;
                        else if (key == "extras") t.extras = value;
                    }

                    if (!pathIncluded) throw LoadException("Animation path not defined!");
                }
            };

            size_t sampler {};
            Target target {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, Channel& channel) {
                struct Included {
                    bool sampler : 1 {};
                    bool target  : 1 {};
                } included {};

                for (const auto& [key, value] : json.items()) {
                    if (key == "sampler") {
                        included.sampler = true;
                        channel.sampler = value;
                    } else if (key == "target") {
                        included.target = true;
                        channel.target = value;
                    }
                    else if (key == "extensions") channel.extensions = value;
                    else if (key == "extras") channel.extras = value;
                }

                if (!included.sampler) throw LoadException("Animation Channel Sampler not defined!");
                if (!included.target) throw LoadException("Animation Channel Target not defined!");
            }
        };

        struct AnimationSampler {
            enum class Interpolation : std::uint8_t {
                linear,
                step,
                cubic_spline
            };

            static constexpr std::array<std::string_view, 3> interpolationNames = {
                "LINEAR",
                "STEP",
                "CUBICSPLINE"
            };

            size_t input {};
            Interpolation interpolation {};
            size_t output {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, AnimationSampler& sampler) {
                struct Included {
                    bool input  : 1 {};
                    bool output : 1 {};
                } included;

                for (const auto& [key, value] : json.items()) {
                    if (key == "input") {
                        included.input = true;
                        sampler.input = value;
                    } else if (key == "interpolation") {
                        std::string tmp = value;
                        const auto it = std::ranges::find(interpolationNames, tmp);
                        if (it == interpolationNames.end())
                            throw LoadException(std::format("Sampler input ({}) missing configuration!", tmp));

                        sampler.interpolation = static_cast<Interpolation>(std::distance(interpolationNames.begin(), it));
                    } else if (key == "output") {
                        included.output = true;
                        sampler.output = value;
                    }
                    else if (key == "extensions") sampler.extensions = value;
                    else if (key == "extras") sampler.extras = value;
                }

                if (!included.input) throw LoadException("Sampler input not defined!");
                if (!included.output) throw LoadException("Sampler output not defined!");
            }
        };

        HeapArray<Channel> channels {};
        HeapArray<AnimationSampler> samplers {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Animation& animation) {
            struct Included {
                bool channels : 1 {};
                bool samplers : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "channels") {
                    included.channels = true;
                    animation.channels = value;
                }
                else if (key == "samplers") {
                    included.samplers = true;
                    animation.samplers = value;
                }
                else if (key == "name") animation.name = value;
                else if (key == "extensions") animation.extensions = value;
                else if (key == "extras") animation.extras = value;
            }

            if (!included.channels) throw LoadException("Animation Channel not defined!");
            if (!included.samplers) throw LoadException("Animation Sampler not defined!");
        }
    };

    struct Asset {
        struct Version {
            unsigned int major {};
            unsigned int minor {};

            Version() = default;
            explicit Version(std::string_view version) {
                const auto pos = version.find('.');
                if (pos == std::string_view::npos)
                    throw LoadException(std::format("Invalid asset version: {}", version));

                if (
                    std::from_chars(version.begin(), version.begin() + pos, major).ec != std::errc {} ||
                    std::from_chars(version.begin() + pos + 1, version.end(), minor).ec != std::errc {}
                )
                    throw LoadException(std::format("Invalid asset version: {}", version));
            }

            friend auto operator<=>(const Version& lhs, const Version& rhs) {
                if (lhs.major == rhs.major) {
                    return lhs.minor <=> rhs.minor;
                }
                return lhs.major <=> rhs.major;
            }
        };

        std::string copyright {};
        std::string generator {};
        Version version {};
        Version minVersion {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Asset& a) {
            struct Included {
                bool version : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "copyright") a.copyright = value;
                else if (key == "generator") a.generator = value;
                else if (key == "version") {
                    included.version = true;
                    a.version = Version(value.get<std::string_view>());
                }
                else if (key == "minVersion") a.minVersion = Version(value.get<std::string_view>());
                else if (key == "extensions") a.extensions = value;
                else if (key == "extras") a.extras = value;
            }

            if (!included.version) throw LoadException("Asset version not defined!");
            if (a.minVersion > a.version) throw LoadException("Asset version less than minimum version!");
        }
    };

    struct Buffer {
        std::string uri {};
        size_t byteLength {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Buffer& buffer) {
            struct Included {
                bool byteLength : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "uri") buffer.uri = value;
                else if (key == "byteLength") {
                    included.byteLength = true;
                    buffer.byteLength = value;
                }
                else if (key == "name") buffer.name = value;
                else if (key == "extensions") buffer.extensions = value;
                else if (key == "extras") buffer.extras = value;
            }

            if (!included.byteLength) throw LoadException("Buffer byteLength not defined!");
        }
    };

    struct BufferView {
        enum class Target {
            array_buffer = 34962,
            element_array_buffer = 34963
        };

        size_t buffer {};
        size_t byteOffset {};
        size_t byteLength {};
        std::optional<size_t> byteStride {};
        std::optional<Target> target {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, BufferView& bufferView) {
            struct Included {
                bool byteLength : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "buffer") bufferView.buffer = value;
                else if (key == "byteOffset") bufferView.byteOffset = value;
                else if (key == "byteLength") {
                    included.byteLength = true;
                    bufferView.byteLength = value;
                    if (bufferView.byteLength == 0)
                        throw LoadException("BufferView byteLength must be >= 1!");
                }
                else if (key == "byteStride") {
                    bufferView.byteStride = value;
                    if (bufferView.byteStride < 4 || bufferView.byteStride > 252)
                        throw LoadException("BufferView stride must be within range [4, 252]!");
                }
                else if (key == "target") {
                    bufferView.target = value;
                    if (bufferView.target != Target::array_buffer && bufferView.target != Target::element_array_buffer)
                        throw LoadException("invalid BufferView Target!");
                }
                else if (key == "name") bufferView.name = value;
                else if (key == "extensions") bufferView.extensions = value;
                else if (key == "extras") bufferView.extras = value;
            }

            if (!included.byteLength) throw LoadException("BufferView byteLength is undefined!");
        }
    };

    struct Camera {
        struct Orthographic {
            float xmag {};
            float ymag {};
            float zfar {};
            float znear {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, Orthographic& orthographic) {
                struct Included {
                    bool xmag : 1 {};
                    bool ymag : 1 {};
                    bool zfar : 1 {};
                    bool znear : 1 {};
                } included {};

                for (const auto& [key, value] : json.items()) {
                    if (key == "xmag") {
                        included.xmag = true;
                        orthographic.xmag = value;
                    }
                    else if (key == "ymag") {
                        included.ymag = true;
                        orthographic.ymag = value;
                    }
                    else if (key == "zfar") {
                        included.zfar = true;
                        orthographic.zfar = value;
                    }
                    else if (key == "znear") {
                        included.znear = true;
                        orthographic.znear = value;
                    }
                    else if (key == "extensions") orthographic.extensions = value;
                    else if (key == "extras") orthographic.extras = value;
                }

                if (!included.xmag) throw LoadException("Camera Orthographic xmag not defined!");
                if (!included.ymag) throw LoadException("Camera Orthographic ymag not defined!");
                if (!included.zfar) throw LoadException("Camera Orthographic zfar not defined!");
                if (!included.znear) throw LoadException("Camera Orthographic znear not defined!");
            }
        };

        struct Perspective {
            std::optional<float> aspectRatio {};
            std::optional<float> zfar {};

            float yfov {};
            float znear {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, Perspective& perspective) {
                struct Included {
                    bool yfov : 1 {};
                    bool znear : 1 {};
                } included {};

                for (const auto& [key, value] : json.items()) {
                    if (key == "aspectRatio") perspective.aspectRatio = value;
                    else if (key == "yfov") {
                        included.yfov = true;
                        perspective.yfov = value;
                    }
                    else if (key == "zfar") perspective.zfar = value;
                    else if (key == "znear") {
                        included.znear = true;
                        perspective.znear = value;
                    }
                    else if (key == "extensions") perspective.extensions = value;
                    else if (key == "extras") perspective.extras = value;
                }

                if (!included.yfov) throw LoadException("Camera Perspective yfov not defined!");
                if (!included.znear) throw LoadException("Camera Perspective znear not defined!");
            }
        };

        std::variant<Orthographic, Perspective> projection {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Camera& camera) {
            struct Included {
                bool type : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "type") {
                    included.type = true;
                    std::string tmp = value;
                    const auto it = json.find(tmp);

                    if (tmp == "orthographic") camera.projection = Orthographic(*it);
                    else if (tmp == "perspective") camera.projection = Perspective(*it);
                    else throw LoadException(std::format("Invalid Camera type ({})!", tmp));

                    if (it == json.end())
                        throw LoadException(std::format("Camera type ({}) missing configuration!", tmp));
                }
                else if (key == "name") camera.name = value;
                else if (key == "extensions") camera.extensions = value;
                else if (key == "extras") camera.extras = value;
            }

            if (!included.type) throw LoadException("Camera type not defined!");
        }
    };

    struct Image {
        enum class MimeType : uint8_t {
            jpeg,
            png
        };

        struct URI {
            std::string uri {};
        };
        struct BufferView {
            MimeType mimeType {};
            size_t bufferView {};
        };

        std::variant<URI, BufferView> data {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Image& image) {
            struct Included {
                bool uri : 1 {};
                bool bufferView : 1{};
                bool mimeType : 1 {};
            } included {};

            std::string uri {};
            BufferView bufferView {};
            
            for (const auto& [key, value] : json.items()) {
                if (key == "uri") {
                    included.uri = true;
                    uri = value;
                } else if (key == "mimeType") {
                    included.mimeType = true;

                    std::string tmp = value;
                    if (value == "image/jpeg") bufferView.mimeType = MimeType::jpeg;
                    else if (value == "image/png") bufferView.mimeType = MimeType::png;
                    else throw LoadException(std::format("Invalid mime type ({})!", tmp));
                } else if (key == "bufferView") {
                    included.bufferView = true;
                    bufferView.bufferView = value;
                }
                else if (key == "name") image.name = value;
                else if (key == "extensions") image.extensions = value;
                else if (key == "extras") image.extras = value;
            }

            if (included.uri) {
                if (included.bufferView || included.mimeType)
                    throw LoadException("Image erroneously defined as both URI and bufferView!");

                image.data = URI(std::move(uri));
            } else if (included.bufferView && included.mimeType) {
                image.data = bufferView;
            } else {
                throw LoadException("Image bufferView and mimeType must BOTH be defined (or uri alone)!");
            }
        }
    };

    struct Material {
        enum class AlphaMode {
            opaque,
            mask,
            blend
        };

        struct TextureInfo {
            size_t index {};
            size_t texCoord {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, TextureInfo& textureInfo) {
                for (const auto& [key, value] : json.items()) {
                    if (key == "index") textureInfo.index = value;
                    else if (key == "texCoord") textureInfo.texCoord = value;
                    else if (key == "extensions") textureInfo.extensions = value;
                    else if (key == "extras") textureInfo.extras = value;
                }
            }
        };

        struct NormalTextureInfo : TextureInfo {
            float scale = 1;

            friend void from_json(const JSON& json, NormalTextureInfo& normal) {
                from_json(json, static_cast<TextureInfo&>(normal));
                for (const auto& [key, value] : json.items()) {
                    if (key == "scale") normal.scale = value;
                }
            }
        };

        struct OcclusionTextureInfo : TextureInfo {
            float strength = 1;

            friend void from_json(const JSON& json, OcclusionTextureInfo& occlusion) {
                from_json(json, static_cast<TextureInfo&>(occlusion));
                for (const auto& [key, value] : json.items()) {
                    if (key == "strength") occlusion.strength = value;
                }
            }
        };

        struct PBRMetallicRoughness {
            std::array<float, 4> baseColorFactor { 1, 1, 1, 1 };
            std::optional<TextureInfo> baseColorTexture {};
            float metallicFactor = 1;
            float roughnessFactor = 1;
            std::optional<TextureInfo> metallicRoughnessTexture {};
            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, PBRMetallicRoughness& metallicRoughness) {
                for (const auto& [key, value] : json.items()) {
                    if (key == "baseColorFactor") metallicRoughness.baseColorFactor = value;
                    else if (key == "baseColorTexture") metallicRoughness.baseColorTexture = value;
                    else if (key == "metallicFactor") metallicRoughness.metallicFactor = value;
                    else if (key == "roughnessFactor") metallicRoughness.roughnessFactor = value;
                    else if (key == "metallicRoughnessTexture") metallicRoughness.metallicRoughnessTexture = value;
                    else if (key == "extensions") metallicRoughness.extensions = value;
                    else if (key == "extras") metallicRoughness.extras = value;
                }
            }
        };

        std::optional<PBRMetallicRoughness> pbrMetallicRoughness {};
        std::optional<NormalTextureInfo> normalTexture {};
        std::optional<OcclusionTextureInfo> occlusionTexture {};
        std::optional<TextureInfo> emissiveTexture {};

        std::array<float, 3> emissiveFactor {};
        AlphaMode alphaMode = AlphaMode::opaque;
        float alphaCutoff = 0.5;
        bool doubleSided {};

        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Material& material) {
            for (const auto& [key, value] : json.items()) {
                if (key == "pbrMetallicRoughness") material.pbrMetallicRoughness = value;
                else if (key == "normalTexture") material.normalTexture = value;
                else if (key == "occlusionTexture") material.occlusionTexture = value;
                else if (key == "emissiveTexture") material.emissiveTexture = value;
                else if (key == "emissiveFactor") material.emissiveFactor = value;
                else if (key == "alphaMode") {
                    std::string tmp = value;

                    if (value == "OPAQUE") material.alphaMode = AlphaMode::opaque;
                    else if (value == "MASK") material.alphaMode = AlphaMode::mask;
                    else if (value == "BLEND") material.alphaMode = AlphaMode::blend;
                    else throw LoadException(std::format("Invalid alphaMode ({})!", tmp));
                }
                else if (key == "alphaCutoff") material.alphaCutoff = value;
                else if (key == "doubleSided") material.doubleSided = value;
                else if (key == "name") material.name = value;
                else if (key == "extensions") material.extensions = value;
                else if (key == "extras") material.extras = value;

            }
        }
    };

    struct Mesh {
        struct Primitive {
            enum class Mode : uint8_t {
                points,
                lines,
                line_loop,
                line_strip,
                triangles,
                triangle_strip,
                triangle_fan
            };

            static constexpr std::array<std::string_view, 7> modeNames {
                "POINTS",
                "LINES",
                "LINE_LOOP",
                "LINE_STRIP",
                "TRIANGLES",
                "TRIANGLE_STRIP",
                "TRIANGLE_FAN"
            };

            std::optional<size_t> indices {};
            std::optional<size_t> material {};
            std::optional<HeapArray<JSON>> targets {};

            JSON attributes {};
            Mode mode { Mode::triangles };

            JSON extensions {};
            JSON extras {};

            friend void from_json(const JSON& json, Primitive& primitive) {
                struct Required {
                    bool attributes : 1 {};
                } included {};

                for (const auto& [key, value] : json.items()) {
                    if (key == "attributes") {
                        included.attributes = true;
                        primitive.attributes = value;
                    }
                    else if (key == "indices") primitive.indices = value;
                    else if (key == "material") primitive.material = value;
                    else if (key == "mode") primitive.mode = value;
                    else if (key == "targets") primitive.targets = value;
                    else if (key == "extensions") primitive.extensions = value;
                    else if (key == "extras") primitive.extras = value;
                }

                if (!included.attributes) throw LoadException("Mesh Primitive has undefined attributes!");
            }
        };

        HeapArray<Primitive> primitives {};
        std::optional<HeapArray<float>> weights {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Mesh& mesh) {
            struct Required {
                bool primitives : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "primitives") {
                    included.primitives = true;
                    mesh.primitives = value;
                }
                else if (key == "weights") mesh.weights = value;
                else if (key == "name") mesh.name = value;
                else if (key == "extensions") mesh.extensions = value;
                else if (key == "extras") mesh.extras = value;
            }

            if (!included.primitives) throw LoadException("Mesh has undefined primitives!");
        }
    };

    struct Node {
        std::optional<size_t> camera {};
        std::optional<size_t> skin {};
        std::optional<size_t> mesh {};

        HeapArray<size_t> children {};
        HeapArray<float> weights {};

        std::array<float, 16> matrix {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        }; // column-major order

        std::array<float, 4> rotation { 0, 0, 0, 1 };
        std::array<float, 3> scale { 1, 1, 1 };
        std::array<float, 3> translation = { 0, 0, 0 };
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Node& node) {
            struct Included {
                bool weights : 1 {};
            } included {};

            for (const auto& [key, value] : json.items()) {
                if (key == "weights") {
                    included.weights = true;
                    node.weights = value;;
                }
                else if (key == "camera") node.camera = value;
                else if (key == "skin") node.skin = value;
                else if (key == "mesh") node.mesh = value;
                else if (key == "children") node.children = value;
                else if (key == "matrix") node.matrix = value;
                else if (key == "rotation") node.rotation = value;
                else if (key == "scale") node.scale = value;
                else if (key == "translation") node.translation = value;
                else if (key == "name") node.name = value;
                else if (key == "extensions") node.extensions = value;
                else if (key == "extras") node.extras = value;
            }

            if (included.weights && !node.mesh.has_value())
                throw LoadException("Node Weights defined but Mesh undefined!");
        }
    };

    struct Sampler {
        enum class WrappingMode {
            clamp_to_edge = 33071,
            mirrored_repeat = 33648,
            repeat = 10497
        };

        enum class MagnificationFilter {
            nearest = 9728,
            linear = 9729
        };

        enum class MinificationFilter {
            nearest = 9728,
            linear = 9729,
            nearest_mipmap_nearest = 9984,
            linear_mipmap_nearest = 9985,
            nearest_mipmap_linear = 9986,
            linear_mipmap_linear = 9987,
        };

        std::optional<MagnificationFilter> magFilter {};
        std::optional<MinificationFilter> minFilter {};
        WrappingMode wrapS { WrappingMode::repeat };
        WrappingMode wrapT { WrappingMode::repeat };
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Sampler& sampler) {
            for (const auto& [key, value] : json.items()) {
                if (key == "magFilter") sampler.magFilter = value;
                else if (key == "minFilter") sampler.minFilter = value;
                else if (key == "wrapS") sampler.wrapS = value;
                else if (key == "wrapT") sampler.wrapT = value;
                else if (key == "name") sampler.name = value;
                else if (key == "extensions") sampler.extensions = value;
                else if (key == "extras") sampler.extras = value;
            }
        }
    };

    struct Scene {
        std::optional<HeapArray<size_t>> nodes {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Scene& scn) {
            for (const auto& [key, value] : json.items()) {
                if (key == "nodes") scn.nodes = value;
                else if (key == "name") scn.name = value;
                else if (key == "extensions") scn.extensions = value;
                else if (key == "extras") scn.extras = value;
            }
        }
    };

    struct Skin {
        std::optional<size_t> inverseBindMatrices {}; // Accessor index
        std::optional<size_t> skeleton {}; // Node index
        HeapArray<size_t> joints {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Skin& skin) {
            for (const auto& [key, value] : json.items()) {
                if (key == "inverseBindMatrices") skin.inverseBindMatrices = value;
                else if (key == "skeleton") skin.skeleton = value;
                else if (key == "joints") skin.joints = value;
                else if (key == "name") skin.name = value;
                else if (key == "extensions") skin.extensions = value;
                else if (key == "extras") skin.extras = value;
            }

            if (skin.joints.empty()) throw LoadException("Skin joints not defined!");
        }
    };

    struct Texture {
        std::optional<size_t> sampler {};
        std::optional<size_t> source {};
        std::string name {};
        JSON extensions {};
        JSON extras {};

        friend void from_json(const JSON& json, Texture& texture) {
            for (const auto& [key, value] : json.items()) {
                if (key == "sampler") texture.sampler = value;
                else if (key == "source") texture.source = value;
                else if (key == "name") texture.name = value;
                else if (key == "extensions") texture.extensions = value;
                else if (key == "extras") texture.extras = value;
            }
        }
    };

    HeapArray<std::string> extensionsUsed {};
    HeapArray<std::string> extensionsRequired {};

    HeapArray<Accessor> accessors {};
    HeapArray<Animation> animations {};
    Asset asset;
    std::vector<Buffer> buffers {};
    HeapArray<BufferView> bufferViews {};
    HeapArray<Camera> cameras {};
    HeapArray<Image> images {};
    HeapArray<Material> materials {};
    HeapArray<Mesh> meshes {};
    HeapArray<Node> nodes {};
    HeapArray<Sampler> samplers {};

    size_t scene {};
    HeapArray<Scene> scenes {};

    HeapArray<Skin> skins {};
    HeapArray<Texture> textures {};
    JSON extensions {};
    JSON extras {};

    HeapArray<char> binary {};

    explicit GLTF(const std::filesystem::path& path) {
        std::ifstream file { path, std::ios::binary | std::ios::ate };

        if (!file)
            throw LoadException("Failed to open file!");

        struct Header {
            uint32_t magic;
            uint32_t version;
            uint32_t length;
        };

        Header header {};
        {
            uint32_t length = static_cast<uint32_t>(file.tellg());
            file.seekg(0, std::ios::beg);

            if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
                throw LoadException("Failed to read header!");

            if (header.magic != 0x46546C67) // "glTF"
                throw LoadException("Invalid Magic Number!");

            if (header.version != 2)
                throw LoadException(std::format("Invalid version ({})!", header.version));

            if (header.length != length)
                throw LoadException("File size does not match content!");
        }

        enum class ChunkType : uint32_t {
            json = 0x4E4F534A, // "json"
            bin = 0x004E4942 // "BIN\0"
        };

        struct ChunkHeader {
            uint32_t length;
            ChunkType type;
        };

        JSON json {};

        {
            // Loading json Buffer
            ChunkHeader chunkHeader {};

            if (!file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader))) {
                throw LoadException("Failed to read chunk header!");
            }

            if (chunkHeader.type != ChunkType::json)
                throw LoadException(std::format(
                    "First type ({:X}) is not a json chunk ({:X})!",
                    static_cast<uint32_t>(chunkHeader.type),
                    static_cast<uint32_t>(ChunkType::json)
                ));


            std::string jsonData {};
            jsonData.resize(chunkHeader.length);

            if (!file.read(jsonData.data(), chunkHeader.length))
                throw LoadException("Failed to load json data!");

            json = JSON::parse(jsonData);
        }

        // Potential issues with padding?
        while (!file.eof() && file.tellg() < header.length) {
            if (file.tellg() % 4 != 0) {
                throw LoadException(std::format("Chunk header not aligned to 4-byte boundary! {:X}!", static_cast<uint32_t>(file.tellg())));
            }

            ChunkHeader chunkHeader {};

            if (!file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader)))
                throw LoadException("Failed to read chunk header!");

            if (chunkHeader.type == ChunkType::json)
                throw LoadException("Multiple json chunks!");

            if (chunkHeader.type == ChunkType::bin) {
                if (!buffers.empty())
                    throw LoadException("Multiple binary chunks!");


                if (
                    uint32_t jsonBufferLength = json["buffers"][0].at("byteLength").get<uint32_t>();
                    jsonBufferLength > chunkHeader.length
                ) throw LoadException(std::format(
                    "json buffer[0] byteLength ({}) exceeds buffer chunk length ({})!",
                    jsonBufferLength,
                    chunkHeader.length
                ));

                binary.clear();
                binary.resize(chunkHeader.length);

                if (!file.read(binary.data(), chunkHeader.length))
                    throw LoadException("Failed to load binary data!");

            } else {
                std::cout << std::format("Ignoring chunk type (0x{:X})!", static_cast<uint32_t>(chunkHeader.type)) << std::endl;
            }

            for (const auto& [key, value] : json.items()) {
                if (key == "extensionsUsed") extensionsUsed = value;
                else if (key == "extensionsRequired") {
                    extensionsRequired = value;

                    if (!extensionsRequired.empty())  {
                        std::ostringstream stream {};
                        stream << "Missing required extensions: ";
                        for (std::uint32_t i=0; i<extensionsRequired.size() - 1; ++i) {
                            stream << extensionsRequired[i] << ", ";
                        }
                        stream << extensionsRequired[extensionsRequired.size() - 1] << '!';
                        throw LoadException(stream.str());
                    }
                }
                else if (key == "accessors") accessors = value;
                else if (key == "animations") animations = value;
                else if (key == "asset") asset  = value;
                else if (key == "buffers") buffers = value;
                else if (key == "bufferViews") bufferViews = value;
                else if (key == "cameras") cameras = value;
                else if (key == "images") images = value;
                else if (key == "materials") materials = value;
                else if (key == "meshes") meshes = value;
                else if (key == "nodes") nodes = value;
                else if (key == "textures") textures = value;
                else if (key == "samplers") samplers = value;
                else if (key == "scene") scene  = value;
                else if (key == "scenes") scenes = value;
                else if (key == "skins") skins = value;
            }
        }
    }

    Mod3DS toMod3ds() const {
        Mod3DS model {};

        if (meshes.empty()) throw std::runtime_error("No mesh found!");

        const auto imageStart = std::chrono::high_resolution_clock::now();
        for (const auto& image : images) {
            std::cout << "Image: " << image.name << std::endl;

            if (auto* uri = std::get_if<GLTF::Image::URI>(&image.data)) {
                std::cout << "URI: " << uri->uri << std::endl;
            } else {
                const auto& [mimeType, bufferView] = std::get<GLTF::Image::BufferView>(image.data);

                const auto& view = bufferViews[bufferView];

                const std::filesystem::path imagePath = image.name +
                    (mimeType == Image::MimeType::jpeg ? "temp.jpeg" : "temp.png");

                if (!M3DS::BinaryOutFile { imagePath }.write(std::span{ binary.data() + view.byteOffset, view.byteLength }))
                    throw std::runtime_error{"Failed to write to temp file!"};

                model.mTextures.emplace_back(imagePath);
                std::filesystem::remove(imagePath);
            }
        }
        const auto imageEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Loading Image data took " << std::chrono::duration_cast<std::chrono::milliseconds>(imageEnd - imageStart).count() << "ms" << std::endl;

        if (skins.size() > 1)
            throw std::runtime_error(std::format("Only one skin supported! (Found {}!)", skins.size()));

        const auto boneStart = std::chrono::high_resolution_clock::now();
        HeapArray<std::size_t> bones {};

        if (!skins.empty()) {
            bones = skins[0].joints;
            model.mBones.resize(bones.size());

            for (std::size_t i{}; i < bones.size(); ++i) {
                const GLTF::Node& node = nodes.at(bones[i]);

                model.mBones.at(i).translation = {node.translation[0], node.translation[1], node.translation[2]};
                model.mBones.at(i).rotation = {node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]};
                model.mBones.at(i).scale = {node.scale[0], node.scale[1], node.scale[2]};
                model.mBones.at(i).name = node.name;

                for (const std::size_t child : node.children) {
                    const auto it = std::ranges::find(bones, child);
                    if (it == bones.end())
                        continue;

                    const std::size_t childIndex = it - bones.begin();

                    model.mBones.at(childIndex).parent = i;
                }
            }

            const auto& opt = skins[0].inverseBindMatrices;

            if (!opt) throw std::runtime_error("No inverse bind matrices found!");

            const auto& inverseBindAccessor = accessors[*opt];
            if (!inverseBindAccessor.bufferView) throw std::runtime_error("No inverse bind buffer view found!");
            if (inverseBindAccessor.count < bones.size()) throw std::runtime_error("Not enough inverse bind matrices!");



            if (inverseBindAccessor.type != GLTF::Accessor::Type::mat4) throw std::runtime_error("Inverse bind matrices not mat4!");
            if (inverseBindAccessor.componentType != GLTF::Accessor::ComponentType::eFloat) throw std::runtime_error("Inverse bind matrices not eFloat!");

            const auto& bufferView = bufferViews[*inverseBindAccessor.bufferView];
            const std::span buffer {
                reinterpret_cast<const Matrix4x4*>(binary.data() + bufferView.byteOffset),
                inverseBindAccessor.count
            };

            model.mBones.resize(bones.size());
            for (std::size_t i{}; i < buffer.size(); ++i) {
                const auto& mtx = buffer[i];

                model.mBones[i].inverseBindMatrix = mtx;
            }
        }
        const auto boneEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Loading Bone data took " << std::chrono::duration_cast<std::chrono::milliseconds>(boneEnd - boneStart).count() << "ms" << std::endl;

        const auto meshesStart = std::chrono::high_resolution_clock::now();
        for (const auto& mesh : meshes) {
            for (const GLTF::Mesh::Primitive& primitive : mesh.primitives) {
                auto& [material, triangles] = model.mSurfaces.emplace_back();

                if (primitive.material) {
                    const auto& gltfMaterial = materials[*primitive.material];

                    // TODO: make more robust; an image can have both textures
                    if (gltfMaterial.emissiveTexture) {
                        std::cout << "Emissive found!" << std::endl;
                        const auto& textureInfo = *gltfMaterial.emissiveTexture;

                        material = {
                            {
                                .ambient { 1, 1, 1 },
                                .diffuse { 1, 1, 1 },
                                .emission { gltfMaterial.emissiveFactor[2], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[0] }
                            },
                            textureInfo.index
                        };
                    } else if (gltfMaterial.pbrMetallicRoughness) {
                        std::cout << "Metallic found!" << std::endl;
                        const auto& metallic = *gltfMaterial.pbrMetallicRoughness;

                        material.properties = {
                            .ambient { metallic.baseColorFactor[2] / 4, metallic.baseColorFactor[1] / 4, metallic.baseColorFactor[0] / 4 },
                            .diffuse { metallic.baseColorFactor[2], metallic.baseColorFactor[1], metallic.baseColorFactor[0] },
                            .emission { gltfMaterial.emissiveFactor[2], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[0] }
                        };

                        if (const auto& textureInfo = metallic.baseColorTexture) {
                            const auto& texture = textures.at(textureInfo->index);

                            if (!texture.source)
                                throw std::runtime_error("No texture source image found!");

                            std::cout << "Texture: " << textureInfo->index << std::endl;

                            material.textureIndex = *texture.source;
                        }
                    }
                    if (gltfMaterial.normalTexture) {
                        std::cout << "Normal found!" << std::endl;
                    }
                    if (gltfMaterial.occlusionTexture) {
                        std::cout << "Occlusion found!" << std::endl;
                    }
                } else {
                    std::cout << "No material found!" << std::endl;
                    continue;
                }


                std::vector<Mod3DS::Vertex> vertices {};
                for (const auto& [key, value] : primitive.attributes.items()) {
                    if (key == "POSITION") {
                        const auto& positionAccessor = accessors[value.get<int>()];

                        if (!positionAccessor.bufferView) throw std::runtime_error("No position buffer view found!");
                        if (positionAccessor.type != GLTF::Accessor::Type::vec3) throw std::runtime_error("Position Accessor not Vec3!");
                        if (positionAccessor.componentType != GLTF::Accessor::ComponentType::eFloat) throw std::runtime_error("Position Accessor not eFloat!");

                        const auto& bufferView = bufferViews[*positionAccessor.bufferView];
                        std::span buffer {
                            reinterpret_cast<const Vector3*>(binary.data() + bufferView.byteOffset + positionAccessor.byteOffset),
                            positionAccessor.count
                        };

                        vertices.resize(std::max(vertices.size(), positionAccessor.count));

                        for (size_t i{}; i < positionAccessor.count; ++i) {
                            vertices[i].coords = { buffer[i].x, buffer[i].y, buffer[i].z };
                        }
                    } else if (key == "NORMAL") {
                        const auto& normalAccessor = accessors[value.get<int>()];

                        if (!normalAccessor.bufferView) throw std::runtime_error("No normal buffer view found!");
                        if (normalAccessor.type != GLTF::Accessor::Type::vec3) throw std::runtime_error("Normal Accessor not Vec3!");
                        if (normalAccessor.componentType != GLTF::Accessor::ComponentType::eFloat) throw std::runtime_error("Normal Accessor not eFloat!");

                        const auto& bufferView = bufferViews[*normalAccessor.bufferView];
                        std::span buffer {
                            reinterpret_cast<const Vector3*>(binary.data() + bufferView.byteOffset + normalAccessor.byteOffset),
                            normalAccessor.count
                        };

                        vertices.resize(std::max(vertices.size(), normalAccessor.count));

                        for (size_t i{}; i < normalAccessor.count; ++i) {
                            vertices[i].normal = { buffer[i].x, buffer[i].y, buffer[i].z };
                        }
                    } else if (key == "TEXCOORD_0") {
                        if (!primitive.material)
                            throw std::runtime_error("No material found where TEXCOORDs are defined!");
                        if (!material.textureIndex) {
                            std::cout << "No texture index where TEXCOORDs are defined!" << std::endl;
                            continue;
                        }

                        const auto& texture = textures[*material.textureIndex];

                        const auto sampler = [&] -> Sampler {
                            if (texture.sampler)
                                return samplers[*texture.sampler];
                            std::cout << "No sampler; using default sampler...";
                            return {};
                        }();

                        const auto& texCoordAccessor = accessors[value.get<int>()];

                        if (!texCoordAccessor.bufferView)
                            throw std::runtime_error("No texture buffer view found!");
                        if (texCoordAccessor.type != GLTF::Accessor::Type::vec2)
                            throw std::runtime_error("TexCoord Accessor not Vec2!");
                        if (texCoordAccessor.componentType != GLTF::Accessor::ComponentType::eFloat)
                            throw std::runtime_error("TexCoord Accessor not eFloat!");

                        const auto& bufferView = bufferViews[*texCoordAccessor.bufferView];
                        std::span buffer {
                            reinterpret_cast<const Vector2*>(binary.data() + bufferView.byteOffset + texCoordAccessor.byteOffset),
                            texCoordAccessor.count
                        };

                        if (sampler.wrapS != GLTF::Sampler::WrappingMode::repeat || sampler.wrapT != GLTF::Sampler::WrappingMode::repeat) {
                            std::cout << "WARNING: Texture sampler not repeat! (wrapS: " << std::to_underlying(sampler.wrapS) << ", wrapT: " << std::to_underlying(sampler.wrapT) << ")" << std::endl;
                        }

                        vertices.resize(std::max(vertices.size(), texCoordAccessor.count));
                        for (size_t i{}; i < texCoordAccessor.count; ++i) {
                            Vector2 coords { buffer[i].x, buffer[i].y };

                            if (coords.x > 0) {
                                if (auto floor = std::floor(coords.x); floor != coords.x)
                                    coords.x -= floor;
                                else
                                    coords.x = 1;
                            }


                            if (coords.y > 0) {
                                if (auto floor = std::floor(coords.y); floor != coords.y)
                                    coords.y -= floor;
                                else
                                    coords.y = 1;
                            }

                            vertices[i].texCoords = { coords.x, 1 - coords.y };
                        }
                    } else if (key.starts_with("JOINTS_")) {
                        const auto& jointsAccessor = accessors[value.get<int>()];

                        if (!jointsAccessor.bufferView)
                            throw std::runtime_error(std::format("No joints buffer view found for key {}!", key));

                        auto handleBoneIndices = [&] <typename T> {
                            const auto& bufferView = bufferViews[*jointsAccessor.bufferView];
                            vertices.resize(std::max(vertices.size(), jointsAccessor.count));

                            std::span buffer {
                                reinterpret_cast<const std::array<T, 4>*>(binary.data() + bufferView.byteOffset + jointsAccessor.byteOffset),
                                jointsAccessor.count
                            };

                            for (size_t i{}; i < buffer.size(); ++i) {
                                const auto& joints = buffer[i];
                                vertices[i].boneIndices = {
                                    static_cast<std::uint16_t>(joints[0]),
                                    static_cast<std::uint16_t>(joints[1]),
                                    static_cast<std::uint16_t>(joints[2]),
                                    static_cast<std::uint16_t>(joints[3]),
                                };
                            }
                        };

                        if (jointsAccessor.componentType == GLTF::Accessor::ComponentType::eUnsignedByte)
                            handleBoneIndices.operator()<std::uint8_t>();
                        else if (jointsAccessor.componentType == GLTF::Accessor::ComponentType::eUnsignedShort)
                            handleBoneIndices.operator()<std::uint16_t>();
                        else
                            throw std::runtime_error(std::format("Joints accessor must be of type float or unsigned byte! (Got {}!)", std::to_underlying(jointsAccessor.componentType)));
                    } else if (key.starts_with("WEIGHTS_")) {
                        const auto& weightsAccessor = accessors[value.get<int>()];

                        if (!weightsAccessor.bufferView)
                            throw std::runtime_error("No weights buffer view found!");

                        const auto& bufferView = bufferViews[*weightsAccessor.bufferView];

                        vertices.resize(std::max(vertices.size(), weightsAccessor.count));

                        if (weightsAccessor.componentType != GLTF::Accessor::ComponentType::eFloat)
                            throw std::runtime_error(std::format("Weights accessor must be of type float! (Got {}!)", std::to_underlying(weightsAccessor.componentType)));

                        std::span buffer {
                            reinterpret_cast<const std::array<float, 4>*>(binary.data() + bufferView.byteOffset + weightsAccessor.byteOffset),
                            weightsAccessor.count
                        };

                        for (size_t i{}; i < buffer.size(); ++i)
                            vertices[i].boneWeights = buffer[i];
                    }
                }

                if (!primitive.indices)
                    throw std::runtime_error("No primitive indices found!");

                const auto& primitiveAccessor = accessors[*primitive.indices];
                if (primitiveAccessor.type != GLTF::Accessor::Type::scalar)
                    throw std::runtime_error(std::format("Primitive Accessor not Scalar! ({})", GLTF::Accessor::typeNames[static_cast<size_t>(primitiveAccessor.type)]));

                if (!primitiveAccessor.bufferView) throw std::runtime_error("No primitive buffer view found!");
                const auto& bufferView = bufferViews[*primitiveAccessor.bufferView];

                auto collectVertices = [&]<typename T>(std::span<const T> span) {
                    for (size_t i{}; i<span.size() / 3; ++i) {
                        triangles.emplace_back(Mod3DS::Triangle {vertices[span[i*3]], vertices[span[i*3 + 1]], vertices[span[i*3 + 2]]});
                    }
                };

                if (primitiveAccessor.componentType == GLTF::Accessor::ComponentType::eUnsignedInt) {
                    collectVertices(std::span {
                        reinterpret_cast<const uint32_t*>(binary.data() + bufferView.byteOffset + primitiveAccessor.byteOffset),
                        primitiveAccessor.count
                    });
                } else if (primitiveAccessor.componentType == GLTF::Accessor::ComponentType::eUnsignedShort) {
                    collectVertices(std::span {
                        reinterpret_cast<const uint16_t*>(binary.data() + bufferView.byteOffset + primitiveAccessor.byteOffset),
                        primitiveAccessor.count
                    });
                } else {
                    throw std::runtime_error(std::format("Primitive Accessor not UnsignedInt! ({})", static_cast<size_t>(primitiveAccessor.componentType)));
                }
            }
        }
        const auto meshesEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Loading Meshes took " << std::chrono::duration_cast<std::chrono::milliseconds>(meshesEnd - meshesStart).count() << "ms" << std::endl;

        const auto animationBegin = std::chrono::high_resolution_clock::now();
        for (const auto& anim : animations) {
            auto& animation = model.mAnimations.emplace_back();

            animation.name = anim.name;

            for (const auto& channel : anim.channels) {
                const auto nodeId = channel.target.node;

                uint16_t boneIndex;
                {
                    const auto it = std::ranges::find(bones, nodeId);

                    if (it == bones.end()) {
                        std::cout << "Bone not found! Skipping channel..." << std::endl;
                        continue;
                    }

                    boneIndex = static_cast<uint16_t>(it - bones.begin());
                }

                const auto it = std::ranges::find_if(animation.boneTracks, [&boneIndex](const auto& track) { return track.boneIndex == boneIndex; });

                auto& boneTrack = it == animation.boneTracks.end() ? animation.boneTracks.emplace_back(boneIndex) : *it;

                // const auto& node = nodes[nodeId];
                // std::cout << node.name << std::endl;

                const auto& animationSampler = anim.samplers[channel.sampler];

                const auto& inputAccessor = accessors[animationSampler.input];
                const auto& outputAccessor = accessors[animationSampler.output];

                if (inputAccessor.count != outputAccessor.count) {
                    throw std::runtime_error(std::format(
                        "Input and output accessor count not equal! {} vs {}",
                        inputAccessor.count,
                        outputAccessor.count
                    ));
                }

                if (inputAccessor.type != GLTF::Accessor::Type::scalar) {
                    throw std::runtime_error(std::format(
                        "Translation track accessor type not scalar! ({})",
                        GLTF::Accessor::typeNames[static_cast<size_t>(inputAccessor.type)]
                    ));
                }

                if (inputAccessor.componentType != GLTF::Accessor::ComponentType::eFloat) {
                    throw std::runtime_error(std::format(
                        "Translation track input accessor component type not float! ({})",
                        std::to_underlying(inputAccessor.componentType)
                    ));
                }

                if (outputAccessor.componentType != GLTF::Accessor::ComponentType::eFloat) {
                    throw std::runtime_error(std::format(
                        "Translation track output accessor component type not float! ({})",
                        std::to_underlying(inputAccessor.componentType)
                    ));
                }

                if (!inputAccessor.bufferView) {
                    throw std::runtime_error("No translation track input buffer view found!");
                }

                if (!outputAccessor.bufferView) {
                    throw std::runtime_error("No translation track input buffer view found!");
                }

                const auto& inputBufferView = bufferViews[*inputAccessor.bufferView];
                const auto& outputBufferView = bufferViews[*outputAccessor.bufferView];

                if (channel.target.path == "translation") {
                    auto& track = boneTrack.translationTrack;
                    track.interpolationMethod = static_cast<uint8_t>(animationSampler.interpolation);

                    if (outputAccessor.type != GLTF::Accessor::Type::vec3) {
                        throw std::runtime_error(std::format(
                            "Translation track output accessor type not vec3! ({})",
                            GLTF::Accessor::typeNames[static_cast<size_t>(inputAccessor.type)]
                        ));
                    }

                    std::span inputBuffer { reinterpret_cast<const float*>(binary.data() + inputBufferView.byteOffset), inputAccessor.count };
                    std::span outputBuffer { reinterpret_cast<const Vec3<float>*>(binary.data() + outputBufferView.byteOffset), outputAccessor.count };

                    auto inIt = inputBuffer.begin();
                    auto outIt = outputBuffer.begin();
                    for (; inIt != inputBuffer.end(); ++inIt, ++outIt) {
                        track.frames.emplace(*inIt, *outIt);
                    }
                } else if (channel.target.path == "rotation") {
                    auto& track = boneTrack.rotationTrack;
                    track.interpolationMethod = static_cast<uint8_t>(animationSampler.interpolation);

                    if (outputAccessor.type != GLTF::Accessor::Type::vec4) {
                        throw std::runtime_error(std::format(
                            "Translation track output accessor type not vec4! ({})",
                            GLTF::Accessor::typeNames[static_cast<size_t>(inputAccessor.type)]
                        ));
                    }

                    std::span inputBuffer { reinterpret_cast<const float*>(binary.data() + inputBufferView.byteOffset), inputAccessor.count };
                    std::span outputBuffer { reinterpret_cast<const Quat<float>*>(binary.data() + outputBufferView.byteOffset), outputAccessor.count };

                    auto inIt = inputBuffer.begin();
                    auto outIt = outputBuffer.begin();
                    for (; inIt != inputBuffer.end(); ++inIt, ++outIt) {
                        track.frames.emplace(*inIt, *outIt);
                    }
                } else if (channel.target.path == "scale") {
                    auto& track = boneTrack.scaleTrack;
                    track.interpolationMethod = static_cast<uint8_t>(animationSampler.interpolation);

                    if (outputAccessor.type != GLTF::Accessor::Type::vec3) {
                        throw std::runtime_error(std::format(
                            "Translation track output accessor type not vec3! ({})",
                            GLTF::Accessor::typeNames[static_cast<size_t>(inputAccessor.type)]
                        ));
                    }

                    std::span inputBuffer { reinterpret_cast<const float*>(binary.data() + inputBufferView.byteOffset), inputAccessor.count };
                    std::span outputBuffer { reinterpret_cast<const Vec3<float>*>(binary.data() + outputBufferView.byteOffset), outputAccessor.count };

                    auto inIt = inputBuffer.begin();
                    auto outIt = outputBuffer.begin();
                    for (; inIt != inputBuffer.end(); ++inIt, ++outIt) {
                        track.frames.emplace(*inIt, *outIt);
                    }
                }
            }
        }
        const auto animationEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Loading Animations took " << std::chrono::duration_cast<std::chrono::milliseconds>(animationEnd - animationBegin).count() << "ms" << std::endl;

        return model;
    }
};
