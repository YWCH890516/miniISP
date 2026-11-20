#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "miniISP/AutoWhiteBalance.h"
#include "miniISP/BlackLevelCorrection.h"
#include "miniISP/DefectivePixelCorrection.h"
#include "miniISP/Demosaic.h"
#include "miniISP/HighDynamicRange.h"
#include "miniISP/InputData.h"
#include "miniISP/LensShadingCorrection.h"

namespace {

struct LoadedImage {
    std::vector<mini_isp::RGBFormat> pixels;
    int width{};
    int height{};
    int maxColorValue{};
};

enum class Awb8Mode {
    None,
    GrayWorld,
    NeutralGray
};

enum class Demosaic8Mode {
    Basic,
    Pointer,
    Optimal,
    EdgeAware,
    EdgeAwareV2
};

struct RawOptions {
    bool applyBlc{true};
    bool usePhaseBlackLevels{false};
    mini_isp::BayerBlackLevel phaseBlackLevels{};

    bool applyDpc{false};
    uint16_t dpcThreshold{500};
    std::size_t injectedHotPixelCount{};

    bool applyAwb{false};

    std::string flatFieldPath;
    int lscGridWidth{17};
    int lscGridHeight{13};
    bool useNearestLsc{false};
};

bool LoadImage(const std::string& path, LoadedImage& image) {
    return mini_isp::ReadPPM(
        path, image.pixels, image.width, image.height, image.maxColorValue);
}

bool HaveMatchingGeometry(const LoadedImage& first,
                          const LoadedImage& second,
                          const LoadedImage& third) {
    return first.width == second.width && second.width == third.width &&
           first.height == second.height && second.height == third.height &&
           first.maxColorValue == second.maxColorValue &&
           second.maxColorValue == third.maxColorValue;
}

float ParseFloat(const char* text, const char* name) {
    std::size_t consumed = 0;
    const float value = std::stof(text, &consumed);
    if (text[consumed] != '\0') {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return value;
}

int ParseInt(const char* text, const char* name) {
    std::size_t consumed = 0;
    const int value = std::stoi(text, &consumed);
    if (text[consumed] != '\0') {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return value;
}

uint16_t ParseUint16(const char* text, const char* name) {
    std::size_t consumed = 0;
    const unsigned long value = std::stoul(text, &consumed);
    if (text[consumed] != '\0' || value > std::numeric_limits<uint16_t>::max()) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return static_cast<uint16_t>(value);
}

std::size_t ParseSize(const char* text, const char* name) {
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed);
    if (text[consumed] != '\0' || value > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return static_cast<std::size_t>(value);
}

const char* PatternName(mini_isp::BayerPattern pattern) {
    switch (pattern) {
    case mini_isp::BayerPattern::RGGB:
        return "RGGB";
    case mini_isp::BayerPattern::BGGR:
        return "BGGR";
    case mini_isp::BayerPattern::GRBG:
        return "GRBG";
    case mini_isp::BayerPattern::GBRG:
        return "GBRG";
    case mini_isp::BayerPattern::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

bool LoadExposureTriplet(const char* lowPath,
                         const char* midPath,
                         const char* highPath,
                         LoadedImage& low,
                         LoadedImage& mid,
                         LoadedImage& high) {
    if (!LoadImage(lowPath, low) || !LoadImage(midPath, mid) || !LoadImage(highPath, high)) {
        return false;
    }
    if (!HaveMatchingGeometry(low, mid, high)) {
        std::cerr << "Exposure images must have matching dimensions and PPM ranges.\n";
        return false;
    }
    return true;
}

void RequireResult(const std::vector<uint16_t>& pixels, const char* stage) {
    if (pixels.empty()) {
        throw std::runtime_error(std::string(stage) + " did not produce an output image.");
    }
}

uint16_t ApplyBlc(const mini_isp::RawImage& metadata,
                  const RawOptions& options,
                  const std::vector<uint16_t>& input,
                  std::vector<uint16_t>& output) {
    if (!options.applyBlc) {
        output = input;
        return metadata.whiteLevel;
    }

    if (options.usePhaseBlackLevels) {
        const auto& levels = options.phaseBlackLevels;
        const uint16_t minimumBlack = std::min({levels.r, levels.gr, levels.gb, levels.b});
        const uint16_t maximumBlack = std::max({levels.r, levels.gr, levels.gb, levels.b});
        if (metadata.whiteLevel <= maximumBlack) {
            throw std::invalid_argument("White level must be greater than every effective black level.");
        }
        mini_isp::ApplyBlackLevelCorrection16_v2(
            input, output, metadata.width, metadata.height, metadata.pattern, levels);
        RequireResult(output, "Per-phase BLC");
        return static_cast<uint16_t>(metadata.whiteLevel - minimumBlack);
    }

    if (metadata.whiteLevel <= metadata.blackLevel) {
        throw std::invalid_argument("DNG white level must be greater than its black level.");
    }
    mini_isp::ApplyBlackLevelCorrection16_v1(
        input, output, metadata.width, metadata.height, metadata.blackLevel);
    RequireResult(output, "BLC");
    return static_cast<uint16_t>(metadata.whiteLevel - metadata.blackLevel);
}

RawOptions ParseRawOptions(int argc, char* argv[], int firstOption) {
    RawOptions options;
    for (int i = firstOption; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--no-blc") {
            options.applyBlc = false;
        } else if (option == "--black-levels") {
            if (i + 4 >= argc) {
                throw std::invalid_argument("--black-levels requires R GR GB B values.");
            }
            options.usePhaseBlackLevels = true;
            options.phaseBlackLevels.r = ParseUint16(argv[++i], "R black level");
            options.phaseBlackLevels.gr = ParseUint16(argv[++i], "Gr black level");
            options.phaseBlackLevels.gb = ParseUint16(argv[++i], "Gb black level");
            options.phaseBlackLevels.b = ParseUint16(argv[++i], "B black level");
        } else if (option == "--dpc") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--dpc requires a threshold.");
            }
            options.applyDpc = true;
            options.dpcThreshold = ParseUint16(argv[++i], "DPC threshold");
        } else if (option == "--inject-hot") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--inject-hot requires a pixel count.");
            }
            options.injectedHotPixelCount = ParseSize(argv[++i], "hot-pixel count");
        } else if (option == "--flat-field") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--flat-field requires a DNG path.");
            }
            options.flatFieldPath = argv[++i];
        } else if (option == "--lsc-grid") {
            if (i + 2 >= argc) {
                throw std::invalid_argument("--lsc-grid requires width and height.");
            }
            options.lscGridWidth = ParseInt(argv[++i], "LSC grid width");
            options.lscGridHeight = ParseInt(argv[++i], "LSC grid height");
        } else if (option == "--lsc-nearest") {
            options.useNearestLsc = true;
        } else if (option == "--awb") {
            options.applyAwb = true;
        } else {
            throw std::invalid_argument("Unknown raw option: " + option);
        }
    }
    return options;
}

int RunInspectDng(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: miniisp_cli inspect-dng INPUT.dng\n";
        return 2;
    }

    mini_isp::RawImage raw;
    if (!mini_isp::ReadDNG(argv[2], raw)) {
        return 1;
    }

    std::cout
        << "Visible size : " << raw.width << " x " << raw.height << '\n'
        << "RAW size     : " << raw.rawWidth << " x " << raw.rawHeight << '\n'
        << "Margins      : top=" << raw.topMargin << ", left=" << raw.leftMargin << '\n'
        << "Bit depth    : " << raw.bitDepth << '\n'
        << "Black level  : " << raw.blackLevel << '\n'
        << "White level  : " << raw.whiteLevel << '\n'
        << "Bayer pattern: " << PatternName(raw.pattern) << '\n';
    return 0;
}

int RunRawPipeline(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: miniisp_cli raw INPUT.dng OUTPUT.ppm [options]\n";
        return 2;
    }

    const RawOptions options = ParseRawOptions(argc, argv, 4);
    mini_isp::RawImage raw;
    if (!mini_isp::ReadDNG(argv[2], raw)) {
        return 1;
    }
    if (raw.pattern == mini_isp::BayerPattern::Unknown) {
        throw std::runtime_error("The DNG does not contain a supported Bayer pattern.");
    }

    std::vector<uint16_t> current = raw.visible;

    if (options.injectedHotPixelCount > 0) {
        const auto defects = mini_isp::GenerateRandomHotPixel(
            raw.width, raw.height, options.injectedHotPixelCount);
        std::vector<uint16_t> injected;
        mini_isp::InjectSyntheticDefect16(
            current, injected, raw.width, raw.height,
            raw.blackLevel, raw.whiteLevel, defects);
        RequireResult(injected, "Synthetic defect injection");
        current.swap(injected);
    }

    if (options.applyDpc) {
        std::vector<uint16_t> corrected;
        mini_isp::DefectivePixelCorrection16_v1(
            current, corrected, raw.width, raw.height,
            raw.pattern, options.dpcThreshold);
        RequireResult(corrected, "DPC");
        current.swap(corrected);
    }

    std::vector<uint16_t> blcOutput;
    const uint16_t correctedWhiteLevel = ApplyBlc(raw, options, current, blcOutput);
    current.swap(blcOutput);

    if (!options.flatFieldPath.empty()) {
        mini_isp::RawImage flatField;
        if (!mini_isp::ReadDNG(options.flatFieldPath, flatField)) {
            return 1;
        }
        if (flatField.width != raw.width || flatField.height != raw.height ||
            flatField.pattern != raw.pattern) {
            throw std::invalid_argument(
                "Flat-field DNG must match the input dimensions and Bayer pattern.");
        }

        std::vector<uint16_t> flatFieldBlc;
        ApplyBlc(flatField, options, flatField.visible, flatFieldBlc);

        mini_isp::LensShadingCorrectionMap map;
        mini_isp::BuildLensShadingMap(
            flatFieldBlc, raw.width, raw.height, raw.pattern,
            options.lscGridWidth, options.lscGridHeight, map);
        if (map.gains.empty()) {
            throw std::runtime_error(
                "LSC map creation failed; check the flat field and grid dimensions.");
        }

        std::vector<uint16_t> lscOutput;
        if (options.useNearestLsc) {
            mini_isp::ApplyLensShadingCorrection(
                current, lscOutput, raw.width, raw.height,
                raw.pattern, correctedWhiteLevel, map);
        } else {
            mini_isp::ApplyLensShadingCorrectionV2(
                current, lscOutput, raw.width, raw.height,
                raw.pattern, correctedWhiteLevel, map);
        }
        RequireResult(lscOutput, "LSC");
        current.swap(lscOutput);
    }

    if (options.applyAwb) {
        std::vector<uint16_t> awbOutput;
        mini_isp::AWB_GrayWorldV2_16(
            current, awbOutput, raw.width, raw.height,
            raw.pattern, correctedWhiteLevel);
        RequireResult(awbOutput, "AWB");
        current.swap(awbOutput);
    }

    std::vector<mini_isp::RGBFormat16> rgb16;
    mini_isp::DemosaicBilinear16(
        current, rgb16, raw.width, raw.height,
        raw.pattern, correctedWhiteLevel);
    if (rgb16.empty()) {
        throw std::runtime_error("16-bit demosaic did not produce an output image.");
    }

    std::vector<mini_isp::RGBFormat> rgb8;
    mini_isp::ConvertRGB16ToRGB8(rgb16, rgb8, correctedWhiteLevel);
    if (!mini_isp::SavePPM(argv[3], rgb8, raw.width, raw.height)) {
        return 1;
    }

    std::cout << "Saved RAW pipeline result to " << argv[3] << '\n';
    return 0;
}

Awb8Mode ParseAwb8Mode(const std::string& name) {
    if (name == "none") {
        return Awb8Mode::None;
    }
    if (name == "gray") {
        return Awb8Mode::GrayWorld;
    }
    if (name == "neutral") {
        return Awb8Mode::NeutralGray;
    }
    throw std::invalid_argument("Unknown 8-bit AWB mode: " + name);
}

Demosaic8Mode ParseDemosaic8Mode(const std::string& name) {
    if (name == "basic") {
        return Demosaic8Mode::Basic;
    }
    if (name == "pointer") {
        return Demosaic8Mode::Pointer;
    }
    if (name == "optimal") {
        return Demosaic8Mode::Optimal;
    }
    if (name == "edge") {
        return Demosaic8Mode::EdgeAware;
    }
    if (name == "edge-v2") {
        return Demosaic8Mode::EdgeAwareV2;
    }
    throw std::invalid_argument("Unknown 8-bit demosaic mode: " + name);
}

int RunBayerDemo(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: miniisp_cli bayer-demo INPUT.ppm OUTPUT.ppm [options]\n";
        return 2;
    }

    Awb8Mode awbMode = Awb8Mode::None;
    Demosaic8Mode demosaicMode = Demosaic8Mode::Optimal;
    for (int i = 4; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--awb") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--awb requires none, gray, or neutral.");
            }
            awbMode = ParseAwb8Mode(argv[++i]);
        } else if (option == "--demosaic") {
            if (i + 1 >= argc) {
                throw std::invalid_argument(
                    "--demosaic requires basic, pointer, optimal, edge, or edge-v2.");
            }
            demosaicMode = ParseDemosaic8Mode(argv[++i]);
        } else {
            throw std::invalid_argument("Unknown bayer-demo option: " + option);
        }
    }

    LoadedImage input;
    if (!LoadImage(argv[2], input)) {
        return 1;
    }
    if (input.width < 3 || input.height < 3) {
        throw std::invalid_argument("The Bayer demo requires an image of at least 3 x 3 pixels.");
    }

    std::vector<uint8_t> bayer;
    mini_isp::RGBToBayerRaw(input.pixels, bayer, input.width, input.height);
    if (bayer.empty()) {
        throw std::runtime_error("RGB-to-Bayer conversion failed.");
    }

    if (awbMode != Awb8Mode::None) {
        std::vector<uint8_t> balanced;
        if (awbMode == Awb8Mode::GrayWorld) {
            mini_isp::AWB_GrayWorld(bayer, balanced, input.width, input.height);
        } else {
            mini_isp::AWB_GrayWorldV2(bayer, balanced, input.width, input.height);
        }
        if (balanced.empty()) {
            throw std::runtime_error("8-bit AWB did not produce an output image.");
        }
        bayer.swap(balanced);
    }

    std::vector<mini_isp::RGBFormat> output;
    switch (demosaicMode) {
    case Demosaic8Mode::Basic:
        mini_isp::DemosaicByBayer(bayer, output, input.width, input.height);
        break;
    case Demosaic8Mode::Pointer:
        mini_isp::DemosaicByBayer_Pointer(bayer, output, input.width, input.height);
        break;
    case Demosaic8Mode::Optimal:
        mini_isp::DemosaicByBayer_Optimal(bayer, output, input.width, input.height);
        break;
    case Demosaic8Mode::EdgeAware:
        mini_isp::DemosaicByBayer_EdgeAware(bayer, output, input.width, input.height);
        break;
    case Demosaic8Mode::EdgeAwareV2:
        mini_isp::DemosaicByBayer_EdgeAwareV2(bayer, output, input.width, input.height);
        break;
    }

    if (!mini_isp::SavePPM(argv[3], output, input.width, input.height)) {
        return 1;
    }
    std::cout << "Saved 8-bit RGGB Bayer demo result to " << argv[3] << '\n';
    return 0;
}

int RunRadianceHdr(int argc, char* argv[]) {
    if (argc != 9) {
        std::cerr << "Usage: miniisp_cli hdr LOW.ppm MID.ppm HIGH.ppm OUTPUT.ppm "
                     "LOW_TIME MID_TIME HIGH_TIME\n";
        return 2;
    }

    LoadedImage low;
    LoadedImage mid;
    LoadedImage high;
    if (!LoadExposureTriplet(argv[2], argv[3], argv[4], low, mid, high)) {
        return 1;
    }

    const float lowTime = ParseFloat(argv[6], "low exposure time");
    const float midTime = ParseFloat(argv[7], "middle exposure time");
    const float highTime = ParseFloat(argv[8], "high exposure time");

    std::vector<mini_isp::RGBFloatFormat> radiance;
    mini_isp::ApplyHighDynamicRange(
        low.pixels, mid.pixels, high.pixels, radiance,
        low.width, low.height, lowTime, midTime, highTime);

    std::vector<mini_isp::RGBFormat> result;
    mini_isp::ToneMapReinhard(radiance, result);
    if (!mini_isp::SavePPM(argv[5], result, low.width, low.height)) {
        return 1;
    }

    std::cout << "Saved radiance HDR result to " << argv[5] << '\n';
    return 0;
}

int RunAdaptiveFusion(int argc, char* argv[]) {
    if (argc != 6 && argc != 8) {
        std::cerr << "Usage: miniisp_cli fusion LOW.ppm MID.ppm HIGH.ppm OUTPUT.ppm "
                     "[GRID_WIDTH GRID_HEIGHT]\n";
        return 2;
    }

    LoadedImage low;
    LoadedImage mid;
    LoadedImage high;
    if (!LoadExposureTriplet(argv[2], argv[3], argv[4], low, mid, high)) {
        return 1;
    }

    const int gridWidth = argc == 8 ? ParseInt(argv[6], "grid width") : 8;
    const int gridHeight = argc == 8 ? ParseInt(argv[7], "grid height") : 8;

    std::vector<mini_isp::RGBFormat> result;
    mini_isp::AdaptiveExposureFusion(
        low.pixels, mid.pixels, high.pixels, result,
        low.width, low.height, gridWidth, gridHeight);
    if (!mini_isp::SavePPM(argv[5], result, low.width, low.height)) {
        return 1;
    }

    std::cout << "Saved adaptive fusion result to " << argv[5] << '\n';
    return 0;
}

void PrintUsage() {
    std::cout
        << "miniISP command-line tools\n\n"
        << "  miniisp_cli inspect-dng INPUT.dng\n"
        << "  miniisp_cli raw INPUT.dng OUTPUT.ppm [options]\n"
        << "  miniisp_cli bayer-demo INPUT.ppm OUTPUT.ppm [options]\n"
        << "  miniisp_cli hdr LOW.ppm MID.ppm HIGH.ppm OUTPUT.ppm "
           "LOW_TIME MID_TIME HIGH_TIME\n"
        << "  miniisp_cli fusion LOW.ppm MID.ppm HIGH.ppm OUTPUT.ppm "
           "[GRID_WIDTH GRID_HEIGHT]\n\n"
        << "RAW options:\n"
        << "  --no-blc                   Skip black-level correction\n"
        << "  --black-levels R GR GB B   Use four CFA-phase black levels\n"
        << "  --dpc THRESHOLD            Enable defective-pixel correction\n"
        << "  --inject-hot COUNT         Inject deterministic synthetic hot pixels\n"
        << "  --flat-field FILE.dng      Build and apply an LSC map\n"
        << "  --lsc-grid WIDTH HEIGHT    LSC grid size (default 17 x 13)\n"
        << "  --lsc-nearest              Use V1 cell gains instead of bilinear V2\n"
        << "  --awb                      Enable 16-bit neutral gray-world AWB\n\n"
        << "Bayer-demo options (the simulated CFA is RGGB):\n"
        << "  --awb none|gray|neutral\n"
        << "  --demosaic basic|pointer|optimal|edge|edge-v2\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            PrintUsage();
            return 0;
        }

        const std::string mode = argv[1];
        if (mode == "inspect-dng") {
            return RunInspectDng(argc, argv);
        }
        if (mode == "raw") {
            return RunRawPipeline(argc, argv);
        }
        if (mode == "bayer-demo") {
            return RunBayerDemo(argc, argv);
        }
        if (mode == "hdr") {
            return RunRadianceHdr(argc, argv);
        }
        if (mode == "fusion") {
            return RunAdaptiveFusion(argc, argv);
        }
        if (mode == "help" || mode == "--help" || mode == "-h") {
            PrintUsage();
            return 0;
        }

        std::cerr << "Unknown mode: " << mode << '\n';
        PrintUsage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
