#include "miniISP/InputData.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <libraw/libraw.h>
#include <limits>
#include <string>

namespace {

bool ReadPpmToken(std::istream& input, std::string& token) {
    while (input >> std::ws && input.peek() == '#') {
        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return static_cast<bool>(input >> token);
}

} // namespace

namespace mini_isp{
    bool ReadPPM(const std::string& fileName, std::vector<RGBFormat>& pixels, int& width, int& height, int& maxColorValue) {
        pixels.clear();
        width = 0;
        height = 0;
        maxColorValue = 0;

        std::ifstream inputImage(fileName, std::ios::binary);
        if (!inputImage) {
            std::cerr << "Cannot open PPM file: " << fileName << '\n';
            return false;
        }

        std::string magic;
        std::string widthToken;
        std::string heightToken;
        std::string maxValueToken;
        if (!ReadPpmToken(inputImage, magic) ||
            !ReadPpmToken(inputImage, widthToken) ||
            !ReadPpmToken(inputImage, heightToken) ||
            !ReadPpmToken(inputImage, maxValueToken)) {
            std::cerr << "Invalid PPM header: " << fileName << '\n';
            return false;
        }

        try {
            width = std::stoi(widthToken);
            height = std::stoi(heightToken);
            maxColorValue = std::stoi(maxValueToken);
        } catch (const std::exception&) {
            std::cerr << "Invalid PPM dimensions: " << fileName << '\n';
            width = height = maxColorValue = 0;
            return false;
        }

        if (magic != "P6" || width <= 0 || height <= 0 || maxColorValue != 255) {
            std::cerr << "Only 8-bit binary P6 PPM is supported: " << fileName << '\n';
            width = height = maxColorValue = 0;
            return false;
        }

        char separator = '\0';
        if (!inputImage.get(separator) || !std::isspace(static_cast<unsigned char>(separator))) {
            std::cerr << "Missing PPM header separator: " << fileName << '\n';
            width = height = maxColorValue = 0;
            return false;
        }
        if (separator == '\r' && inputImage.peek() == '\n') {
            inputImage.get();
        }

        const std::size_t pixelCount = static_cast<std::size_t>(width) *
                                       static_cast<std::size_t>(height);
        std::vector<uint8_t> bytes(pixelCount * 3U);
        inputImage.read(reinterpret_cast<char*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
        if (inputImage.gcount() != static_cast<std::streamsize>(bytes.size())) {
            std::cerr << "PPM pixel data is incomplete: " << fileName << '\n';
            pixels.clear();
            width = height = maxColorValue = 0;
            return false;
        }

        pixels.resize(pixelCount);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            pixels[i] = {bytes[i * 3U], bytes[i * 3U + 1U], bytes[i * 3U + 2U]};
        }
        return true;
    }

    bool SavePPM(const std::string& fileName, const std::vector<RGBFormat>& pixels, int width, int height){
        if (width <= 0 || height <= 0 ||
            pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            std::cerr << "Invalid PPM dimensions or pixel count\n";
            return false;
        }

        std::ofstream outputImage(fileName, std::ios::binary);
        if(!outputImage){
            std::cerr << "Cannot open PPM file for writing: " << fileName << '\n';
            return false;
        }

        outputImage << "P6\n" << width << " " << height << "\n255\n";

        for(const auto& pixel : pixels){
            outputImage.write(reinterpret_cast<const char*>(&pixel.r), 1);
            outputImage.write(reinterpret_cast<const char*>(&pixel.g), 1);
            outputImage.write(reinterpret_cast<const char*>(&pixel.b), 1);
        }
        return static_cast<bool>(outputImage);
    }

    void RGBToBayerRaw(const std::vector<RGBFormat>& pixels, std::vector<uint8_t>& bayerRaw, int width, int height){
        if(pixels.empty() || width <=0 || height <=0){
            std::cout << "Invalid input data" << std::endl;
            return;
        }

        if(pixels.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)){
            std::cout << "Pixel data size does not match width and height" << std::endl;
            return;
        }

        bayerRaw.resize(width* height);

        const RGBFormat* srcPtr = pixels.data();
        uint8_t* dstPtr = bayerRaw.data();

        for(int y = 0 ; y < height ; ++y){
            const RGBFormat* srcRow = srcPtr + y * width;
            uint8_t* bayerRowPtr = dstPtr + y * width;
            for(int x = 0 ; x < width ; ++x){
                const RGBFormat& pixel = srcRow[x];
                if(y % 2 == 0){
                    if(x % 2 == 0){
                        bayerRowPtr[x] = pixel.r; // R
                    } else {
                        bayerRowPtr[x] = pixel.g; // G
                    }
                } else {
                    if(x % 2 == 0){
                        bayerRowPtr[x] = pixel.g; // G
                    } else {
                        bayerRowPtr[x] = pixel.b; // B
                    }
                }
            }
        }

    }

    // Convert 16bit ---> 8bit
     void ConvertRGB16ToRGB8(const std::vector<RGBFormat16>& input, std::vector<RGBFormat>& output, uint16_t whiteLevel){

        output.clear();

        if(input.empty() || whiteLevel == 0) return ;

        output.resize(input.size());

        auto convertChannel = [whiteLevel](uint16_t value){
            const auto cliped = std::min(value, whiteLevel);

            const uint32_t scaled = static_cast<uint32_t>(cliped)*255U / static_cast<uint32_t>(whiteLevel);

            return static_cast<uint8_t>(scaled);
        };

        for(size_t i = 0 ; i < input.size() ; ++i){
            output[i].r = convertChannel(input[i].r);
            output[i].g = convertChannel(input[i].g);
            output[i].b = convertChannel(input[i].b);
        }

     }



    // ---------- Read .DNG ------------

    bool ReadDNG(const std::string& fileName, RawImage& output){
        LibRaw processor;

        output = {};

        // 1.Open RAW/DNG file and raw metadata
        int ret = processor.open_file(fileName.c_str());

        if (ret != LIBRAW_SUCCESS) {
            std::cerr<< "LibRaw open_file failed: "<< libraw_strerror(ret)<< '\n';
            return false;
        }
        // 2.Decode
        ret = processor.unpack();
        if (ret != LIBRAW_SUCCESS) {
            std::cerr<< "LibRaw unpack failed: "<< libraw_strerror(ret)<< '\n';

            return false;
        }


        // 3. Check whether this is a Bayer-style uint16 RAW buffer
        uint16_t* raw = processor.imgdata.rawdata.raw_image;

        if (raw == nullptr) {
            std::cerr
                << "No Bayer raw_image buffer found.\n";
            return false;
        }

        const auto& sizes = processor.imgdata.sizes;
        const auto& color = processor.imgdata.color;

        // Visible image size
        output.width = sizes.width;
        output.height = sizes.height;

        // Full RAW buffer size
        output.rawWidth = sizes.raw_width;
        output.rawHeight = sizes.raw_height;

        const std::size_t rawStride = sizes.raw_pitch > 0
            ? static_cast<std::size_t>(sizes.raw_pitch) / sizeof(uint16_t)
            : static_cast<std::size_t>(sizes.raw_width);

        if (rawStride < static_cast<std::size_t>(output.rawWidth) ||
            sizes.left_margin + output.width > output.rawWidth ||
            sizes.top_margin + output.height > output.rawHeight) {
            std::cerr << "Invalid RAW dimensions or row pitch\n";
            output = {};
            return false;
        }

        output.whiteLevel =
            static_cast<uint16_t>(color.maximum);

        // Simple first version:
        // LibRaw may provide more detailed per-channel black levels.
        output.blackLevel =
            static_cast<uint16_t>(color.black);

        // Estimate bit depth from white level.
        // Example:
        // whiteLevel = 4095 -> 12 bit
        // whiteLevel = 16383 -> 14 bit
        uint32_t maxValue = output.whiteLevel;

        while (maxValue > 0) {
            ++output.bitDepth;
            maxValue >>= 1;
        }

        // 4. Copy only the visible image area.
        //
        // raw_image layout:
        // full RAW sensor buffer
        //
        // Visible image begins at:
        // top_margin, left_margin

        output.visible.resize(
            static_cast<size_t>(output.width) *
            static_cast<size_t>(output.height)
        );

        for (int y = 0; y < output.height; ++y) {
            for (int x = 0; x < output.width; ++x) {

                const int rawY = y + sizes.top_margin;
                const int rawX = x + sizes.left_margin;

                const size_t srcIdx =
                    static_cast<size_t>(rawY) *
                    rawStride +
                    rawX;

                const size_t dstIdx =
                    static_cast<size_t>(y) *
                    output.width +
                    x;

                output.visible[dstIdx] = raw[srcIdx];
            }
        }

        // Copy Raw Data
        output.topMargin = sizes.top_margin;
        output.leftMargin = sizes.left_margin;
        
        output.rawFull.resize(static_cast<std::size_t>(output.rawWidth) *
                              static_cast<std::size_t>(output.rawHeight));
        for(int y = 0 ; y < output.rawHeight ; ++y){
            for(int x = 0 ; x < output.rawWidth ; ++x){
                const std::size_t dstIdx = static_cast<std::size_t>(y) *
                                           static_cast<std::size_t>(output.rawWidth) +
                                           static_cast<std::size_t>(x);
                const std::size_t srcIdx = static_cast<std::size_t>(y) * rawStride +
                                           static_cast<std::size_t>(x);
                output.rawFull[dstIdx] = raw[srcIdx];
            }
        }

        // 5. Detect Bayer pattern.
        //
        // COLOR(row, col) returns channel index:
        // 0 = R
        // 1 = G
        // 2 = B
        // 3 = second G (depending on RAW/CFA)
        //
        // We inspect one 2x2 CFA cell.

        const int c00 = processor.COLOR(
            sizes.top_margin,
            sizes.left_margin
        );

        const int c01 = processor.COLOR(
            sizes.top_margin,
            sizes.left_margin + 1
        );

        const int c10 = processor.COLOR(
            sizes.top_margin + 1,
            sizes.left_margin
        );

        const int c11 = processor.COLOR(
            sizes.top_margin + 1,
            sizes.left_margin + 1
        );

        // Treat both G indexes as green.
        auto isGreen = [](int c) {
            return c == 1 || c == 3;
        };

        if (c00 == 0 &&
            isGreen(c01) &&
            isGreen(c10) &&
            c11 == 2)
        {
            output.pattern = BayerPattern::RGGB;
        }
        else if (c00 == 2 &&
                isGreen(c01) &&
                isGreen(c10) &&
                c11 == 0)
        {
            output.pattern = BayerPattern::BGGR;
        }
        else if (isGreen(c00) &&
                c01 == 0 &&
                c10 == 2 &&
                isGreen(c11))
        {
            output.pattern = BayerPattern::GRBG;
        }
        else if (isGreen(c00) &&
                c01 == 2 &&
                c10 == 0 &&
                isGreen(c11))
        {
            output.pattern = BayerPattern::GBRG;
        }
        else {
            output.pattern = BayerPattern::Unknown;
        }

        return true;


    }

}
