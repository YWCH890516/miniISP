#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <tiffio.h>

namespace {

constexpr std::uint32_t kWidth = 1024;
constexpr std::uint32_t kHeight = 768;
constexpr std::uint16_t kBlackLevel = 64;
constexpr std::uint32_t kWhiteLevel = 4095;
constexpr double kCenterSignal = 3000.0;

enum class CfaPhase {
    Blue,
    GreenBlue,
    GreenRed,
    Red,
};

CfaPhase GetBggrPhase(std::uint32_t x, std::uint32_t y) {
    const bool evenX = (x & 1U) == 0U;
    const bool evenY = (y & 1U) == 0U;

    if (evenY) {
        return evenX ? CfaPhase::Blue : CfaPhase::GreenBlue;
    }
    return evenX ? CfaPhase::GreenRed : CfaPhase::Red;
}

double CornerLoss(CfaPhase phase) {
    switch (phase) {
    case CfaPhase::Red:
        return 0.50;
    case CfaPhase::GreenRed:
        return 0.43;
    case CfaPhase::GreenBlue:
        return 0.45;
    case CfaPhase::Blue:
        return 0.55;
    }
    return 0.50;
}

bool SetDngTags(TIFF* tif) {
    const std::array<std::uint8_t, 4> dngVersion{1, 4, 0, 0};
    const std::array<std::uint8_t, 4> backwardVersion{1, 3, 0, 0};
    const std::array<std::uint16_t, 2> cfaRepeat{2, 2};
    const std::array<std::uint8_t, 4> cfaPattern{2, 1, 1, 0};  // BGGR
    const std::array<std::uint8_t, 3> cfaPlaneColor{0, 1, 2};
    const std::array<std::uint16_t, 2> blackRepeat{1, 1};
    const float blackLevel = static_cast<float>(kBlackLevel);
    const std::uint32_t whiteLevel = kWhiteLevel;

    return TIFFSetField(tif, TIFFTAG_DNGVERSION, dngVersion.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_DNGBACKWARDVERSION,
                        backwardVersion.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_UNIQUECAMERAMODEL,
                        "miniISP Synthetic LSC Camera") == 1 &&
           TIFFSetField(tif, TIFFTAG_CFAREPEATPATTERNDIM,
                        cfaRepeat.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_CFAPATTERN,
                        static_cast<std::uint16_t>(cfaPattern.size()),
                        cfaPattern.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_CFAPLANECOLOR,
                        static_cast<std::uint16_t>(cfaPlaneColor.size()),
                        cfaPlaneColor.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_CFALAYOUT,
                        static_cast<std::uint16_t>(1)) == 1 &&
           TIFFSetField(tif, TIFFTAG_BLACKLEVELREPEATDIM,
                        blackRepeat.data()) == 1 &&
           TIFFSetField(tif, TIFFTAG_BLACKLEVEL,
                        static_cast<std::uint16_t>(1), &blackLevel) == 1 &&
           TIFFSetField(tif, TIFFTAG_WHITELEVEL,
                        static_cast<std::uint16_t>(1), &whiteLevel) == 1;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outputPath =
        argc > 1 ? argv[1] : "Image/lsc_flatfield_synthetic.dng";

    TIFF* tif = TIFFOpen(outputPath.c_str(), "w");
    if (tif == nullptr) {
        std::cerr << "Cannot create " << outputPath << '\n';
        return 1;
    }

    const bool baseTagsOk =
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, kWidth) == 1 &&
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, kHeight) == 1 &&
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,
                     static_cast<std::uint16_t>(16)) == 1 &&
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL,
                     static_cast<std::uint16_t>(1)) == 1 &&
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT,
                     static_cast<std::uint16_t>(SAMPLEFORMAT_UINT)) == 1 &&
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,
                     static_cast<std::uint16_t>(PHOTOMETRIC_CFA)) == 1 &&
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG,
                     static_cast<std::uint16_t>(PLANARCONFIG_CONTIG)) == 1 &&
        TIFFSetField(tif, TIFFTAG_COMPRESSION,
                     static_cast<std::uint16_t>(COMPRESSION_NONE)) == 1 &&
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP,
                     static_cast<std::uint32_t>(32)) == 1 &&
        TIFFSetField(tif, TIFFTAG_ORIENTATION,
                     static_cast<std::uint16_t>(ORIENTATION_TOPLEFT)) == 1;

    if (!baseTagsOk || !SetDngTags(tif)) {
        std::cerr << "Failed to set required DNG tags\n";
        TIFFClose(tif);
        return 1;
    }

    std::vector<std::uint16_t> row(kWidth);
    const double centerX = (static_cast<double>(kWidth) - 1.0) * 0.5;
    const double centerY = (static_cast<double>(kHeight) - 1.0) * 0.5;

    for (std::uint32_t y = 0; y < kHeight; ++y) {
        for (std::uint32_t x = 0; x < kWidth; ++x) {
            const double nx = (static_cast<double>(x) - centerX) / centerX;
            const double ny = (static_cast<double>(y) - centerY) / centerY;
            const double radiusSquared =
                std::clamp((nx * nx + ny * ny) * 0.5, 0.0, 1.0);

            const CfaPhase phase = GetBggrPhase(x, y);
            const double shading = 1.0 - CornerLoss(phase) * radiusSquared;
            const double rawValue =
                static_cast<double>(kBlackLevel) + kCenterSignal * shading;

            row[x] = static_cast<std::uint16_t>(std::clamp(
                std::lround(rawValue), 0L,
                static_cast<long>(kWhiteLevel)));
        }

        if (TIFFWriteScanline(tif, row.data(), y, 0) < 0) {
            std::cerr << "Failed while writing row " << y << '\n';
            TIFFClose(tif);
            return 1;
        }
    }

    TIFFClose(tif);
    std::cout << "Created " << outputPath << " (" << kWidth << 'x'
              << kHeight << ", BGGR, black=" << kBlackLevel
              << ", white=" << kWhiteLevel << ")\n";
    return 0;
}
