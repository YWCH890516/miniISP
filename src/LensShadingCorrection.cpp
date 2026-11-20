#include "miniISP/LensShadingCorrection.h"
#include "miniISP/InputData.h"
#include "miniISP/BayerUtils.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace mini_isp{

    struct PhaseMean{
        double rMean{};
        double grMean{};
        double gbMean{};
        double bMean{};
    };

    void BuildLensShadingMap(const std::vector<uint16_t>& flatFieldImg, int width, int height, BayerPattern pattern, int gridWidth, int gridHeight, LensShadingCorrectionMap& lscMap){
        // Implementation of building lens shading map

        lscMap = {};

        if(width <= 0 || height <= 0 || gridWidth <= 0 || gridHeight <= 0 ||
           gridWidth > width / 2 || gridHeight > height / 2){
            std::cerr << "Invalid dimensions for lens shading map" << std::endl;
            return;
        }
        const auto expectedSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if(flatFieldImg.size() != expectedSize){
            std::cerr << "Flat field image size does not match width and height" << std::endl;
            return;
        }

        if(pattern == BayerPattern::Unknown){
            std::cerr << "Unknown Bayer pattern" << std::endl;
            return;
        }


        lscMap.gridWidth = gridWidth;
        lscMap.gridHeight = gridHeight;
        lscMap.gains.assign(gridHeight, std::vector<LSCGain>(gridWidth));
        std::vector<std::vector<PhaseMean>> phaseMeans(gridHeight, std::vector<PhaseMean>(gridWidth));

        for(int gridY = 0 ; gridY < gridHeight ; ++gridY){
            const int yStart = gridY * height / gridHeight;
            const int yEnd = (gridY + 1) * height / gridHeight;
            for(int gridX = 0 ; gridX < gridWidth ; ++gridX){
                const int xStart = gridX * width / gridWidth;
                const int xEnd = (gridX + 1) * width / gridWidth;

                uint64_t rSum = 0, grSum = 0, gbSum = 0, bSum = 0;
                std::size_t rCount = 0, grCount = 0, gbCount = 0, bCount = 0;

                for(int y = yStart ; y < yEnd ; ++y){
                    for(int x = xStart ; x < xEnd ; ++x){
                        const auto index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                        const uint16_t pixelValue = flatFieldImg[index];

                        switch(GetCFAPhase(pattern, x, y)){
                            case CFAPhase::Red:
                                rSum += pixelValue;
                                ++rCount;
                                break;
                            case CFAPhase::GreenRed:
                                grSum += pixelValue;
                                ++grCount;
                                break;
                            case CFAPhase::GreenBlue:
                                gbSum += pixelValue;
                                ++gbCount;
                                break;
                            case CFAPhase::Blue:
                                bSum += pixelValue;
                                ++bCount;
                                break;
                        }
                    }
                }

                if (rCount == 0 || grCount == 0 ||
                    gbCount == 0 || bCount == 0){
                    lscMap = {};
                    return;
                }

                PhaseMean& mean = phaseMeans[gridY][gridX];
                mean.rMean = static_cast<double>(rSum) / rCount;
                mean.grMean = static_cast<double>(grSum) / grCount;
                mean.gbMean = static_cast<double>(gbSum) / gbCount;
                mean.bMean = static_cast<double>(bSum) / bCount;
            }
        }

        const PhaseMean& centerMean = phaseMeans[gridHeight / 2][gridWidth / 2];
        std::vector<std::vector<LSCGain>>& gains = lscMap.gains;
        for(int gridY = 0 ; gridY < gridHeight ; ++gridY){
            for(int gridX = 0 ; gridX < gridWidth ; ++gridX){
                const PhaseMean& mean = phaseMeans[gridY][gridX];
                if (mean.rMean <= 0.0 ||
                    mean.grMean <= 0.0 ||
                    mean.gbMean <= 0.0 ||
                    mean.bMean <= 0.0)
                {
                    lscMap = {};
                    return;
                }
                LSCGain& gain = gains[gridY][gridX];
                gain.rGain = static_cast<float>(centerMean.rMean / mean.rMean);
                gain.grGain = static_cast<float>(centerMean.grMean / mean.grMean);
                gain.gbGain = static_cast<float>(centerMean.gbMean / mean.gbMean);
                gain.bGain = static_cast<float>(centerMean.bMean / mean.bMean); 
            }
        }

    }

    void ApplyLensShadingCorrection(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t whiteLevel ,const LensShadingCorrectionMap& lscMap){
        // Implementation of applying lens shading correction
        output.clear();

        if (width <= 0 || height <= 0 || whiteLevel == 0)
        {
            return;
        }

        if (lscMap.gridWidth <= 0 || lscMap.gridHeight <= 0)
        {
            return;
        }

        if (lscMap.gains.size() !=
            static_cast<std::size_t>(lscMap.gridHeight))
        {
            return;
        }

        for (const auto &row : lscMap.gains)
        {
            if (row.size() !=
                static_cast<std::size_t>(lscMap.gridWidth))
            {
                return;
            }
        }
        const auto expectedSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if(input.size() != expectedSize) return ;

        if(pattern == BayerPattern::Unknown){
            std::cerr << "Unknown Bayer pattern" << std::endl;
            return;
        }

        output.resize(input.size());

        for(int y = 0 ; y < height ; ++y){
            const int gridY = static_cast<int>(static_cast<double>(y) * lscMap.gridHeight / height);
            for(int x = 0 ; x < width ; ++x){
                const int gridX = static_cast<int>(static_cast<double>(x) * lscMap.gridWidth / width);
                const LSCGain& gain = lscMap.gains[gridY][gridX];
                const auto index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const uint16_t pixelValue = input[index];
                double correctedValue = pixelValue;


                switch(GetCFAPhase(pattern, x, y)){
                    case CFAPhase::Red:
                        correctedValue *= gain.rGain;
                        break;
                    case CFAPhase::GreenRed:
                        correctedValue *= gain.grGain;
                        break;
                    case CFAPhase::GreenBlue:
                        correctedValue *= gain.gbGain;
                        break;
                    case CFAPhase::Blue:
                        correctedValue *= gain.bGain;
                        break;
                }
                output[index] = static_cast<uint16_t>(std::min(correctedValue, static_cast<double>(whiteLevel)));
            }
        }

    }

    void ApplyLensShadingCorrectionV2(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t whiteLevel ,const LensShadingCorrectionMap& lscMap){
        // Implementation of applying lens shading correction with bilinear interpolation
        output.clear();

        if (width <= 0 || height <= 0 || whiteLevel == 0)
        {
            return;
        }

        if (lscMap.gridWidth <= 0 || lscMap.gridHeight <= 0)
        {
            return;
        }

        if (lscMap.gains.size() !=
            static_cast<std::size_t>(lscMap.gridHeight))
        {
            return;
        }

        for (const auto &row : lscMap.gains)
        {
            if (row.size() !=
                static_cast<std::size_t>(lscMap.gridWidth))
            {
                return;
            }
        }
        const auto expectedSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if(input.size() != expectedSize) return ;

        if(pattern == BayerPattern::Unknown){
            std::cerr << "Unknown Bayer pattern" << std::endl;
            return;
        }

        output.resize(input.size());
        

        for(int y = 0 ; y < height ; ++y){
            float mapY = static_cast<float>(static_cast<float>(y + 0.5F) * lscMap.gridHeight / height - 0.5);
            mapY = std::max(0.0f, std::min(mapY, static_cast<float>(lscMap.gridHeight - 1)));
            for(int x = 0 ; x < width ; ++x){
                float mapX = static_cast<float>(static_cast<double>(x + 0.5) * lscMap.gridWidth / width - 0.5);
                mapX = std::max(0.0f, std::min(mapX, static_cast<float>(lscMap.gridWidth - 1)));
                const auto index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                const uint16_t pixelValue = input[index];
                double correctedValue = pixelValue;

                int x0 = static_cast<int>(std::floor(mapX));
                int y0 = static_cast<int>(std::floor(mapY));
                int x1 = std::min(x0 + 1, lscMap.gridWidth - 1);
                int y1 = std::min(y0 + 1, lscMap.gridHeight - 1);

                const LSCGain& gain00 = lscMap.gains[y0][x0];
                const LSCGain& gain10 = lscMap.gains[y0][x1];
                const LSCGain& gain01 = lscMap.gains[y1][x0];
                const LSCGain& gain11 = lscMap.gains[y1][x1];

                float dx = mapX - x0;
                float dy = mapY - y0;

                switch (GetCFAPhase(pattern, x, y)){
                    case CFAPhase::Red:
                    {
                        const float gainTop = gain00.rGain * (1.0f - dx) + gain10.rGain * dx;
                        const float gainBottom = gain01.rGain * (1.0f - dx) + gain11.rGain * dx;
                        const float gain = gainTop * (1.0f - dy) + gainBottom * dy;
                        correctedValue *= gain;
                        
                    }
                    break;
                    case CFAPhase::GreenRed:
                    {
                        const float gainTop = gain00.grGain * (1.0f - dx) + gain10.grGain * dx;
                        const float gainBottom = gain01.grGain * (1.0f - dx) + gain11.grGain * dx;
                        const float gain = gainTop * (1.0f - dy) + gainBottom * dy;
                        correctedValue *= gain;
                    }
                    break;
                    case CFAPhase::GreenBlue:
                    {
                        const float gainTop = gain00.gbGain * (1.0f - dx) + gain10.gbGain * dx;
                        const float gainBottom = gain01.gbGain * (1.0f - dx) + gain11.gbGain * dx;
                        const float gain = gainTop * (1.0f - dy) + gainBottom * dy;
                        correctedValue *= gain;
                    }
                    break;
                    case CFAPhase::Blue:
                    {
                        const float gainTop = gain00.bGain * (1.0f - dx) + gain10.bGain * dx;
                        const float gainBottom = gain01.bGain * (1.0f - dx) + gain11.bGain * dx;
                        const float gain = gainTop * (1.0f - dy) + gainBottom * dy;
                        correctedValue *= gain;
                    }
                    break;
                }

                output[index] = static_cast<uint16_t>(std::min(correctedValue, static_cast<double>(whiteLevel)));



            }
        }


    }

}
