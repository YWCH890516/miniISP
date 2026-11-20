#pragma once

#include <vector>
#include "miniISP/InputData.h"
namespace mini_isp{
    enum class SyntheticDefectType{
        Hot,
        Dead,
        Stuck
    };

    struct SyntheticDefect{
        int x{};
        int y{};
        SyntheticDefectType type{};
        uint16_t stuckVal{};
    };
    std::vector<SyntheticDefect> GenerateRandomHotPixel(int width, int height, std::size_t count);
    void InjectSyntheticDefect16(const std::vector<uint16_t>& raw, std::vector<uint16_t>& output, int width, int height, uint16_t blackLevel, uint16_t whiteLevel, const std::vector<SyntheticDefect>& defects);

    void DefectivePixelCorrection16_v1(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t threshold);
}
