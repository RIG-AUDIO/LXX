#pragma once

#include <array>
#include <cstdint>

namespace lxx::dsp {

static constexpr std::size_t kDrumMapNodeBytes = 48;

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode0 = {
    236, 0, 0, 138, 0, 0, 208, 0, 58, 28, 174, 0, 104, 0, 58, 0, 10, 66, 0, 8, 232, 0, 0, 38, 0, 148, 0, 14, 198, 0, 114, 0, 154, 98, 244, 34, 160, 108, 192, 24, 160, 98, 228, 20, 160, 92, 194, 44
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode1 = {
    246, 10, 88, 14, 214, 10, 62, 8, 250, 8, 40, 14, 198, 14, 160, 120, 16, 186, 44, 52, 230, 12, 116, 18, 22, 154, 10, 18, 246, 88, 72, 58, 136, 130, 220, 64, 130, 120, 156, 32, 128, 112, 220, 32, 126, 106, 184, 88
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode2 = {
    224, 0, 98, 0, 0, 68, 0, 198, 0, 136, 174, 0, 46, 28, 116, 12, 0, 94, 0, 0, 224, 160, 20, 34, 0, 52, 0, 0, 194, 0, 16, 118, 228, 104, 138, 90, 122, 102, 108, 76, 196, 160, 182, 160, 96, 36, 202, 22
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode3 = {
    240, 204, 42, 0, 86, 108, 66, 104, 190, 22, 224, 0, 14, 148, 0, 36, 0, 0, 112, 62, 232, 180, 0, 34, 0, 48, 26, 18, 214, 18, 138, 38, 232, 186, 224, 182, 108, 60, 80, 62, 142, 42, 24, 34, 136, 14, 170, 26
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode4 = {
    228, 14, 36, 24, 74, 54, 122, 26, 186, 14, 96, 34, 18, 30, 48, 12, 2, 0, 46, 38, 226, 0, 68, 0, 2, 0, 92, 30, 232, 166, 116, 22, 64, 12, 236, 128, 160, 30, 202, 74, 68, 28, 228, 120, 160, 28, 188, 82
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode5 = {
    236, 24, 14, 54, 0, 0, 106, 0, 202, 220, 0, 178, 0, 160, 140, 8, 134, 82, 114, 160, 224, 0, 22, 44, 66, 40, 0, 0, 192, 22, 14, 158, 174, 86, 230, 58, 124, 64, 210, 58, 160, 76, 224, 22, 124, 34, 194, 26
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode6 = {
    236, 0, 226, 0, 0, 0, 160, 0, 0, 0, 188, 0, 0, 0, 210, 0, 26, 188, 0, 62, 242, 102, 8, 160, 22, 216, 0, 48, 200, 112, 30, 22, 230, 212, 222, 228, 180, 14, 114, 32, 160, 38, 66, 12, 154, 22, 88, 36
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode7 = {
    226, 0, 42, 0, 66, 0, 226, 14, 238, 0, 126, 0, 84, 10, 170, 22, 0, 0, 54, 0, 182, 0, 128, 36, 6, 10, 84, 10, 238, 8, 158, 26, 240, 46, 218, 24, 232, 0, 96, 0, 240, 28, 204, 30, 214, 0, 64, 0
};

inline constexpr std::array<uint8_t, kDrumMapNodeBytes> kDrumMapNode8 = {
    228, 0, 212, 0, 14, 0, 214, 0, 160, 52, 218, 0, 0, 0, 134, 32, 104, 0, 22, 84, 230, 22, 0, 58, 6, 0, 138, 20, 220, 18, 176, 34, 230, 26, 52, 24, 82, 28, 52, 118, 154, 26, 52, 24, 202, 212, 186, 196
};

inline const std::array<uint8_t, kDrumMapNodeBytes>& drumMapNodeArray(uint8_t nodeIndex)
{
    switch (nodeIndex) {
        case 0: return kDrumMapNode0;
        case 1: return kDrumMapNode1;
        case 2: return kDrumMapNode2;
        case 3: return kDrumMapNode3;
        case 4: return kDrumMapNode4;
        case 5: return kDrumMapNode5;
        case 6: return kDrumMapNode6;
        case 7: return kDrumMapNode7;
        case 8: return kDrumMapNode8;
        default: return kDrumMapNode0;
    }
}

inline const std::array<uint8_t, kDrumMapNodeBytes>& drumMapNode(uint8_t row, uint8_t col)
{
    static constexpr std::array<std::array<uint8_t, 3>, 3> kLayout = {{
        { 8, 3, 6 },
        { 2, 4, 0 },
        { 7, 1, 5 },
    }};
    const uint8_t r = row > 2 ? 2 : row;
    const uint8_t c = col > 2 ? 2 : col;
    return drumMapNodeArray(kLayout[r][c]);
}

}  // namespace lxx::dsp
