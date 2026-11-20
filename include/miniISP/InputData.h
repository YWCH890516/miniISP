#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace mini_isp
{   
    enum class BayerPattern {
        RGGB,
        BGGR,
        GRBG,
        GBRG,
        Unknown
    };

    enum class CFAColor {
        Red,
        Green,
        Blue
    };

    enum class CFAPhase{
        Red,
        GreenRed,
        GreenBlue,
        Blue
    };

    struct RGBFormat{
        uint8_t r{};
        uint8_t g{};
        uint8_t b{};
    };

    struct RGBFloatFormat{
        float r{};
        float g{};
        float b{};
    };

    struct RGBFormat16{
        uint16_t r{};
        uint16_t g{};
        uint16_t b{};
    };

    struct RawImage {
        int width{};
        int height{};

        int rawWidth{};
        int rawHeight{};

        int topMargin{};
        int leftMargin{};

        int bitDepth{};

        std::uint16_t blackLevel{};
        std::uint16_t whiteLevel{};

        BayerPattern pattern{BayerPattern::Unknown};
        std::vector<uint16_t> rawFull;
        std::vector<uint16_t> visible;
    };

    struct BayerBlackLevel{
        uint16_t r{};
        uint16_t gr{};
        uint16_t gb{};
        uint16_t b{};
    };

    bool ReadPPM(const std::string& fileName, std::vector<RGBFormat>& pixels, int& width, int& height, int& maxColorValue);
    bool SavePPM(const std::string& fileName, const std::vector<RGBFormat>& pixels, int width, int height);
    void RGBToBayerRaw(const std::vector<RGBFormat> &pixels, std::vector<uint8_t> &bayerRaw, int width, int height);
    void ConvertRGB16ToRGB8(const std::vector<RGBFormat16>& input, std::vector<RGBFormat>& output, uint16_t whiteLevel);
    bool ReadDNG(const std::string& fileName, RawImage& output);
    
}
