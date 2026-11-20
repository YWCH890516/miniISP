#pragma once
#include <cstdint>
#include <vector>

#include "miniISP/InputData.h"

namespace mini_isp{
    // 8 bit function
    void DemosaicOnePixelBoundary(const std::vector<uint8_t>& bayerRaw, RGBFormat& outPixel, int x, int y, int width, int height);
    void DemosaicByBayer(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height);
    void DemosaicByBayer_Pointer(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height);
    void DemosaicByBayer_Optimal(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height);
    void DemosaicByBayer_EdgeAware(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height);
    void DemosaicByBayer_EdgeAwareV2(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height);

    // 16bit function 
    void DemosaicBilinear16(const std::vector<uint16_t>& bayerRaw, std::vector<RGBFormat16>& pixels, int width, int height, BayerPattern pattern, uint16_t whiteLevel);

}
