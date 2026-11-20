#include <algorithm>
#include <array>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include "miniISP/BayerUtils.h"
#include "miniISP/DefectivePixelCorrection.h"

namespace mini_isp{
    std::vector<SyntheticDefect> GenerateRandomHotPixel(int width, int height, std::size_t count){
        std::vector<SyntheticDefect> defects;

        if(width < 5 || height < 5) return defects;

        const std::size_t interiorWidth = static_cast<std::size_t>(width - 4);
        const std::size_t interiorHeight = static_cast<std::size_t>(height - 4);
        const std::size_t maximumCount = interiorWidth * interiorHeight;
        if (count > maximumCount) {
            throw std::invalid_argument("Defect count exceeds the available interior pixels.");
        }

        defects.reserve(count);

        std::mt19937 generator{12345};

        std::uniform_int_distribution<int> xDistribution{2, width-3};
        std::uniform_int_distribution<int> yDistribution{2, height-3};

        std::unordered_set<std::size_t> usedIndices;

        while(defects.size() < count){
            const int x = xDistribution(generator);
            const int y = yDistribution(generator);

            const auto idx = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);

            if(!usedIndices.insert(idx).second){
                continue;
            }

            defects.push_back({x,y,SyntheticDefectType::Hot,0});
        }

        return defects;
        
    }


    void InjectSyntheticDefect16(const std::vector<uint16_t>& raw, std::vector<uint16_t>& output, int width, int height, uint16_t blackLevel, uint16_t whiteLevel, const std::vector<SyntheticDefect>& defects){
        output.clear();

        if(width <= 0 || height <= 0) return;

        const auto expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height);

        if(raw.size() != expectedSize) return ;

        if(whiteLevel <= blackLevel) return;
        
        output = raw;

        for(const auto& defect : defects){
            if(defect.x < 0 || defect.x >= width || defect.y < 0 || defect.y >= height) continue;

            const auto index = static_cast<size_t>(defect.y) * static_cast<size_t>(width) +
                                static_cast<size_t>(defect.x);
            
            switch(defect.type){
            case SyntheticDefectType::Hot:
                output[index] = whiteLevel;
                break;
            case SyntheticDefectType::Dead:
                output[index] = blackLevel;
                break;
            case SyntheticDefectType::Stuck:
                output[index] = std::min(defect.stuckVal, whiteLevel);
                break;
            }

        }

    }


    void DefectivePixelCorrection16_v1(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, BayerPattern pattern, uint16_t threshold){
        output.clear();

        if(width<=0 || height<=0 || pattern == BayerPattern::Unknown) return;

        const auto expectedSize = static_cast<size_t>(height) * static_cast<size_t>(width);

        if(input.size()!= expectedSize) return;

        output = input;
        for(int y = 2 ; y+2 < height ; ++y){
            for(int x = 2 ; x+2 < width ; ++x){
                const int idx = y * width + x ;
                const CFAColor centerColor = GetCFAColor(pattern,x,y);
                const uint16_t centerVal = input[idx];
                if(centerColor == CFAColor::Green){
                    uint16_t leftTop = GetBayerPixelClamped16(input, x-1 , y-1, width, height);
                    uint16_t Top = GetBayerPixelClamped16(input, x , y-2, width, height);
                    uint16_t rightTop = GetBayerPixelClamped16(input , x+1, y-1 , width, height);
                    uint16_t right = GetBayerPixelClamped16(input, x+2, y , width, height);
                    uint16_t rightBottom = GetBayerPixelClamped16(input, x+1, y+1, width, height);
                    uint16_t bottom = GetBayerPixelClamped16(input, x, y+2, width, height);
                    uint16_t leftBottom = GetBayerPixelClamped16(input, x -1, y+1, width, height);
                    uint16_t left = GetBayerPixelClamped16(input, x -2, y, width, height);
                    std::array<uint16_t,8> pts{
                        leftTop,
                        Top,
                        rightTop,
                        right,  
                        rightBottom,
                        bottom,
                        leftBottom,
                        left
                    };

                    bool allHigher = true;
                    bool allLower = true;

                    for(const uint16_t neighbor : pts){
                        const int32_t diff = static_cast<int32_t>(centerVal) - static_cast<int32_t>(neighbor);

                        allHigher = allHigher && diff > static_cast<int32_t>(threshold);
                        allLower = allLower && diff < - static_cast<int32_t>(threshold);
                    }

                    if(allHigher || allLower){
                        std::sort(pts.begin(),pts.end());

                        output[idx] = static_cast<uint16_t>((static_cast<uint32_t>(pts[3]) + static_cast<uint32_t>(pts[4])) / 2 );
                    }
                    

                }else{
                    uint16_t leftTop = GetBayerPixelClamped16(input, x-2 , y-2, width, height);
                    uint16_t Top = GetBayerPixelClamped16(input, x , y-2, width, height);
                    uint16_t rightTop = GetBayerPixelClamped16(input , x+2, y -2 , width, height);
                    uint16_t right = GetBayerPixelClamped16(input, x+2, y , width, height);
                    uint16_t rightBottom = GetBayerPixelClamped16(input, x+2, y+2, width, height);
                    uint16_t bottom = GetBayerPixelClamped16(input, x, y+2, width, height);
                    uint16_t leftBottom = GetBayerPixelClamped16(input, x -2, y+2, width, height);
                    uint16_t left = GetBayerPixelClamped16(input, x -2, y, width, height);
                    std::array<uint16_t,8> pts{
                        leftTop,
                        Top,
                        rightTop,
                        right,  
                        rightBottom,
                        bottom,
                        leftBottom,
                        left
                    };

                    bool allHigher = true;
                    bool allLower = true;

                    for(const uint16_t neighbor : pts){
                        const int32_t diff = static_cast<int32_t>(centerVal) - static_cast<int32_t>(neighbor);

                        allHigher = allHigher && diff > static_cast<int32_t>(threshold);
                        allLower = allLower && diff < - static_cast<int32_t>(threshold);
                    }

                    if(allHigher || allLower){
                        std::sort(pts.begin(),pts.end());

                        output[idx] = static_cast<uint16_t>((static_cast<uint32_t>(pts[3]) + static_cast<uint32_t>(pts[4])) / 2U);
                    }
                    
                }
                
            }
        }
    }
}
