#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <flat_set>
#include <iostream>
#include <optional>
#include <stdfloat>
#include <string_view>
#include <vector>

#include <BinaryFile.hpp>

template<typename T> struct Vec2 { T x, y; };
template<typename T> struct Vec3 { T x, y, z; };
template<typename T> struct Quat { T x, y, z, w; };

using Vector2 = Vec2<float>;
using Vec2_f16 = Vec2<std::float16_t>;

using Vector3 = Vec3<float>;
using Vec3_f16 = Vec3<std::float16_t>;

using Quaternion = Quat<float>;
using Quat_f16 = Quat<std::float16_t>;

using Matrix4x4 = std::array<std::array<float, 4>, 4>;
using Matrix4x4_f16 = std::array<std::array<std::float16_t, 4>, 4>;

struct RGB { float r, g, b; };
struct BGR { float b, g, r; };

/*  File Layout
 *
 *
 *  Header:
 *      char[4] magic "M3DS"
 *
 *      [offsets from the start of the file, 0 if not present]
 *                                      REQUIRED?
 *      u32 surfaceDataOffset           YES
 *      u32 triangleDataOffset          YES
 *      u32 t3xDataOffset               NO
 *      u32 boneDataOffset              NO
 *      u32 animationDataOffset         NO
 *
 *  SurfaceData:
 *      Header:
 *          char[4] magic "SURF"
 *          u32 surfaceCount
 *      Surface[surfaceCount] surfaces
 *
 *  TriangleData:
 *      Header:
 *          char[4] magic "TRIA"
 *          u32 triangleCount
 *      Triangle[triangleCount] triangleData
 *
 *  T3XData:
 *      Header:
 *          char[4] magic "T3Xs"
 *          u32 t3xCount
 *      T3Xs[t3xCount]
 *
 *  BoneData:
 *      Header:
 *          char[4] magic "BONE"
 *          u16 boneCount
 *          u32 boneNameDataLength
 *      char[boneNameDataLength] boneNameData
 *      Bone[boneCount]
 *      u16[boneCount] updateOrder
 *
 *  AnimationData:
 *      Header:
 *          char[4] magic "ANIM"
 *          u32 animationCount
 *      Animation[animationCount] animations
 *
 *
 *  Surface:
 *      Material material
 *      bool hasTexture
 *      u8 padding
 *      u16 textureIndex
 *      u32 triangleStartIndex (from start of triangleData)
 *      u32 triangleCount
 *      u16[maxBonesPerSurface] boneMappings
 *
 *  T3X:
 *      u32 t3xSize
 *      char[t3xSize] data
 *
 *  Bone:
 *      i32 parentBone
 *      Vector3 translation
 *      Quaternion rotation
 *      Vector3 scale
 *      Matrix4x4 inverseBindMatrix
 *      u32 nameStartIndex
 *      u32 nameLength
 *
 * Animation:
 *      Header:
 *          u32 nameLength
 *          u32 boneTrackCount
 *          Seconds<float> duration
 *      char[nameLength] name
 *      BoneTrack[boneTrackCount] boneTracks
 *
 *      BoneTrack:
 *          Header:
 *              u16 boneIndex
 *
 *              u32 translationFrameCount
 *              u32 rotationFrameCount
 *              u32 scaleFrameCount
 *
 *              u8 translationInterpolationMethod
 *              u8 rotationInterpolationMethod
 *              u8 scaleInterpolationMethod
 *
 *          f16[] data
 *              TranslationFrame[]
 *              RotationFrame[]
 *              ScaleFrame[]
 *
 *      TranslationFrame:
 *          f16 time
 *          f16 x, y, z
 *
 *      RotationFrame:
 *          f16 time
 *          f16 x, y, z, w
 *
 *      ScaleFrame:
 *          f16 time
 *          f16 x, y, z
 */

inline std::string getTex3DSPath() {
    std::string_view path = std::getenv("DEVKITPRO");
#ifdef _WIN32 
    if (path.starts_with("/opt/"))
        return std::format("C:/{}/tools/bin/tex3ds", path.substr(5));
#endif
    return std::format("{}/tools/bin/tex3ds", path);
}

struct Mod3DS {
    static constexpr std::string_view extension = ".mod3ds";
    static constexpr std::uint16_t maxBonesPerSurface = 28;

    struct Material {
        struct Properties {
            BGR ambient { 0.25, 0.25, 0.25 };
            BGR diffuse { 1, 1, 1 };
            BGR specular0 { 0.5, 0.5, 0.5 };
            BGR specular1 { 0.5, 0.5, 0.5 };
            BGR emission { 0, 0, 0 };
        } properties {};

        std::optional<std::uint16_t> textureIndex {};
    };

    struct Vertex {
        Vector3 coords {};
        Vector2 texCoords {};
        Vector3 normal {};
        std::array<std::uint16_t, 4> boneIndices { 0, 0, 0, 0 };
        std::array<float, 4> boneWeights { 0, 0, 0, 0 };
    };

    using Triangle = std::array<Vertex, 3>;
    struct T3X {
        std::vector<char> mData {};
        std::filesystem::path mPath {};

        explicit T3X(std::filesystem::path p) : mPath(std::move(p)) {
            constexpr std::string_view t3xPath = "temp.t3x";

            if (system(std::format("{} --atlas -f rgba8888 -z -i {} -o {} > nul", getTex3DSPath(), mPath.string(), t3xPath).c_str()) != 0)
                throw std::runtime_error(std::format("Failed to load t3x! {}", mPath.string()));

            // std::ifstream t3xFile { std::filesystem::path(t3xPath), std::ios::binary | std::ios::ate };
            {
                M3DS::BinaryInFile t3xFile { t3xPath };
                mData.resize(t3xFile.length());

                if (!t3xFile.read(std::span{mData.data(), mData.size()}))
                    throw std::runtime_error(std::format("Failed to read t3x! {}", mPath.string()));
            }
            std::filesystem::remove(t3xPath);
        }
    };

    struct Surface {
        Material material {};
        std::vector<Triangle> triangles {};
    };

    struct Animation {
        struct BoneTrack {
            template <typename T>
            struct Track {
                struct Frame {
                    float time {};
                    T value {};

                    constexpr auto operator<=>(const Frame& other) const noexcept {
                        return time <=> other.time;
                    }

                    constexpr auto operator<=>(const float t) const noexcept {
                        return t <=> time;
                    }
                };

                std::uint8_t interpolationMethod {};
                std::flat_set<Frame> frames {};
            };

            using RotationTrack = Track<Quaternion>;
            using TranslationTrack = Track<Vector3>;
            using ScaleTrack = Track<Vector3>;

            std::uint16_t boneIndex {};
            RotationTrack rotationTrack {};
            TranslationTrack translationTrack {};
            ScaleTrack scaleTrack {};

            constexpr auto operator<=>(const BoneTrack& other) const noexcept {
                return boneIndex <=> other.boneIndex;
            }
        };

        std::string name {};
        std::vector<BoneTrack> boneTracks {};
    };

    struct Bone {
        std::optional<std::uint16_t> parent {};
        std::string name {};
        Vector3 translation {};
        Quaternion rotation {};
        Vector3 scale {};
        Matrix4x4 inverseBindMatrix {};
    };

    std::vector<Surface> mSurfaces {};
    std::vector<T3X> mTextures {};
    std::vector<Bone> mBones {};
    std::vector<Animation> mAnimations {};

    void save(const std::filesystem::path& path) const {
        std::cout << std::format("Saving {}...", path.string()) << std::endl;
        M3DS::BinaryOutFile file { path };
        struct Header {
            std::array<char, 4> magic { 'M', '3', 'D', 'S' };
            std::uint32_t surfaceDataOffset {};
            std::uint32_t triangleDataOffset {};
            std::uint32_t t3xDataOffset {};
            std::uint32_t boneDataOffset {};
            std::uint32_t animationDataOffset {};
        } header {};

        if (!file.write(header))
            throw std::runtime_error{"Failed to write to file!"};

        // Surface Data
        const auto surfaceStart = std::chrono::high_resolution_clock::now();
        {
            struct ReducedVertex {
                Vector3 coords {};
                Vector2 texCoords {};
                Vector3 normal {};
                std::array<std::uint8_t, 4> boneIndices { 0, 0, 0, 0};
                std::array<float, 4> boneWeights { 0, 0, 0, 0};
            };

            using ReducedTriangle = std::array<ReducedVertex, 3>;

            struct ReducedSurface {
                Material material {};
                std::vector<ReducedTriangle> triangles {};
                std::array<std::uint16_t, maxBonesPerSurface> boneIndices {};
            };

            std::vector<ReducedSurface> boneReducedSurfaces {};

            for (const auto& [material, triangles] : mSurfaces) {
                std::array<std::uint16_t, maxBonesPerSurface> boneIndices {};
                std::uint16_t boneIndexCount {};

                std::vector<bool> usedTriangles(triangles.size(), false);

                std::vector<std::uint8_t> boneMappings {};

                const auto testTri = [&](const Triangle& tri) -> bool {
                    std::array<std::uint16_t, 12> required {};
                    std::uint_fast8_t count {};

                    for (const auto& v : tri) {
                        for (std::size_t i{}; i < 4; ++i) {
                            if (
                                const auto bone = v.boneIndices[i];
                                v.boneWeights[i] != 0 &&
                                !std::ranges::contains(required.begin(), required.begin() + count, bone) &&
                                !std::ranges::contains(boneIndices.begin(), boneIndices.begin() + boneIndexCount, bone)
                            ) {
                                required[count++] = bone;
                            }
                        }
                    }

                    if (boneIndexCount + count > maxBonesPerSurface) {
                        return false;
                    }

                    for (std::uint_fast8_t i{}; i < count; ++i) {
                        const auto b = required[i];

                        boneIndices[boneIndexCount++] = b;

                        boneMappings.resize(std::max(boneMappings.size(), static_cast<std::size_t>(b) + 1));
                        boneMappings[b] = static_cast<uint8_t>(boneIndexCount - 1);
                    }

                    return true;
                };

                std::size_t remaining = triangles.size();

                if (remaining == 0) std::cout << "Empty surface?" << std::endl;

                while (remaining > 0) {
                    ReducedSurface& surface = boneReducedSurfaces.emplace_back();
                    surface.material = material;

                    boneIndexCount = 0;
                    boneMappings.clear();

                    for (std::size_t i{}; i < triangles.size(); ++i) {
                        if (usedTriangles[i])
                            continue;

                        if (const auto& tri = triangles[i]; testTri(tri)) {
                            usedTriangles[i] = true;
                            --remaining;

                            const auto map = [&boneMappings](const Vertex& v) -> ReducedVertex{
                                return {
                                    v.coords,
                                    v.texCoords,
                                    v.normal,
                                    boneMappings.empty() ? std::array<std::uint8_t, 4>{0, 0, 0, 0} : std::array{boneMappings[v.boneIndices[0]], boneMappings[v.boneIndices[1]], boneMappings[v.boneIndices[2]], boneMappings[v.boneIndices[3]]},
                                    v.boneWeights
                                };
                            };

                            surface.triangles.emplace_back(ReducedTriangle {map(tri[0]), map(tri[1]), map(tri[2])});
                        }
                    }

                    surface.boneIndices = boneIndices;
                }
            }

            header.triangleDataOffset = static_cast<std::uint32_t>(file.tell());
            struct TriangleDataHeader {
                std::array<char, 4> magic { 'T', 'R', 'I', 'A' };
                std::uint32_t triangleCount {};
            } triangleDataHeader {};

            for (const auto& surface : boneReducedSurfaces) {
                triangleDataHeader.triangleCount += static_cast<uint32_t>(surface.triangles.size());
            }

            if (!file.write(triangleDataHeader))
                throw std::runtime_error{"Failed to write to file!"};

            for (const auto& surface : boneReducedSurfaces) {
                if (!file.write(std::span{surface.triangles}))
                    throw std::runtime_error{"Failed to write to file!"};
            }

            header.surfaceDataOffset = static_cast<std::uint32_t>(file.tell());
            struct SurfaceDataHeader {
                std::array<char, 4> magic { 'S', 'U', 'R', 'F' };
                std::uint32_t surfaceCount {};
            } surfaceDataHeader {
                .surfaceCount = static_cast<uint32_t>(boneReducedSurfaces.size())
            };

            if (!file.write(surfaceDataHeader))
                throw std::runtime_error{"Failed to write to file!"};

            struct SurfaceHeader {
                Material::Properties material {};
                bool hasTexture {};
                std::uint16_t textureIndex {};
                std::uint32_t triangleStartIndex {};
                std::uint32_t triangleCount {};
                std::array<std::uint16_t, maxBonesPerSurface> boneMappings {};
            };

            std::uint32_t pos {};

            for (const auto& surface : boneReducedSurfaces) {
                SurfaceHeader surfaceHeader {
                    .material = surface.material.properties,
                    .hasTexture = surface.material.textureIndex.has_value(),
                    .textureIndex = surface.material.textureIndex.value_or(0),
                    .triangleStartIndex = pos,
                    .triangleCount = static_cast<uint32_t>(surface.triangles.size()),
                    .boneMappings = surface.boneIndices
                };

                if (!file.write(surfaceHeader))
                    throw std::runtime_error{"Failed to write to file!"};

                pos += static_cast<std::uint32_t>(surfaceHeader.triangleCount * sizeof(surface.triangles[0]));
            }
        }
        const auto surfaceEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Saving Surface data took " << std::chrono::duration_cast<std::chrono::milliseconds>(surfaceEnd - surfaceStart).count() << "ms" << std::endl;

        const auto t3xStart = std::chrono::high_resolution_clock::now();
        // T3X Data
        {
            header.t3xDataOffset = static_cast<std::uint32_t>(file.tell());
            struct T3XDataHeader {
                std::array<char, 4> magic { 'T', '3', 'X', 's' };
                std::uint32_t t3xCount {};
            } const t3xDataHeader {
                .t3xCount = static_cast<uint32_t>(mTextures.size())
            };

            if (!file.write(t3xDataHeader))
                throw std::runtime_error{"Failed to write to file!"};

            for (const auto& tex : mTextures) {
                struct T3XHeader {
                    uint32_t t3xSize;
                } texHeader { .t3xSize = static_cast<uint32_t>(tex.mData.size()) };

                if (!file.write(texHeader) || !file.write(std::span{tex.mData}))
                    throw std::runtime_error{"Failed to write to file!"};
            }
        }
        const auto t3xEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Saving T3X data took " << std::chrono::duration_cast<std::chrono::milliseconds>(t3xEnd - t3xStart).count() << "ms" << std::endl;

        const auto boneStart = std::chrono::high_resolution_clock::now();
        // Bone Data
        {
            header.boneDataOffset = static_cast<std::uint32_t>(file.tell());
            struct BoneDataHeader {
                std::array<char, 4> magic { 'B', 'O', 'N', 'E' };
                std::uint16_t boneCount {};
                std::uint32_t boneNameDataLength {};
            } boneDataHeader {
                .boneCount = static_cast<std::uint16_t>(mBones.size()),
                .boneNameDataLength = [&] {
                    std::uint32_t length {};
                    for (const auto& bone : mBones)
                        length += static_cast<std::uint32_t>(bone.name.size());

                    return length;
                }()
            };

            if (!file.write(boneDataHeader))
                throw std::runtime_error{"Failed to write to file!"};

            for (const auto& bone : mBones) {
                if (!file.write(std::span{bone.name}))
                    throw std::runtime_error{"Failed to write to file!"};
            }

            std::uint32_t nameStartIndex {};

            for (const auto& bone : mBones) {
                if (
                    !file.write(bone.parent ? static_cast<int32_t>(bone.parent.value()) : -1) ||
                    !file.write(bone.translation) ||
                    !file.write(bone.rotation) ||
                    !file.write(bone.scale)
                )
                    throw std::runtime_error{"Failed to write to file!"};

                // file.write(bone.inverseBindMatrix);
                Matrix4x4 result{};
                for (int row = 0; row < 4; ++row)
                    for (int col = 0; col < 4; ++col)
                        result[row][3 - col] = bone.inverseBindMatrix[col][row];

                if (!file.write(result) || !file.write(nameStartIndex))
                    throw std::runtime_error{"Failed to write to file!"};
                nameStartIndex += static_cast<std::uint32_t>(bone.name.size());
                if (!file.write(static_cast<std::uint32_t>(bone.name.size())))
                    throw std::runtime_error{"Failed to write to file!"};
            }

            struct OrderElement {
                std::uint16_t boneIndex {};
                std::uint16_t parentIndex {};
            };

            std::vector<OrderElement> orderElements {};

            for (std::uint16_t i = 0; i < boneDataHeader.boneCount; ++i) {
                orderElements.emplace_back(
                    i,
                    mBones.at(i).parent.value_or(-1)
                );
            }

            if (orderElements.size() != mBones.size())
                throw std::runtime_error("Bone count mismatch!");

            std::ranges::sort(orderElements, [](const OrderElement& a, const OrderElement& b) { return a.parentIndex < b.parentIndex; });

            for (const auto& element : orderElements) {
                if (!file.write(element.boneIndex))
                    throw std::runtime_error{"Failed to write to file!"};
            }
        }
        const auto boneEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Saving Bone data took " << std::chrono::duration_cast<std::chrono::milliseconds>(boneEnd - boneStart).count() << "ms" << std::endl;

        const auto animStart = std::chrono::high_resolution_clock::now();
        // Animation Data
        {
            header.animationDataOffset = static_cast<std::uint32_t>(file.tell());
            struct AnimationDataHeader {
                std::array<char, 4> magic { 'A', 'N', 'I', 'M' };
                std::uint32_t animationCount {};
            } animationDataHeader {
                .animationCount = static_cast<uint32_t>(mAnimations.size())
            };

            if (!file.write(animationDataHeader))
                throw std::runtime_error{"Failed to write to file!"};

            for (const auto& animation : mAnimations) {
                struct AnimationHeader {
                    std::uint32_t nameLength {};
                    std::uint32_t boneTrackCount {};
                    float duration {};
                } animationHeader {
                    .nameLength = static_cast<std::uint32_t>(animation.name.size()),
                    .boneTrackCount = static_cast<uint32_t>(animation.boneTracks.size())
                };

                for (const auto& boneTrack : animation.boneTracks) {
                    const auto getTrackTime = []<typename T>(const Animation::BoneTrack::Track<T>& track) -> float {
                        if (track.frames.empty()) return 0;
                        return track.frames.rbegin()->time;
                    };

                    animationHeader.duration = std::max({
                        animationHeader.duration,
                        getTrackTime(boneTrack.translationTrack),
                        getTrackTime(boneTrack.rotationTrack),
                        getTrackTime(boneTrack.scaleTrack)
                    });
                }

                std::cout << std::format("Animation: {} - {}s", animation.name, animationHeader.duration) << std::endl;

                if (!file.write(animationHeader) || !file.write(std::span{animation.name}))
                    throw std::runtime_error{"Failed to write to file!"};

                for (const auto& boneTrack : animation.boneTracks) {
                    struct BoneTrackHeader {
                        std::uint16_t boneIndex {};

                        std::uint32_t translationTrackCount {};
                        std::uint32_t rotationTrackCount {};
                        std::uint32_t scaleTrackCount {};

                        std::uint8_t translationMethod {};
                        std::uint8_t rotationMethod {};
                        std::uint8_t scaleMethod {};
                    } boneTrackHeader {
                        .boneIndex = boneTrack.boneIndex,

                        .translationTrackCount = static_cast<std::uint32_t>(boneTrack.translationTrack.frames.size()),
                        .rotationTrackCount = static_cast<std::uint32_t>(boneTrack.rotationTrack.frames.size()),
                        .scaleTrackCount = static_cast<std::uint32_t>(boneTrack.scaleTrack.frames.size()),

                        .translationMethod = boneTrack.translationTrack.interpolationMethod,
                        .rotationMethod = boneTrack.rotationTrack.interpolationMethod,
                        .scaleMethod = boneTrack.scaleTrack.interpolationMethod
                    };
                    if (!file.write(boneTrackHeader))
                        throw std::runtime_error{"Failed to write to file!"};

                    const auto writeTrack = [&file]<typename T>(const Animation::BoneTrack::Track<T>& track) {
                        for (const auto& frame : track.frames) {
                            if (
                                !file.write(static_cast<std::float16_t>(frame.time)) ||
                                !file.write(static_cast<std::float16_t>(frame.value.x)) ||
                                !file.write(static_cast<std::float16_t>(frame.value.y)) ||
                                !file.write(static_cast<std::float16_t>(frame.value.z))
                            )
                                throw std::runtime_error{"Failed to write to file!"};
                            if constexpr (std::is_same_v<T, Quaternion>) {
                                if (!file.write(static_cast<std::float16_t>(frame.value.w)))
                                    throw std::runtime_error{"Failed to write to file!"};
                            }
                        }
                    };

                    writeTrack(boneTrack.translationTrack);
                    writeTrack(boneTrack.rotationTrack);
                    writeTrack(boneTrack.scaleTrack);
                }
            }
        }
        const auto animEnd = std::chrono::high_resolution_clock::now();
        std::cout << "Saving Animation data took " << std::chrono::duration_cast<std::chrono::milliseconds>(animEnd - animStart).count() << "ms" << std::endl;

        if (!file.seek(0) || !file.write(header))
            throw std::runtime_error{"Failed to write to file!"};
    }
};
