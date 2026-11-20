#include "miniISP/Demosaic.h"
#include "miniISP/InputData.h"
#include "miniISP/BayerUtils.h"

#include <cmath>
#include <iostream>

namespace mini_isp{

    // ------------------ 8 bit function --------------------
    void DemosaicOnePixelBoundary(const std::vector<uint8_t>& bayerRaw, RGBFormat& outPixel, int x, int y, int width, int height){
    auto get = [&](int x, int y){
        return GetBayerPixel_EqualBoundary(bayerRaw, x, y, width, height);
    };

    if((y & 1) == 0){
        if((x&1) ==0){
            outPixel.r = get(x, y);
            outPixel.g = (get(x-1, y) + get(x+1, y) + get(x, y-1) + get(x, y+1)) >> 2;
            outPixel.b = (get(x-1, y-1) + get(x+1, y-1) + get(x-1, y+1) + get(x+1, y+1)) >> 2;
        }else{
            outPixel.g = get(x, y);
            outPixel.r = (get(x-1, y) + get(x+1, y)) >> 1;
            outPixel.b = (get(x, y-1) + get(x, y+1)) >> 1;
        }
    }else{
        if((x&1) ==0){
            outPixel.g = get(x, y);
            outPixel.r = (get(x, y-1) + get(x, y+1)) >> 1;
            outPixel.b = (get(x-1, y) + get(x+1, y)) >> 1;
        }else{
            outPixel.b = get(x, y);
            outPixel.g = (get(x-1, y) + get(x+1, y) + get(x, y-1) + get(x, y+1)) >> 2;
            outPixel.r = (get(x-1, y-1) + get(x+1, y-1) + get(x-1, y+1) + get(x+1, y+1)) >> 2;
        }
    }
}

void DemosaicByBayer_Pointer(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height){
    if(bayerRaw.empty() || width <=0 || height <=0){
        std::cout << "Invalid input data" << std::endl;
        return;
    }
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(bayerRaw.size() != pixelCount){
        std::cout << "Bayer raw data size does not match width and height" << std::endl;
        return;
    }

    pixels.resize(pixelCount);
    const uint8_t* src = bayerRaw.data();
    RGBFormat* dst = pixels.data();

    for(int y = 1 ; y < height ; ++y){
        const uint8_t* prevRowPtr = src + (y - 1) * width;
        const uint8_t* currRowPtr = src + y * width;
        const uint8_t* nextRowPtr = (y < height - 1) ? src + (y + 1) * width : currRowPtr;
        RGBFormat* dstRowPtr = dst + y * width;
    
        if((y&1) == 0){
            //evenRow 
            for(int x = 1; x < width-1 ; ++x){
                RGBFormat& outPixel = dstRowPtr[x];
                if((x&1) == 0){
                    outPixel.r = currRowPtr[x]; // R
                    outPixel.g = (currRowPtr[x-1] + currRowPtr[x+1] + prevRowPtr[x] + nextRowPtr[x]) >> 2; // G
                    outPixel.b = (prevRowPtr[x-1] + prevRowPtr[x+1] + nextRowPtr[x-1] + nextRowPtr[x+1]) >> 2; // B
                }else{
                    outPixel.g = currRowPtr[x]; // G
                    outPixel.r = (currRowPtr[x-1] + currRowPtr[x+1]) >> 1; // R
                    outPixel.b = (prevRowPtr[x] + nextRowPtr[x]) >> 1; // B
                }
            }
        }else{
            //oddRow
            for(int x = 1; x < width-1 ; ++x){
                RGBFormat& outPixel = dstRowPtr[x];
                if((x&1) == 0){
                    outPixel.g = currRowPtr[x]; // G
                    outPixel.r = (prevRowPtr[x] + nextRowPtr[x]) >> 1; // R
                    outPixel.b = (currRowPtr[x-1] + currRowPtr[x+1]) >> 1; // B
                }else{
                    outPixel.b = currRowPtr[x]; // B
                    outPixel.g = (currRowPtr[x-1] + currRowPtr[x+1] + prevRowPtr[x] + nextRowPtr[x]) >> 2; // G
                    outPixel.r = (prevRowPtr[x-1] + prevRowPtr[x+1] + nextRowPtr[x-1] + nextRowPtr[x+1]) >> 2; // R
                }
            }
        }
    }



    // 4 Boundary pixels
    for(int x = 0 ; x < width ; ++x){
        DemosaicOnePixelBoundary(bayerRaw, pixels[x], x, 0, width, height); // top row
        DemosaicOnePixelBoundary(bayerRaw, pixels[(height-1)*width + x], x, height-1, width, height); // bottom row
    }

    for(int y = 1 ; y < height-1 ; ++y){
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width], 0, y, width, height); // left column
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width + (width-1)], width-1, y, width, height); // right column
    }
}

void DemosaicByBayer(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height){
    if(bayerRaw.empty() || width <=0 || height <=0){
        std::cout << "Invalid input data" << std::endl;
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(bayerRaw.size() != pixelCount){
        std::cout << "Bayer raw data size does not match width and height" << std::endl;
        return;
    }

    pixels.resize(pixelCount);


    for(int y = 0 ; y < height ; ++y){
        for(int x = 0 ; x < width ; ++x){
            RGBFormat& pixel = pixels[y * width + x];
            if(y % 2 ==0){
                if(x%2 == 0){
                    pixel.r = GetBayerPixel_EqualBoundary(bayerRaw, x, y, width, height); //R

                    uint16_t gSum = GetBayerPixel_EqualBoundary(bayerRaw, x+1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x-1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y+1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y-1, width, height);

                    pixel.g = static_cast<uint8_t>(gSum >> 2); // average of 4 neighbors

                    uint16_t bSum = GetBayerPixel_EqualBoundary(bayerRaw, x+1, y+1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x-1, y+1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y-1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x-1, y-1, width, height);
                    pixel.b = static_cast<uint8_t>(bSum >> 2); // average of 4 diagonal neighbors
                }else{
                    pixel.g = GetBayerPixel_EqualBoundary(bayerRaw, x, y, width, height); //G

                    uint16_t rSum = GetBayerPixel_EqualBoundary(bayerRaw, x-1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y, width, height);
                    pixel.r = static_cast<uint8_t>(rSum >> 1); // average of left and right neighbors

                    uint16_t bSum = GetBayerPixel_EqualBoundary(bayerRaw, x, y+1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y-1, width, height);
                    pixel.b = static_cast<uint8_t>(bSum >> 1); // average of top and bottom neighbors
                }  
            }else{
                if(x%2 ==0){
                    pixel.g = GetBayerPixel_EqualBoundary(bayerRaw, x, y, width, height); //G

                    uint16_t rSum = GetBayerPixel_EqualBoundary(bayerRaw, x, y-1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y+1, width, height);
                    pixel.r = static_cast<uint8_t>(rSum >> 1); // average of top and bottom neighbors

                    uint16_t bSum = GetBayerPixel_EqualBoundary(bayerRaw, x-1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y, width, height);
                    pixel.b = static_cast<uint8_t>(bSum >> 1); // average of left and right neighbors
                }else{
                    pixel.b = GetBayerPixel_EqualBoundary(bayerRaw, x, y, width, height); //B

                    uint16_t gSum = GetBayerPixel_EqualBoundary(bayerRaw, x-1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y-1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x, y+1, width, height);
                    pixel.g = static_cast<uint8_t>(gSum >> 2); // average of 4 neighbors

                    uint16_t rSum = GetBayerPixel_EqualBoundary(bayerRaw, x-1, y-1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y-1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x-1, y+1, width, height)
                                 + GetBayerPixel_EqualBoundary(bayerRaw, x+1, y+1, width, height);
                    pixel.r = static_cast<uint8_t>(rSum >> 2); // average of 4 diagonal neighbors
                }
            }
        }
    }
}

void DemosaicByBayer_Optimal(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height){
    if(bayerRaw.empty() || width <=0 || height <=0){
        std::cout << "Invalid input data" << std::endl;
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(bayerRaw.size() != pixelCount){
        std::cout << "Bayer raw data size does not match width and height" << std::endl;
        return;
    }

    pixels.resize(pixelCount);

    for(int y = 1 ; y < height-1 ; ++y){
        for(int x = 1 ; x < width-1 ; ++x){
            RGBFormat& pixel = pixels[y * width + x];
            if((y & 1) == 0){
                if((x & 1) == 0){
                    pixel.r = bayerRaw[y * width + x]; // R
                    pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y * width + (x+1)] + bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 2; // G
                    pixel.b = (bayerRaw[(y-1) * width + (x-1)] + bayerRaw[(y-1) * width + (x+1)] + bayerRaw[(y+1) * width + (x-1)] + bayerRaw[(y+1) * width + (x+1)]) >> 2; // B
                }else{
                    pixel.g = bayerRaw[y * width + x]; // G
                    pixel.r = (bayerRaw[y * width + (x-1)] + bayerRaw[y * width + (x+1)]) >> 1; // R
                    pixel.b = (bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 1; // B
                }
            }else{
                if((x & 1) == 0){
                    pixel.g = bayerRaw[y * width + x]; // G     
                    pixel.r = (bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 1; // R
                    pixel.b = (bayerRaw[(y*width + (x-1))] + bayerRaw[(y*width +(x+1))])>>1; // B
                }else{
                    pixel.b = bayerRaw[y * width + x]; // B
                    pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y * width + (x+1)] +
                               bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 2; // G
                    pixel.r = (bayerRaw[(y-1) * width + (x-1)] + bayerRaw[(y-1) * width + (x+1)] +
                               bayerRaw[(y+1) * width + (x-1)] + bayerRaw[(y+1) * width + (x+1)]) >> 2; // R
                }
            }
        }
    }

    // 4 Boundary pixels
    for(int x = 0 ; x < width ; ++x){
        DemosaicOnePixelBoundary(bayerRaw, pixels[x], x, 0, width, height); // top row
        DemosaicOnePixelBoundary(bayerRaw, pixels[(height-1)*width + x], x, height-1, width, height); // bottom row
    }

    for(int y = 1 ; y < height-1 ; ++y){
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width], 0, y, width, height); // left column
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width + (width-1)], width-1, y, width, height); // right column
    }


}

void DemosaicByBayer_EdgeAware(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height){
    if(bayerRaw.empty() || width <=0 || height <=0){
        std::cout << "Invalid input data" << std::endl;
        return;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(bayerRaw.size() != pixelCount){
        std::cout << "Bayer raw data size does not match width and height" << std::endl;
        return;
    }

    pixels.resize(pixelCount);

    for(int y = 1 ; y < height-1 ; ++y){
        for(int x = 1 ; x <width-1 ; ++x){
            RGBFormat& pixel = pixels[y*width +x];
            if((y & 1) == 0){
                if((x & 1) == 0){
                    pixel.r = bayerRaw[y * width + x]; //R 
                    const int gradientH_G = std::abs(bayerRaw[y * width + (x-1)] - bayerRaw[y * width + (x+1)]);
                    const int gradientV_G = std::abs(bayerRaw[(y-1) * width + x] - bayerRaw[(y+1) * width + x]);
                    if(gradientH_G < gradientV_G){
                        //interpolate G from horizontal neighbors
                        pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y * width + (x+1)]) >> 1;
                    }else if(gradientH_G > gradientV_G){
                        //interpolate G from vertical neighbors
                        pixel.g = (bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 1;
                    }else{
                        //average both directions
                        pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y * width + (x+1)] + bayerRaw[(y-1) * width + x] + bayerRaw[(y+1) * width + x]) >> 2;
                    }

                    const int gradientDiagLB_RT = std::abs(bayerRaw[(y+1)*width + (x-1)] - bayerRaw[(y-1)*width + (x+1)]);
                    const int gradientDiagLT_RB = std::abs(bayerRaw[(y-1)*width + (x-1)] - bayerRaw[(y+1)*width + (x+1)]);
                    if(gradientDiagLB_RT < gradientDiagLT_RB){
                        // interpolate B from diagonal neighbors (LB and RT)
                        pixel.b = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)]) >> 1;
                    }else if(gradientDiagLB_RT > gradientDiagLT_RB){
                        // interpolate B from diagonal neighbors (LT and RB)
                        pixel.b = (bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 1;
                    }else{
                        pixel.b = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)] + bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 2;
                    }
                }else{
                    pixel.g = bayerRaw[y* width +x]; //G
                    pixel.r = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)]) >> 1; //R
                    pixel.b = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 1; //B
                }
            }else{
                if((x&1) == 0){
                    pixel.g = bayerRaw[y*width + x]; //G
                    pixel.r = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 1; //R
                    pixel.b = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)]) >> 1; //B
                }else{
                    pixel.b = bayerRaw[y*width + x]; //B
                    const int gradientH_G = std::abs(bayerRaw[y*width + (x-1)] - bayerRaw[y*width + (x+1)]);
                    const int gradientV_G = std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y+1)*width + x]);
                    if(gradientH_G < gradientV_G){
                        pixel.g = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)]) >> 1;
                    }else if(gradientH_G > gradientV_G){
                        pixel.g = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 1;
                    }else{
                        pixel.g = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)] + bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 2;
                    }
                    const int gradientDiagLB_RT = std::abs(bayerRaw[(y+1)*width + (x-1)] - bayerRaw[(y-1)*width + (x+1)]);
                    const int gradientDiagLT_RB = std::abs(bayerRaw[(y-1)*width + (x-1)] - bayerRaw[(y+1)*width + (x+1)]);
                    if(gradientDiagLB_RT < gradientDiagLT_RB){
                        pixel.r = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)]) >> 1;
                    }else if(gradientDiagLB_RT > gradientDiagLT_RB){
                        pixel.r = (bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 1;
                    }else{
                        pixel.r = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)] + bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 2;
                    }
                }
            }
        }
    }

    for(int x = 0 ; x < width ; ++x){
        DemosaicOnePixelBoundary(bayerRaw, pixels[x], x, 0, width, height); // top row
        DemosaicOnePixelBoundary(bayerRaw, pixels[(height-1)*width + x], x, height-1, width, height); // bottom row
    }

    for(int y = 0 ; y < height ; ++y){
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width], 0, y, width, height); // left column
        DemosaicOnePixelBoundary(bayerRaw, pixels[y*width + (width-1)], width-1, y, width, height); // right column
    }


}

void DemosaicByBayer_EdgeAwareV2(const std::vector<uint8_t>& bayerRaw, std::vector<RGBFormat>& pixels, int width, int height){
        //V2 goal:
        // Improve the robustness of G directional interpolation
        // by replacing single-pair gradient decisions with
        // multi-sample directional evidence.
        if (bayerRaw.empty() || width <= 0 || height <= 0)
        {
            std::cout << "Invalid input data" << std::endl;
            return;
        }
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if(bayerRaw.size() != pixelCount){
        std::cout << "Bayer raw data size does not match width and height" << std::endl;
        return;
    }

    pixels.resize(pixelCount);
    /// Use V1 as the fallback for the full image.
    // V2 overwrites only pixels with complete 5x5 neighborhood support.
    DemosaicByBayer_EdgeAware(bayerRaw, pixels, width, height);
    int directionScoreThreshold = 10;
    int diagonalThreshold = 3;
    for(int y = 2; y < height -2 ; ++y){
        for(int x = 2; x < width -2 ; ++x){
            RGBFormat& pixel = pixels[y*width +x];
            if((y&1) == 0){
                // even row (RG)
                if((x&1) == 0){
                    // R pixel 
                    pixel.r = bayerRaw[y*width +x]; // R
                    // G interpolation
                    const int trendH0 = std::abs(bayerRaw[(y-2)*width + (x-1)] - bayerRaw[(y-2)*width + (x+1)]) ;
                    const int trendH1 = (std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y-1)*width + (x-2)]) + 
                                    std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y-1)*width + (x+2)])) >>1; 
                    const int trendH2 = std::abs(bayerRaw[(y*width + (x - 1))] - bayerRaw[(y*width + (x + 1))]);
                    const int trendH3 = (std::abs(bayerRaw[(y+1)*width + x] - bayerRaw[(y+1)*width + (x-2)]) + 
                                    std::abs(bayerRaw[(y+1)*width + x] - bayerRaw[(y+1)*width + (x+2)])) >> 1;
                    const int trendH4 = std::abs(bayerRaw[(y+2)*width + (x-1)] - bayerRaw[(y+2)*width + (x+1)]);

                    const int scoreH = 1*trendH0 + 2*trendH1 + 4*trendH2 + 2*trendH3 + 1*trendH4;

                    const int trendV0 = std::abs(bayerRaw[(y-1)*width + (x-2)] - bayerRaw[(y+1)*width + (x-2)]);
                    const int trendV1 = (std::abs(bayerRaw[y*width + (x -1)] - bayerRaw[(y-2)*width + (x-1)]) +
                                    std::abs(bayerRaw[y*width + (x-1)] - bayerRaw[(y+2) *width + (x-1)])) >> 1 ;
                    const int trendV2 = std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y+1)*width + x]);
                    const int trendV3 = (std::abs(bayerRaw[y*width + (x+1)] - bayerRaw[(y-2)*width + (x+1)]) +
                                    std::abs(bayerRaw[y*width + (x+1)] - bayerRaw[(y+2)*width + (x+1)])) >> 1;
                    const int trendV4 = std::abs(bayerRaw[(y-1)*width + (x+2)] - bayerRaw[(y+1)*width + (x+2)]);

                    const int scoreV = 1*trendV0 + 2*trendV1 + 4*trendV2 + 2*trendV3 + 1*trendV4;
                    if(std::abs(scoreH - scoreV) > directionScoreThreshold){
                        if(scoreH < scoreV){
                            pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y*width +(x+1)]) >>1;
                        }else{
                            pixel.g = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width+x])>>1;
                        }
                    }else{
                        // HV convidence is not enough -->Low directional confidence: fall back to non-directional interpolation.
                        pixel.g = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)] +
                                    bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width +x]) >>2;
                    }
                    
                    const int gradientDiagLB_RT = std::abs(bayerRaw[(y+1)*width + (x-1)] - bayerRaw[(y-1)*width + (x+1)]);
                    const int gradientDiagLT_RB = std::abs(bayerRaw[(y-1)*width + (x-1)] - bayerRaw[(y+1)*width + (x+1)]);
                    
                    if(std::abs(gradientDiagLB_RT - gradientDiagLT_RB)>diagonalThreshold){
                        if(gradientDiagLB_RT < gradientDiagLT_RB){
                        // interpolate B from diagonal neighbors (LB and RT)
                        pixel.b = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)]) >> 1;
                        }else{
                        // interpolate B from diagonal neighbors (LT and RB)
                        pixel.b = (bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 1;}
                    }else{
                        pixel.b = (bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y-1)*width+(x+1)] +
                                    bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)])>>2;
                    }
                }else{
                    pixel.g = bayerRaw[y* width +x]; //G
                    pixel.r = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)]) >> 1; //R
                    pixel.b = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 1; //B
                }
            }else{
                if((x&1)==0){
                    //G pixel 
                    pixel.g = bayerRaw[y*width + x]; //G
                    pixel.r = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width + x]) >> 1; //R
                    pixel.b = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)]) >> 1; //B
                }else{
                    //B pixel 
                    pixel.b = bayerRaw[y*width +x]; //B
                    // G interpolation 
                    const int trendH0 = std::abs(bayerRaw[(y-2)*width + (x-1)] - bayerRaw[(y-2)*width + (x+1)]) ;
                    const int trendH1 = (std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y-1)*width + (x-2)]) + 
                                    std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y-1)*width + (x+2)])) >>1; 
                    const int trendH2 = std::abs(bayerRaw[(y*width + (x - 1))] - bayerRaw[(y*width + (x + 1))]);
                    const int trendH3 = (std::abs(bayerRaw[(y+1)*width + x] - bayerRaw[(y+1)*width + (x-2)]) + 
                                    std::abs(bayerRaw[(y+1)*width + x] - bayerRaw[(y+1)*width + (x+2)]))>> 1;
                    const int trendH4 = std::abs(bayerRaw[(y+2)*width + (x-1)] - bayerRaw[(y+2)*width + (x+1)]);

                    const int scoreH = 1*trendH0 + 2*trendH1 + 4*trendH2 + 2*trendH3 + 1*trendH4;

                    const int trendV0 = std::abs(bayerRaw[(y-1)*width + (x-2)] - bayerRaw[(y+1)*width + (x-2)]);
                    const int trendV1 = (std::abs(bayerRaw[y*width + (x -1)] - bayerRaw[(y-2)*width + (x-1)]) +
                                    std::abs(bayerRaw[y*width + (x-1)] - bayerRaw[(y+2) *width + (x-1)])) >> 1 ;
                    const int trendV2 = std::abs(bayerRaw[(y-1)*width + x] - bayerRaw[(y+1)*width + x]);
                    const int trendV3 = (std::abs(bayerRaw[y*width + (x+1)] - bayerRaw[(y-2)*width + (x+1)]) +
                                    std::abs(bayerRaw[y*width + (x+1)] - bayerRaw[(y+2)*width + (x+1)])) >> 1;
                    const int trendV4 = std::abs(bayerRaw[(y-1)*width + (x+2)] - bayerRaw[(y+1)*width + (x+2)]);

                    const int scoreV = 1*trendV0 + 2*trendV1 + 4*trendV2 + 2*trendV3 + 1*trendV4;
                    if(std::abs(scoreH - scoreV) > directionScoreThreshold){
                        if(scoreH < scoreV){
                            pixel.g = (bayerRaw[y * width + (x-1)] + bayerRaw[y*width +(x+1)]) >>1;
                        }else{
                            pixel.g = (bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width+x])>>1;
                        }
                    }else{
                        // this would flatten region
                        pixel.g = (bayerRaw[y*width + (x-1)] + bayerRaw[y*width + (x+1)] +
                                    bayerRaw[(y-1)*width + x] + bayerRaw[(y+1)*width +x]) >>2;
                    }

                    const int gradientDiagLB_RT = std::abs(bayerRaw[(y+1)*width + (x-1)] - bayerRaw[(y-1)*width + (x+1)]);
                    const int gradientDiagLT_RB = std::abs(bayerRaw[(y-1)*width + (x-1)] - bayerRaw[(y+1)*width + (x+1)]);
                    if(std::abs(gradientDiagLB_RT - gradientDiagLT_RB)> diagonalThreshold){
                        if(gradientDiagLB_RT < gradientDiagLT_RB){
                            pixel.r = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)]) >> 1;
                        }else{
                            pixel.r = (bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 1;
                        }
                    }else{
                        pixel.r = (bayerRaw[(y+1)*width + (x-1)] + bayerRaw[(y-1)*width + (x+1)] + bayerRaw[(y-1)*width + (x-1)] + bayerRaw[(y+1)*width + (x+1)]) >> 2;
                    }
                    
                }
            }
        }
    }
}


// --------------------- 16 bit function ----------------------------
    void DemosaicBilinear16(const std::vector<uint16_t>& bayerRaw, std::vector<RGBFormat16>& pixels, int width, int height, BayerPattern pattern, uint16_t whiteLevel){
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

        pixels.resize(bayerRaw.size());

        // Create Helper 
        auto average2 = [](uint16_t a , uint16_t b){
            return static_cast<uint16_t>((static_cast<uint32_t>(a) + static_cast<uint32_t>(b))/2U);
        };

        auto average4 = [](uint16_t a, uint16_t b, uint16_t c, uint16_t d){
            return static_cast<uint16_t>((
                static_cast<uint32_t>(a)+
                static_cast<uint32_t>(b)+
                static_cast<uint32_t>(c)+
                static_cast<uint32_t>(d)
            )/ 4U);
        };

        auto get = [&](int x, int y){
            return std::min(GetBayerPixelClamped16(bayerRaw, x, y, width, height), whiteLevel);
        };

        for(int y = 0 ; y < height ; ++y){
            for(int x = 0 ; x < width ; ++x){
                auto& pixel = pixels[static_cast<size_t>(y) * width + x];

                switch(GetCFAColor(pattern,x,y)){
                case CFAColor::Red:
                    pixel.r = get(x,y);
                    pixel.g = average4(get(x-1,y), 
                                       get(x+1,y), 
                                       get(x,y-1),
                                       get(x,y+1));
                    pixel.b = average4(get(x-1,y-1),
                                       get(x-1,y+1),
                                       get(x+1,y-1),
                                       get(x+1,y+1));
                    break;
                case CFAColor::Blue:
                    pixel.b = get(x,y);
                    pixel.g = average4(get(x-1,y), 
                                       get(x+1,y), 
                                       get(x,y-1),
                                       get(x,y+1));
                    pixel.r = average4(get(x-1,y-1),
                                       get(x-1,y+1),
                                       get(x+1,y-1),
                                       get(x+1,y+1));
                    break;

                case CFAColor::Green:
                    const int neighborX = (x+1 < width) ? x+1 : x-1;
                    const auto horizontalColor = GetCFAColor(pattern, neighborX, y);
                    pixel.g = get(x,y);
                    if(horizontalColor == CFAColor::Red){
                        pixel.r = average2(get(x-1,y) , get(x+1,y));
                        pixel.b = average2(get(x,y-1) , get(x,y+1));
                    }else{
                        pixel.b = average2(get(x-1,y) , get(x+1,y));
                        pixel.r = average2(get(x,y-1) , get(x,y+1));
                    }
                    break;
                }
            }
        }


    }




}
