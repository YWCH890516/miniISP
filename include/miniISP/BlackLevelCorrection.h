#pragma once
#include<vector>
#include "miniISP/InputData.h"
namespace mini_isp{
    void ApplyBlackLevelCorrection16_v1(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, uint16_t blackLevel);
    // v2 : each channel reduce BlackLevel 
    void ApplyBlackLevelCorrection16_v2(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, BayerBlackLevel levels);
}