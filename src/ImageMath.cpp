#include "miniISP/ImageMath.h"

#include <cmath>

namespace mini_isp {

float SRGBToLinear(float srgb) {
    if (srgb <= 0.04045F) {
        return srgb / 12.92F;
    }

    return std::pow((srgb + 0.055F) / 1.055F, 2.4F);
}

float LinearToSRGB(float linear) {
    if (linear <= 0.0031308F) {
        return linear * 12.92F;
    }

    return 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
}
float AdobeRGBToLinear(float adobe_rgb){
    return std::pow(adobe_rgb, 2.19921875F);
}

RGBFloatFormat LinearAdobeRGBToLinearSRGB(const RGBFloatFormat& input){
    return {
        1.39835574F * input.r - 0.39835574F * input.g,
        input.g,
        -0.04292899F * input.g + 1.04292899F * input.b
    };
}
uint8_t LinearSRGBToUint8(float linear){
    const float bounded =
        std::max(0.0F, std::min(1.0F, linear));

    const float encoded = LinearToSRGB(bounded);

    return static_cast<uint8_t>(
        std::lround(encoded * 255.0F));
}
void ConvertLinearAdobeRGBToSRGB(const std::vector<RGBFloatFormat>& input, std::vector<RGBFormat>& output){
    output.resize(input.size());

    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto srgbLinear =
            LinearAdobeRGBToLinearSRGB(input[i]);

        output[i] = {
            LinearSRGBToUint8(srgbLinear.r),
            LinearSRGBToUint8(srgbLinear.g),
            LinearSRGBToUint8(srgbLinear.b)
        };
    }
}



}  // namespace mini_isp
