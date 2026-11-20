#pragma once
#include <vector>

#include "miniISP/InputData.h"
namespace mini_isp{
    void ApplyHighDynamicRange(const std::vector<RGBFormat>& lowExposure, const std::vector<RGBFormat>& midExposure, const std::vector<RGBFormat>& highExposure, std::vector<RGBFloatFormat>& hdrImage, int width, int height, float lowExposureTime, float midExposureTime, float highExposureTime);
    void ToneMapReinhard(const std::vector<RGBFloatFormat>& hdrImage, std::vector<RGBFormat>& ldrImage, float key = 0.18f);

    void AdaptiveExposureFusion(const std::vector<RGBFormat>& lowExposure, const std::vector<RGBFormat>& midExposure, const std::vector<RGBFormat>& highExposure, std::vector<RGBFormat>& fusedImage, int width, int height, int gridWidth = 8, int gridHeight = 8);
}
