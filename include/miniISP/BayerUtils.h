# pragma once
#include <cstdint>
#include <vector>
#include "miniISP/InputData.h"

namespace mini_isp{
    uint8_t GetBayerPixel_EqualBoundary(const std::vector<uint8_t>& bayerRaw, int x, int y, int width, int height);
    // Clamp coordinates to the nearest valid boundary pixel.
    // Note: this does not preserve CFA parity at image boundaries.
    uint16_t GetBayerPixelClamped16(const std::vector<uint16_t>& bayerRaw, int x, int y, int width, int height);
    uint16_t GetBayerPixelMirror16(const std::vector<uint16_t>& bayerRaw, int x, int y, int width, int height);
    int MirrorIndex(int coordinate, int length);
    CFAColor GetCFAColor(BayerPattern pattern, int x, int y);
    CFAPhase GetCFAPhase(BayerPattern pattern, int x, int y);
}
