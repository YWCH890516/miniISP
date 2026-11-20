#include "miniISP/AutoWhiteBalance.h"
#include "miniISP/InputData.h"
#include "miniISP/BayerUtils.h"
#include <algorithm>
#include <iostream>

namespace mini_isp{
    //// Auto White Balance 
    // =====【Gray World】==========
    void AWB_GrayWorld(const std::vector<uint8_t>& bayerRaw, std::vector<uint8_t>& bayerAWB, int width , int height){
        bayerAWB.clear();
        if (width <= 0 || height <= 0 ||
            bayerRaw.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            std::cerr << "Invalid RAW data or dimensions\n";
            return;
        }
        bayerAWB.resize(bayerRaw.size());
        uint64_t totalR = 0;
        uint64_t totalG = 0;
        uint64_t totalB = 0;
        uint64_t countR = 0;
        uint64_t countG = 0;
        uint64_t countB = 0;
        
        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                if((y&1) == 0){
                    // even row 
                    if((x&1) == 0) {
                        totalR+=bayerRaw[y*width+x];
                        ++countR;
                    }else {
                        totalG+=bayerRaw[y*width +x];
                        ++countG;
                    }
                }else{
                    if((x&1)==0) {
                        totalG+=bayerRaw[y*width+x];
                        ++countG;
                    }else {
                        totalB+=bayerRaw[y*width+x];
                        ++countB;
                    }
                }
            }
        }

        if (countR == 0 || countG == 0 || countB == 0) {
            std::cerr << "Image dimensions do not contain every CFA channel\n";
            bayerAWB.clear();
            return;
        }

        double meanR = static_cast<double>(totalR) / countR;
        double meanG = static_cast<double>(totalG) / countG;
        double meanB = static_cast<double>(totalB) / countB;

        if (meanR <= 0.0 || meanG <= 0.0 || meanB <= 0.0) {
            std::cerr << "Cannot calculate AWB gains from a zero channel mean\n";
            bayerAWB.clear();
            return;
        }

        // G is reference 
        double gainG = 1.0;
        double gainR = meanG / meanR;
        double gainB = meanG / meanB;

        for(int y = 0 ; y < height ; ++y){
            for (int x = 0; x < width; ++x)
            {
                const int idx = y * width + x;
                double gain;

                if ((y & 1) == 0)
                {
                    if ((x & 1) == 0)
                        gain = gainR; // R
                    else
                        gain = gainG; // Gr
                }
                else
                {
                    if ((x & 1) == 0)
                        gain = gainG; // Gb
                    else
                        gain = gainB; // B
                }

                const double value = bayerRaw[idx] * gain;

                bayerAWB[idx] = static_cast<uint8_t>(
                    std::max(0.0, std::min(value, 255.0)));
            }
        } 

        // print the color information 

        
        std::cout<<"=============={Gray World V1}==================="<<std::endl;
        std::cout<<"Mean Value of R by GrayWorldV1:"<<meanR<<std::endl;
        std::cout<<"Mean Value of G by GrayWorldV1:"<<meanG<<std::endl;
        std::cout<<"Mean Value of B by GrayWorldV1:"<<meanB<<std::endl;

        std::cout<<"Value of gainR by GrayWorldV1:"<<gainR<<std::endl;
        std::cout<<"Value of gainG by GrayWorldV1:"<<gainG<<std::endl;
        std::cout<<"Value of gainB by GrayWorldV1:"<<gainB<<std::endl;

        std::cout<<"candidate of R by GrayWorldV1:"<<countR<<std::endl;
        std::cout<<"candidate of G by GrayWorldV1:"<<countG<<std::endl;
        std::cout<<"candidate of B by GrayWorldV1:"<<countB<<std::endl;
        std::cout<<"=============={END}==================="<<std::endl;
    }

    // Gray World V2 
    void AWB_GrayWorldV2(const std::vector<uint8_t>& bayerRaw, std::vector<uint8_t>& bayerAWB, int width , int height){
        bayerAWB.clear();
        if (width <= 0 || height <= 0 ||
            bayerRaw.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
            std::cerr << "Invalid RAW data or dimensions\n";
            return;
        }

        bayerAWB.resize(bayerRaw.size());
        // only count neutral coloe R ~ B ~ G
        uint64_t totalR = 0;
        uint64_t totalG = 0;
        uint64_t totalB = 0;
        uint64_t countR = 0;
        uint64_t countG = 0;
        uint64_t countB = 0;

        int thresholdNeutral = 10; // max - min < threshold ==> neutral 
        int lowThreshold = 5;
        int highThreshold = 250;
        // Block Size == 2 
        for(int y = 0 ; y < height - 1 ; y += 2){
            for(int x = 0 ; x < width -1 ; x += 2){
                uint8_t R  = bayerRaw[y*width + x];
                uint8_t G1 = bayerRaw[y*width +(x+1)];
                uint8_t G2 = bayerRaw[(y+1)*width + x];
                uint8_t B  = bayerRaw[(y+1)*width + (x+1)];

                uint8_t G = (G1+G2)>>1;

                const int intensity = (R + G + B) / 3;

                if (intensity < lowThreshold ||
                    intensity > highThreshold) {
                    continue;
                }          
                
                


                if((std::max({R,G,B}) - std::min({R,G,B})) <= thresholdNeutral){
                    totalR += R; totalG += (G1+G2); totalB += B;
                    ++countR ; countG += 2; ++countB;
                }

            }
        }

        if (countR == 0 || countG == 0 || countB == 0) {
                    std::cout<< "no neutral so undifined" <<std::endl;
                    bayerAWB.clear();
                    return;
        }

        double meanR = static_cast<double>(totalR)/ countR;
        double meanG = static_cast<double>(totalG)/ countG;
        double meanB = static_cast<double>(totalB)/ countB;
        
        // G is reference 
        double gainG = 1.0;
        double gainR = meanG / meanR;
        double gainB = meanG / meanB;


        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                const int idx = y*width +x;
                double gain = 1.0;
                if((y&1)==0){
                    if((x&1)==0) gain = gainR;
                    else gain = gainG;
                }else{
                    if((x&1)==0) gain = gainG;
                    else gain = gainB;
                }

                const double value = bayerRaw[idx] * gain;

                bayerAWB[idx] = static_cast<uint8_t>(std::max(0.0,std::min(value,255.0)));

            }
        }

        // print the color information 
        std::cout<<"=============={Gray World V2}==================="<<std::endl;
        std::cout<<"Mean Value of R by GrayWorldV2:"<<meanR<<std::endl;
        std::cout<<"Mean Value of G by GrayWorldV2:"<<meanG<<std::endl;
        std::cout<<"Mean Value of B by GrayWorldV2:"<<meanB<<std::endl;

        std::cout<<"Value of gainR by GrayWorldV2:"<<gainR<<std::endl;
        std::cout<<"Value of gainG by GrayWorldV2:"<<gainG<<std::endl;
        std::cout<<"Value of gainB by GrayWorldV2:"<<gainB<<std::endl;

        std::cout<<"candidate of R by GrayWorldV2:"<<countR<<std::endl;
        std::cout<<"candidate of G by GrayWorldV2:"<<countG<<std::endl;
        std::cout<<"candidate of B by GrayWorldV2:"<<countB<<std::endl;
        std::cout<<"=============={END}==================="<<std::endl;
    }

    void AWB_GrayWorldV2_16(const std::vector<uint16_t>& bayerRaw, std::vector<uint16_t>& bayerAWB, int width , int height, BayerPattern pattern, uint16_t whiteLevel){
        bayerAWB.clear();
        if (width <= 0 || height <= 0){
            std::cerr << "Invalid image dimensions\n";
            return;
        }

        const auto pixelCount =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height);

        if (bayerRaw.size() != pixelCount){
            std::cerr << "RAW size does not match dimensions\n";
            return;
        }
        if (whiteLevel == 0) {
            std::cerr << "Invalid white level\n";
            return;
        }
        if (pattern == BayerPattern::Unknown) {
            std::cerr << "Unknown Bayer pattern\n";
            return;
        }



        bayerAWB.resize(bayerRaw.size());

        uint64_t totalR = 0;
        uint64_t totalG = 0;
        uint64_t totalB = 0;
        uint64_t countR = 0;
        uint64_t countG = 0;
        uint64_t countB = 0;

        const auto thresholdNeutral =static_cast<std::uint32_t>(static_cast<double>(whiteLevel) * 0.04); // max - min < threshold ==> neutral 
        const auto lowThreshold =static_cast<std::uint32_t>(static_cast<double>(whiteLevel) * 0.02);

        const auto highThreshold =static_cast<std::uint32_t>(static_cast<double>(whiteLevel) * 0.98);

        for(int y = 0 ; y < height-1 ; y+=2){
            for(int x = 0 ; x < width-1 ; x+=2){
                
                uint16_t red{};
                uint16_t green1{};
                uint16_t green2{};
                uint16_t blue{};
                int greenCount = 0;

                for(int offsetY = 0 ; offsetY < 2 ; ++offsetY){
                    for(int offsetX = 0 ; offsetX <2 ; ++offsetX){
                        const int pixelX = x + offsetX;
                        const int pixelY = y + offsetY;

                        const auto idx = static_cast<size_t>(pixelY) * width + static_cast<size_t>(pixelX);
                        const auto val = bayerRaw[idx];

                        switch(GetCFAColor(pattern, pixelX, pixelY)){
                        case CFAColor::Red:
                            red = val;
                            break;
                        case CFAColor::Green:
                            if(greenCount == 0){
                                green1 = val;
                            }else{
                                green2 = val;
                            }
                            ++greenCount;
                            break;
                        case CFAColor::Blue:
                            blue = val;
                            break;
                        }
                    }
                }
                if (greenCount != 2) {
                    std::cerr << "Invalid CFA block\n";
                    bayerAWB.clear();
                    return;
                }   
                const uint32_t green = (static_cast<uint32_t>(green1) + static_cast<uint32_t>(green2)) >> 1;

                const uint32_t intensity = ((static_cast<uint32_t>(red) + static_cast<uint32_t>(green) + static_cast<uint32_t>(blue)) / 3);

                if(intensity <= lowThreshold || intensity >= highThreshold) continue;


                if((std::max({static_cast<uint32_t>(red),green,static_cast<uint32_t>(blue)}) - std::min({static_cast<uint32_t>(red),green,static_cast<uint32_t>(blue)})) <= thresholdNeutral){
                    totalR += red; totalG += (green1+green2); totalB += blue;
                    ++countR ; countG += 2; ++countB;
                }

            }
        }

        if (countR == 0 || countG == 0 || countB == 0) {
                    std::cout<< "no neutral so undifined" <<std::endl;
                    bayerAWB.clear();
                    return;
        }

        double meanR = static_cast<double>(totalR)/ countR;
        double meanG = static_cast<double>(totalG)/ countG;
        double meanB = static_cast<double>(totalB)/ countB;
        
        if (meanR <= 0.0 || meanG <= 0.0 || meanB <= 0.0) {
            std::cerr << "Cannot calculate AWB gains from zero channel mean\n";
            bayerAWB.clear();
            return;
        }


        // G is reference 
        double gainG = 1.0;
        double gainR = meanG / meanR;
        double gainB = meanG / meanB;

        for(int y = 0 ; y<height ; ++y){
            for(int x= 0 ; x<width ; ++x){
                const int idx = y * width + x;
                double gain = 1;
                
                switch (GetCFAColor(pattern,x,y)){
                case CFAColor::Red :
                    gain = gainR;
                    break;
                case CFAColor::Green:
                    gain = gainG;
                    break;
                case CFAColor::Blue:
                    gain = gainB;
                    break;
                }
                const double value = bayerRaw[idx]*gain;
                
                bayerAWB[idx] = static_cast<uint16_t>(std::max(0.0, std::min(value, static_cast<double>(whiteLevel))));
            }
        }

    
    }

    void ApplyWhiteBalanceGains16(const std::vector<uint16_t>& bayerRaw, std::vector<uint16_t>& bayerAWB, int width, int height,BayerPattern pattern, double gainR, double gainG, double gainB, uint16_t whiteLevel){
        
        bayerAWB.clear();
        if(width <= 0 || height <= 0){
            std::cerr << "Invalid image dimensions\n";
            return;
        }
        if(pattern == BayerPattern::Unknown){
            std::cerr << "Unknown Bayer pattern\n";
            return;
        }
        if(whiteLevel == 0){
            std::cerr << "Invalid white level\n";
            return;
        }

        const auto pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if(bayerRaw.size() != pixelCount){
            std::cerr << "RAW size does not match dimensions\n";
            return;
        }


        bayerAWB.resize(bayerRaw.size());

        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                const auto index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                uint16_t pixelValue = bayerRaw[index];
                double gain = 1.0;
                switch(GetCFAColor(pattern, x, y)){
                    case CFAColor::Red:
                        gain = gainR;
                        break;
                    case CFAColor::Green:
                        gain = gainG;
                        break;
                    case CFAColor::Blue:
                        gain = gainB;
                        break;
                }
                const double correctedVal = static_cast<double>(pixelValue) * gain;
                const double storageMax = static_cast<double>(std::numeric_limits<uint16_t>::max());

                // bayerAWB[index] = static_cast<uint16_t>(std::max(0.0,std::min(correctedVal, static_cast<double>(whiteLevel))));
                bayerAWB[index] = static_cast<uint16_t>(std::max(0.0,std::min(correctedVal, storageMax)));
            }
        }
    }
}
