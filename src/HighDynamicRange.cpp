#include "miniISP/HighDynamicRange.h"
#include "miniISP/ImageMath.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace mini_isp{

    uint8_t ClampToUint8(float value) {
        return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, std::round(value * 255.0f))));
    }

    uint8_t ClampCodeValueToUint8(float value) {
        return static_cast<uint8_t>(std::round(std::max(0.0f, std::min(255.0f, value))));
    }

    // weighted calcuation 
    float CalculateWeight(uint8_t value){
        const float normalizedValue = static_cast<float>(value) / 255.0f;
        
        return 1.0f - std::abs(normalizedValue - 0.5f) * 2.0f; // Weight is highest at mid-range and decreases towards the extremes
    }

    
    void ApplyHighDynamicRange(const std::vector<RGBFormat>& lowExposure, const std::vector<RGBFormat>& midExposure, const std::vector<RGBFormat>& highExposure, std::vector<RGBFloatFormat>& hdrImage, int width, int height, float lowExposureTime, float midExposureTime, float highExposureTime) {
        if (lowExposure.size() != midExposure.size() || midExposure.size() != highExposure.size()) {
            throw std::invalid_argument("Input images must have the same size.");
        }

        if(lowExposureTime <= 0 || midExposureTime <= 0 || highExposureTime <= 0) {
            throw std::invalid_argument("Exposure times must be positive.");
        }

        if(lowExposure.empty() || width <= 0 || height <= 0) {
            throw std::invalid_argument("Input images must not be empty and dimensions must be positive.");
        }

        const std::size_t expectedSize =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height);

        if (lowExposure.size() != expectedSize)
        {
            throw std::invalid_argument(
                "Image size does not match dimensions.");
        }

        if (!(lowExposureTime < midExposureTime &&
              midExposureTime < highExposureTime))
        {
            throw std::invalid_argument(
                "Exposure times must be in increasing order.");
        }

        hdrImage.resize(lowExposure.size());
        for (size_t i = 0; i < lowExposure.size(); ++i) {

            const float lowRNormalized = static_cast<float>(lowExposure[i].r) / 255.0f;
            const float lowGNormalized = static_cast<float>(lowExposure[i].g) / 255.0f;
            const float lowBNormalized = static_cast<float>(lowExposure[i].b) / 255.0f;

            const float lowRLinear = SRGBToLinear(lowRNormalized);
            const float lowGLinear = SRGBToLinear(lowGNormalized);
            const float lowBLinear = SRGBToLinear(lowBNormalized);


            const float midRNormalized = static_cast<float>(midExposure[i].r) / 255.0f;
            const float midGNormalized = static_cast<float>(midExposure[i].g) / 255.0f;
            const float midBNormalized = static_cast<float>(midExposure[i].b) / 255.0f; 

            const float midRLinear = SRGBToLinear(midRNormalized);
            const float midGLinear = SRGBToLinear(midGNormalized);
            const float midBLinear = SRGBToLinear(midBNormalized);

            const float highRNormalized = static_cast<float>(highExposure[i].r) / 255.0f;
            const float highGNormalized = static_cast<float>(highExposure[i].g) / 255.0f;
            const float highBNormalized = static_cast<float>(highExposure[i].b) / 255.0f;       

            const float highRLinear = SRGBToLinear(highRNormalized);
            const float highGLinear = SRGBToLinear(highGNormalized);    
            const float highBLinear = SRGBToLinear(highBNormalized);

            // use the exposure times to scale the linear values
            const float lowRScaled = lowRLinear / lowExposureTime;
            const float lowGScaled = lowGLinear / lowExposureTime;
            const float lowBScaled = lowBLinear / lowExposureTime;

            const float midRScaled = midRLinear / midExposureTime;
            const float midGScaled = midGLinear / midExposureTime;
            const float midBScaled = midBLinear / midExposureTime;

            const float highRScaled = highRLinear / highExposureTime;   
            const float highGScaled = highGLinear / highExposureTime;
            const float highBScaled = highBLinear / highExposureTime;

            // Calculate weights based on the mid-range values
            const float lowRWeight = CalculateWeight(lowExposure[i].r);
            const float lowGWeight = CalculateWeight(lowExposure[i].g);
            const float lowBWeight = CalculateWeight(lowExposure[i].b);

            const float midRWeight = CalculateWeight(midExposure[i].r);
            const float midGWeight = CalculateWeight(midExposure[i].g);
            const float midBWeight = CalculateWeight(midExposure[i].b);

            const float highRWeight = CalculateWeight(highExposure[i].r);
            const float highGWeight = CalculateWeight(highExposure[i].g);
            const float highBWeight = CalculateWeight(highExposure[i].b);

            const float totalRWeight = lowRWeight + midRWeight + highRWeight;
            const float totalGWeight = lowGWeight + midGWeight + highGWeight;
            const float totalBWeight = lowBWeight + midBWeight + highBWeight;


            // // test 
            // hdrImage[i].r = midRScaled;
            // hdrImage[i].g = midGScaled;
            // hdrImage[i].b = midBScaled;


            if(totalRWeight > 1e-6f) {
                hdrImage[i].r = (lowRScaled * lowRWeight + midRScaled * midRWeight + highRScaled * highRWeight) / totalRWeight;
            } else {
                hdrImage[i].r = midRScaled; // If all weights are zero, use the mid exposure value as a fallback
            }

            if(totalGWeight > 1e-6f) {
                hdrImage[i].g = (lowGScaled * lowGWeight + midGScaled * midGWeight + highGScaled * highGWeight) / totalGWeight;
            } else {
                hdrImage[i].g = midGScaled; // If all weights are zero, use the mid exposure value as a fallback
            }

            if(totalBWeight > 1e-6f) {
                hdrImage[i].b = (lowBScaled * lowBWeight + midBScaled * midBWeight + highBScaled * highBWeight) / totalBWeight;
            } else {
                hdrImage[i].b = midBScaled; // If all weights are zero, use the mid exposure value as a fallback
            }



        }
    }

    void ToneMapReinhard(const std::vector<RGBFloatFormat>& hdrImage, std::vector<RGBFormat>& ldrImage, float key) {
        if (hdrImage.empty()) {
            throw std::invalid_argument("HDR image must not be empty.");
        }

        if(key<=0.0f){
            throw std::invalid_argument("Key value must be positive.");
        }

        constexpr float epsilon = 1e-6f; // Small value to avoid division by zero
        double logSum = 0.0;

        for(const RGBFloatFormat& pixel : hdrImage) {
            float luminance = 0.2126f * pixel.r + 0.7152f * pixel.g + 0.0722f * pixel.b;
            logSum += std::log(epsilon + luminance);
        }
        const float logAverageLuminance = static_cast<float>(std::exp(logSum / static_cast<double>(hdrImage.size())));
        
        const float scale = key / logAverageLuminance;

        ldrImage.resize(hdrImage.size());
        for(size_t i = 0; i < hdrImage.size(); ++i) {
            const RGBFloatFormat& hdrPixel = hdrImage[i];

            float luminance = 0.2126f * hdrPixel.r + 0.7152f * hdrPixel.g + 0.0722f * hdrPixel.b;

            if(luminance <= 0.0f) {
                ldrImage[i] = {0, 0, 0};
                continue;
            }

            // Reinhard tone mapping (L / (1 + L))
            float mappedLuminance = (luminance * scale) / (1.0f + luminance * scale);
            
            const float luminanceRatio = mappedLuminance / luminance;

            const float mappedR = hdrPixel.r * luminanceRatio;
            const float mappedG = hdrPixel.g * luminanceRatio;
            const float mappedB = hdrPixel.b * luminanceRatio;

            // Linear to sRGB conversion and clamping to [0, 255]
            ldrImage[i].r = ClampToUint8(LinearToSRGB(mappedR));
            ldrImage[i].g = ClampToUint8(LinearToSRGB(mappedG));
            ldrImage[i].b = ClampToUint8(LinearToSRGB(mappedB));

        }

        
    }

    float CalculateIntensity(const RGBFormat& pixel){
        return (0.299f * pixel.r + 0.587f * pixel.g + 0.114f * pixel.b) / 255.0f;
    }

    float CalculateExposureConfidence(float meanIntensity, float sigma = 0.2f){
        if (sigma <= 0.0f) {
            throw std::invalid_argument("Exposure confidence sigma must be positive.");
        }
        // use a Gaussian function to calculate the confidence of the exposure based on the mean intensity
        // the confidence is highest when the mean intensity is around 0.5 and decreases as it moves away from 0.5
        const float diff = meanIntensity - 0.5f;
        return std::exp(- (diff * diff) / (2.0f * sigma * sigma));
    }

    void AdaptiveExposureFusion(const std::vector<RGBFormat>& lowExposure, const std::vector<RGBFormat>& midExposure, const std::vector<RGBFormat>& highExposure, std::vector<RGBFormat>& fusedImage, int width, int height, int gridWidth, int gridHeight) {
        // Adaptive exposure fusion: each grid cell combines local contrast
        // with a mid-tone exposure confidence term.

        if (lowExposure.size() != midExposure.size() || midExposure.size() != highExposure.size()) {
            throw std::invalid_argument("Input images must have the same size.");
        }

        if(lowExposure.empty() || width <= 0 || height <= 0) {
            throw std::invalid_argument("Input images must not be empty and dimensions must be positive.");
        }

        if (gridWidth <= 0 || gridHeight <= 0 || gridWidth > width || gridHeight > height) {
            throw std::invalid_argument("Grid dimensions must be positive and no larger than the image.");
        }

        const std::size_t expectedSize =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height);

        if (lowExposure.size() != expectedSize)
        {
            throw std::invalid_argument(
                "Image size does not match dimensions.");
        }



        fusedImage.resize(lowExposure.size());

        // Store standard deviation and normalized weight maps for every grid cell.
        std::vector<std::vector<float>> lowExposureStdDev(gridHeight, std::vector<float>(gridWidth, 0.0f));
        std::vector<std::vector<float>> midExposureStdDev(gridHeight, std::vector<float>(gridWidth, 0.0f));
        std::vector<std::vector<float>> highExposureStdDev(gridHeight, std::vector<float>(gridWidth, 0.0f));
        std::vector<std::vector<float>> lowWeight(gridHeight, std::vector<float>(gridWidth, 0.0f));
        std::vector<std::vector<float>> midWeight(gridHeight, std::vector<float>(gridWidth, 0.0f));
        std::vector<std::vector<float>> highWeight(gridHeight, std::vector<float>(gridWidth, 0.0f));    


        for(int gridY = 0; gridY < gridHeight; ++gridY){
            const int startY = gridY * height / gridHeight;
            int endY = (gridY + 1) * height / gridHeight;
            for(int gridX = 0; gridX < gridWidth; ++gridX){
                // Calculate the standard deviation for each exposure in the current grid block
                const int startX = gridX * width / gridWidth ;
                const int endX = (gridX + 1) * width / gridWidth;

                // Calculate the mean intensity for each exposure in the current grid block
                double lowExposureMean = 0.0f;
                double midExposureMean = 0.0f;
                double highExposureMean = 0.0f;
                int pixelCount = 0;
                for(int y = startY ; y < endY ; ++y){
                    for(int x = startX ; x < endX ; ++x){
                        const int index = y * width + x;
                        lowExposureMean += CalculateIntensity(lowExposure[index]);
                        midExposureMean += CalculateIntensity(midExposure[index]);
                        highExposureMean += CalculateIntensity(highExposure[index]);

                        ++pixelCount;
                    }
                }

                const float lowMean = static_cast<float>(lowExposureMean / pixelCount);
                const float midMean = static_cast<float>(midExposureMean / pixelCount);
                const float highMean = static_cast<float>(highExposureMean / pixelCount);

                // second pass to calculate the variance and standard deviation
                double lowExposureVariance = 0.0f;
                double midExposureVariance = 0.0f;
                double highExposureVariance = 0.0f;

                for(int y = startY ; y < endY ; ++y){
                    for(int x = startX ; x < endX ;++x){
                        const int index = y * width +x;
                        const float lowIntensity = CalculateIntensity(lowExposure[index]);
                        const float midIntensity = CalculateIntensity(midExposure[index]);
                        const float highIntensity = CalculateIntensity(highExposure[index]);

                        lowExposureVariance += (lowIntensity - lowMean) * (lowIntensity - lowMean);
                        midExposureVariance += (midIntensity - midMean) * (midIntensity - midMean);
                        highExposureVariance += (highIntensity - highMean) * (highIntensity - highMean);
                    }
                }

                lowExposureStdDev[gridY][gridX] = std::sqrt(lowExposureVariance / pixelCount);
                midExposureStdDev[gridY][gridX] = std::sqrt(midExposureVariance / pixelCount);
                highExposureStdDev[gridY][gridX] = std::sqrt(highExposureVariance / pixelCount);
                
                constexpr float epsilon = 1e-6f;
                
                const float lowConfidence = CalculateExposureConfidence(lowMean);
                const float midConfidence = CalculateExposureConfidence(midMean);
                const float highConfidence = CalculateExposureConfidence(highMean);

                const float lowRawWeight = lowExposureStdDev[gridY][gridX] * lowConfidence + epsilon;
                const float midRawWeight = midExposureStdDev[gridY][gridX] * midConfidence + epsilon;
                const float highRawWeight = highExposureStdDev[gridY][gridX] * highConfidence + epsilon;
                
                const float totalWeight = lowRawWeight + midRawWeight + highRawWeight;
                lowWeight[gridY][gridX] = lowRawWeight/ totalWeight;
                midWeight[gridY][gridX] = midRawWeight / totalWeight;
                highWeight[gridY][gridX] = highRawWeight / totalWeight;

            }
        }


        // Use bilinear interpolation to smooth the weights across the grid blocks and avoid seams
        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                const std::size_t idx = static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(width) +
                                        static_cast<std::size_t>(x);
                float mapX = static_cast<float>(x+0.5f) * gridWidth / width - 0.5f;
                mapX = std::max(0.0f, std::min(mapX, static_cast<float>(gridWidth - 1)));
                float mapY = static_cast<float>(y+0.5f) * gridHeight / height - 0.5f ;
                mapY = std::max(0.0f, std::min(mapY, static_cast<float>(gridHeight - 1)));
                const int x0 = static_cast<int>(std::floor(mapX));
                const int y0 = static_cast<int>(std::floor(mapY));
                const int x1 = std::min(x0 + 1, gridWidth - 1);
                const int y1 = std::min(y0 + 1, gridHeight - 1);
                const float dx = mapX - x0;
                const float dy = mapY - y0;


                float gainTopLow = lowWeight[y0][x0] * (1.0f - dx) + lowWeight[y0][x1] * dx;
                float gainBottomLow = lowWeight[y1][x0] * (1.0f - dx) + lowWeight[y1][x1] * dx;
                float gainLow = gainTopLow * (1.0f - dy) + gainBottomLow * dy;

                float gainTopMid = midWeight[y0][x0] * (1.0f - dx) + midWeight[y0][x1] * dx;
                float gainBottomMid = midWeight[y1][x0] * (1.0f - dx) + midWeight[y1][x1] * dx;
                float gainMid = gainTopMid * (1.0f - dy) + gainBottomMid * dy;

                float gainTopHigh = highWeight[y0][x0] * (1.0f - dx) + highWeight[y0][x1] * dx;
                float gainBottomHigh = highWeight[y1][x0] * (1.0f - dx) + highWeight[y1][x1] * dx;
                float gainHigh = gainTopHigh * (1.0f - dy) + gainBottomHigh * dy;
                
                // Normalize the gains to ensure they sum to 1.0
                const float totalGain = gainLow + gainMid + gainHigh;
                if (totalGain <= 1e-6f) {
                    fusedImage[idx] = midExposure[idx];
                    continue;
                }
                gainLow /= totalGain;
                gainMid /= totalGain;
                gainHigh /= totalGain;


                fusedImage[idx].r = ClampCodeValueToUint8(
                    lowExposure[idx].r * gainLow + midExposure[idx].r * gainMid + highExposure[idx].r * gainHigh);
                fusedImage[idx].g = ClampCodeValueToUint8(
                    lowExposure[idx].g * gainLow + midExposure[idx].g * gainMid + highExposure[idx].g * gainHigh);
                fusedImage[idx].b = ClampCodeValueToUint8(
                    lowExposure[idx].b * gainLow + midExposure[idx].b * gainMid + highExposure[idx].b * gainHigh);
            }
        }
    }

    
}
