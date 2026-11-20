#pragma once

#include <vector>
#include "miniISP/InputData.h"
namespace mini_isp{
    void AWB_GrayWorld(const std::vector<uint8_t>& bayerRaw, std::vector<uint8_t>& bayerAWB, int width , int height);
    void AWB_GrayWorldV2(const std::vector<uint8_t>& bayerRaw, std::vector<uint8_t>& bayerAWB, int width , int height);
    void AWB_GrayWorldV2_16(const std::vector<uint16_t>& bayerRaw, std::vector<uint16_t>& bayerAWB, int width , int height, BayerPattern pattern, uint16_t whiteLevel);
    // Applay Gain by Color Checkers
    void ApplyWhiteBalanceGains16(const std::vector<uint16_t>& bayerRaw, std::vector<uint16_t>& bayerAWB, int width, int height,BayerPattern pattern, double gainR, double gainG, double gainB, uint16_t whiteLevel);
}

