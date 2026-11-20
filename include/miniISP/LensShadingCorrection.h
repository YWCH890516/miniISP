#pragma once

#include <vector>
#include "miniISP/InputData.h"


namespace mini_isp{

    struct LSCGain{
        float rGain{1.0f};
        float grGain{1.0f};
        float gbGain{1.0f};
        float bGain{1.0f};
    };

    struct LensShadingCorrectionMap{
        int gridWidth{};
        int gridHeight{};
        std::vector<std::vector<LSCGain>> gains{};
    };

    void BuildLensShadingMap(const std::vector<uint16_t>& flatFieldImg, int width, int height, BayerPattern pattern, int gridWidth, int gridHeight, LensShadingCorrectionMap& lscMap);

    void ApplyLensShadingCorrection(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t whiteLevel ,const LensShadingCorrectionMap& lscMap);
    void ApplyLensShadingCorrectionV2(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t whiteLevel ,const LensShadingCorrectionMap& lscMap);

}