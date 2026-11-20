#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "miniISP/BayerUtils.h"
#include "miniISP/BlackLevelCorrection.h"
#include "miniISP/DefectivePixelCorrection.h"
#include "miniISP/Demosaic.h"
#include "miniISP/HighDynamicRange.h"

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestCfaMappings() {
    using mini_isp::BayerPattern;
    using mini_isp::CFAColor;

    Require(mini_isp::GetCFAColor(BayerPattern::RGGB, 0, 0) == CFAColor::Red,
            "RGGB top-left must be red");
    Require(mini_isp::GetCFAColor(BayerPattern::BGGR, 0, 0) == CFAColor::Blue,
            "BGGR top-left must be blue");
    Require(mini_isp::GetCFAColor(BayerPattern::GRBG, 1, 0) == CFAColor::Red,
            "GRBG top-right must be red");
    Require(mini_isp::GetCFAColor(BayerPattern::GBRG, 1, 0) == CFAColor::Blue,
            "GBRG top-right must be blue");
}

void TestBlackLevelCorrection() {
    const std::vector<uint16_t> input{100, 110, 120, 130};
    const mini_isp::BayerBlackLevel levels{10, 20, 30, 40};
    std::vector<uint16_t> output;

    mini_isp::ApplyBlackLevelCorrection16_v2(
        input, output, 2, 2, mini_isp::BayerPattern::RGGB, levels);
    Require(output == std::vector<uint16_t>({90, 90, 90, 90}),
            "Per-phase black level correction failed");
}

void TestDefectivePixelCorrection() {
    constexpr int width = 7;
    constexpr int height = 7;
    std::vector<uint16_t> input(width * height, 1000);
    input[3 * width + 3] = 4000;
    std::vector<uint16_t> output;

    mini_isp::DefectivePixelCorrection16_v1(
        input, output, width, height, mini_isp::BayerPattern::RGGB, 500);
    Require(output[3 * width + 3] == 1000,
            "Hot pixel was not replaced by the neighborhood median");

    bool threw = false;
    try {
        static_cast<void>(mini_isp::GenerateRandomHotPixel(5, 5, 2));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Impossible synthetic defect count must throw");
}

void TestOptimizedDemosaicBluePixel() {
    constexpr int width = 3;
    constexpr int height = 3;
    const std::vector<uint8_t> bayer{
        200, 100, 200,
        100,  50, 100,
        200, 100, 200
    };
    std::vector<mini_isp::RGBFormat> rgb;

    mini_isp::DemosaicByBayer_Optimal(bayer, rgb, width, height);
    const auto& center = rgb[4];
    Require(center.r == 200 && center.g == 100 && center.b == 50,
            "Blue-pixel bilinear interpolation uses the wrong neighbors");
}

void TestAdaptiveFusion() {
    constexpr int width = 2;
    constexpr int height = 2;
    const std::vector<mini_isp::RGBFormat> low(4, {0, 0, 0});
    const std::vector<mini_isp::RGBFormat> mid(4, {128, 128, 128});
    const std::vector<mini_isp::RGBFormat> high(4, {255, 255, 255});
    std::vector<mini_isp::RGBFormat> fused;

    mini_isp::AdaptiveExposureFusion(low, mid, high, fused, width, height, 1, 1);
    for (const auto& pixel : fused) {
        Require(pixel.r == 128 && pixel.g == 128 && pixel.b == 128,
                "Equal adaptive weights should average the three inputs");
    }

    bool threw = false;
    try {
        mini_isp::AdaptiveExposureFusion(low, mid, high, fused, width, height, 3, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "Grid larger than image must throw");
}

} // namespace

int main() {
    try {
        TestCfaMappings();
        TestBlackLevelCorrection();
        TestDefectivePixelCorrection();
        TestOptimizedDemosaicBluePixel();
        TestAdaptiveFusion();
        std::cout << "All miniISP tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
