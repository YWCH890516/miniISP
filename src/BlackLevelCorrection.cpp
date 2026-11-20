#include "miniISP/BlackLevelCorrection.h"
#include "miniISP/BayerUtils.h"

#include <iostream>



namespace mini_isp{
    void ApplyBlackLevelCorrection16_v1(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height, uint16_t blackLevel){

        // clear output array 
        output.clear();

        if(width <= 0 || height <= 0) {
            std::cout<<"image size is false"<<std::endl;
            return; 
        }

        const auto expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height);
        if(input.size() != expectedSize) {
            std::cout << "image size is false " << std::endl;
            return;
        }

        output.resize(expectedSize);

        for(size_t idx = 0 ; idx < expectedSize ; ++idx){
            const uint16_t val = input[idx];

            output[idx] = val > blackLevel ? static_cast<uint16_t>(val - blackLevel):uint16_t{0};
        }


    }


    // v2 : {R,Gr,Gb,B} - Black Value 
    void ApplyBlackLevelCorrection16_v2(const std::vector<uint16_t>& input, std::vector<uint16_t>& output, int width, int height,BayerPattern pattern,BayerBlackLevel levels){

        // clear output 
        output.clear();
        // check size 
        if(width <= 0 || height <= 0 || pattern == BayerPattern::Unknown) {
            std::cout << "image size is false"<<std::endl;
            return;
        }

        const auto expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height);
        if(input.size()!=expectedSize){
            std::cout<<"image size is false"<<std::endl;
            return;
        }

        output.resize(expectedSize);

        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                const std::size_t idx = static_cast<std::size_t>(y) *
                                        static_cast<std::size_t>(width) +
                                        static_cast<std::size_t>(x);
                
                uint16_t black{};
                switch(GetCFAPhase(pattern,x,y)){
                case CFAPhase::Red:
                    black = levels.r;
                    break;
                case CFAPhase::Blue:
                    black = levels.b;
                    break;
                case CFAPhase::GreenRed:
                    black = levels.gr;
                    break;
                case CFAPhase::GreenBlue:
                    black = levels.gb;
                    break;
                }

                output[idx] = input[idx]>black? input[idx] - black : uint16_t{0};
            }
        }

    }
}
