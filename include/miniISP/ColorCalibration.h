#pragma once

#include <array>
#include <vector>

#include "miniISP/InputData.h"

namespace mini_isp {
   
    struct Point2f{
        float x;
        float y;
    };

    struct ColorChartCorners{
        Point2f topLeft;
        Point2f topRight;
        Point2f bottomLeft;
        Point2f bottomRight;
    };

    struct PatchColorDouble{
        double r{};
        double g{};
        double b{};
    };
    using Matrix3x3 = std::array<std::array<double,3>,3>;

    Matrix3x3 CalculateCCM(const std::array<PatchColorDouble, 24>& measuredColors, const std::array<PatchColorDouble, 24>& referenceColors, double whiteLevel);
    

    PatchColorDouble ApplyCCM(const PatchColorDouble& input, const Matrix3x3& ccm);

    void ApplyCCMImage(const std::vector<RGBFormat16>& input, std::vector<RGBFloatFormat>& output, const Matrix3x3& ccm , double whiteLevel);


    std::array<PatchColorDouble, 24> GetColorChartPatchColors(
        const std::vector<uint16_t>& bayer,
        int width,
        int height,
        BayerPattern pattern,
        const ColorChartCorners& corners);
    
    void DrawColorChartROIs(std::vector<mini_isp::RGBFormat>& image, int width , int height, const mini_isp::ColorChartCorners& corners, int roiHalfSize = 30);

}
