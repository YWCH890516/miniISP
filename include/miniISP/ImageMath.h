#pragma once
#include "miniISP/ColorCalibration.h" 

namespace mini_isp {

float SRGBToLinear(float srgb);
float LinearToSRGB(float linear);
float AdobeRGBToLinear(float adobe_rgb);

RGBFloatFormat LinearAdobeRGBToLinearSRGB(const RGBFloatFormat& input);
uint8_t LinearSRGBToUint8(float linear);
void ConvertLinearAdobeRGBToSRGB(const std::vector<RGBFloatFormat>& input, std::vector<RGBFormat>& output);


}  // namespace mini_isp
