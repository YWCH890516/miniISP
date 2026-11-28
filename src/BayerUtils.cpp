#include "miniISP/BayerUtils.h"
#include <stdexcept>
#include <algorithm>

namespace mini_isp{
    uint8_t GetBayerPixel_EqualBoundary(const std::vector<uint8_t> &bayerRaw, int x, int y, int width, int height){

        x = std::max(0, std::min(x, width - 1));
        y = std::max(0, std::min(y, height - 1));

        return bayerRaw[y * width + x];
    }

    uint16_t GetBayerPixelClamped16(const std::vector<uint16_t> &bayerRaw, int x, int y, int width, int height){
        x = std::max(0, std::min(x, width - 1));
        y = std::max(0, std::min(y, height - 1));

        return bayerRaw[y * width + x];
    }

    int MirrorIndex(int coordinate, int length){
        if(length < 2){
            throw std::invalid_argument("Bayer mirror requires dimensions >= 2.");
        }

        const std::int64_t period = 2LL * (length - 1);
        std::int64_t position = static_cast<std::int64_t>(coordinate) % period;

        if(position < 0){
            position += period;
        }

        if(position >= length){
            position = period - position;
        }

        return static_cast<int>(position);
    }

    uint16_t GetBayerPixelMirror16(const std::vector<uint16_t>& bayerRaw, int x, int y, int width, int height){

        x = MirrorIndex(x,width);
        y = MirrorIndex(y,height);

        return bayerRaw[y*width+x];
    }

    CFAColor GetCFAColor(BayerPattern pattern, int x, int y){
        const bool evenX = (x&1) == 0;
        const bool evenY = (y&1) == 0;

        switch (pattern){
        case BayerPattern::RGGB:
            if(evenY && evenX) return CFAColor::Red;
            if(!evenY && !evenX) return CFAColor::Blue;
            return CFAColor::Green;
            
        case BayerPattern::BGGR:
            if(evenY && evenX) return CFAColor::Blue;
            if(!evenY && !evenX) return CFAColor::Red;
            return CFAColor::Green;
            
        case BayerPattern::GRBG:
            if(evenY&&!evenX) return CFAColor::Red;
            if(!evenY&&evenX) return CFAColor::Blue;
            return CFAColor::Green;
            
        case BayerPattern::GBRG:
            if(evenY&&!evenX) return CFAColor::Blue;
            if(!evenY&&evenX) return CFAColor::Red;
            return CFAColor::Green;
            
        case BayerPattern::Unknown:
            break;
        }

        throw std::invalid_argument("Unknown Bayer pattern");
    }

    CFAPhase GetCFAPhase(BayerPattern pattern, int x, int y){
        const bool evenX = (x&1) == 0;
        const bool evenY = (y&1) == 0;

        switch(pattern){
        case BayerPattern::RGGB:
            if(evenY){
                if(evenX) return CFAPhase::Red;
                return CFAPhase::GreenRed;
            }else{
                if(evenX) return CFAPhase::GreenBlue;
                return CFAPhase::Blue;
            }
        case BayerPattern::BGGR:
            if(evenY){
                if(evenX) return CFAPhase::Blue;
                return CFAPhase::GreenBlue;
            }else{
                if(evenX) return CFAPhase::GreenRed;
                return CFAPhase::Red;
            }
        case BayerPattern::GRBG:
            if(evenY){
                if(evenX) return CFAPhase::GreenRed;
                return CFAPhase::Red;
            }else{
                if(evenX) return CFAPhase::Blue;
                return CFAPhase::GreenBlue;
            }
        case BayerPattern::GBRG:
            if(evenY){
                if(evenX) return CFAPhase::GreenBlue;
                return CFAPhase::Blue;
            }else{
                if(evenX) return CFAPhase::Red;
                return CFAPhase::GreenRed; 
            }
        case BayerPattern::Unknown:
            break;
        }
        throw std::invalid_argument("Unknown Bayer pattern");

    }
}
