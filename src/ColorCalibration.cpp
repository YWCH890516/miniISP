#include "miniISP/ColorCalibration.h"
#include "miniISP/BayerUtils.h"
#include "miniISP/InputData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace mini_isp{
    std::array<PatchColorDouble, 24> GetColorChartPatchColors(
        const std::vector<uint16_t>& bayer,
        int width,
        int height,
        BayerPattern pattern,
        const ColorChartCorners& corners){

        std::array<PatchColorDouble, 24> patchColors{};

        // Implementation of color chart patch color extraction
        constexpr int columns = 6;
        constexpr int rows = 4;
        const int roiHalfSize = 30; // Half size of the region of interest around each patch center
        


        for(int r = 0 ; r < rows ; ++r){
            for(int c = 0 ; c < columns ; ++c){
                // Calculate the center of each patch based on the corners and the grid layout
                const float u = static_cast<float>(c + 0.5f) / (columns);
                const float v = static_cast<float>(r + 0.5f) / (rows); 
                
                const float centerX = (1.0f - u) * (1.0f - v) * corners.topLeft.x + 
                                        u * (1.0f - v) * corners.topRight.x + 
                                        (1.0f - u) * v * corners.bottomLeft.x + 
                                        u * v * corners.bottomRight.x;
                const float centerY = (1.0f - u) * (1.0f - v) * corners.topLeft.y +
                                        u * (1.0f - v) * corners.topRight.y + 
                                        (1.0f - u) * v * corners.bottomLeft.y + 
                                        u * v * corners.bottomRight.y;
                                        
                // Define the region of interest (ROI) around the patch center
                const int xStart = std::max(0, static_cast<int>(std::round(centerX)) - roiHalfSize);
                const int xEnd = std::min(width, static_cast<int>(std::round(centerX)) + roiHalfSize);
                const int yStart = std::max(0, static_cast<int>(std::round(centerY)) - roiHalfSize);
                const int yEnd = std::min(height, static_cast<int>(std::round(centerY)) + roiHalfSize);

                // Accumulate color values within the ROI
                uint64_t rSum = 0, gSum = 0, bSum = 0;
                uint64_t countR = 0, countG = 0, countB = 0;

                for(int y = yStart ; y < yEnd ; ++y){
                    for(int x = xStart ; x < xEnd ; ++x){
                        const auto index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                        const uint16_t pixelValue = bayer[index];

                        switch(GetCFAPhase(pattern, x, y)){
                            case CFAPhase::Red:
                                rSum += pixelValue;
                                countR++;
                                break;
                            case CFAPhase::GreenRed:
                            case CFAPhase::GreenBlue:
                                gSum += pixelValue;
                                countG++;
                                break;
                            case CFAPhase::Blue:
                                bSum += pixelValue;
                                countB++;
                                break;
                        }
                    }
                }

                // Calculate average color for the patch
                PatchColorDouble& patchColor = patchColors[r * columns + c];
                patchColor.r = countR > 0 ? static_cast<double>(rSum) / countR : 0.0;
                patchColor.g = countG > 0 ? static_cast<double>(gSum) / countG : 0.0;
                patchColor.b = countB > 0 ? static_cast<double>(bSum) / countB : 0.0;


            }
        }



        return patchColors;
    }



    // Draw red rectangle on the image to visualize the detected color chart corners
    void DrawRedRectangle(std::vector<mini_isp::RGBFormat>& image, int width, int height,int xStart, int yStart , int xEnd, int yEnd, int thickness = 4) {
        xStart = std::max(0, std::min(xStart, width - 1));
        xEnd = std::max(0, std::min(xEnd, width - 1));
        yStart = std::max(0, std::min(yStart, height - 1));
        yEnd = std::max(0, std::min(yEnd, height - 1));

        const mini_isp::RGBFormat redColor{255, 0, 0};

        for(int t= 0 ; t < thickness ; ++t){
            const int top = std::min(yStart + t , yEnd);
            const int bottom = std::max(yEnd - t , yStart);
            const int left = std::min(xStart + t , xEnd);
            const int right = std::max(xEnd - t , xStart);

            for(int x = xStart ; x < xEnd ; ++x){
                image[top*width + x] = redColor;
                image[bottom*width + x] = redColor;
            }

            for(int y = yStart ; y < yEnd ; ++y){
                image[y*width + left] = redColor;
                image[y*width + right] = redColor;
            }
        }
    }

    void DrawColorChartROIs(std::vector<mini_isp::RGBFormat>& image, int width , int height, const mini_isp::ColorChartCorners& corners, int roiHalfSize){
        constexpr int columns = 6;
        constexpr int rows = 4;

        for(int r = 0 ; r < rows ; ++r){
            for(int c = 0 ; c < columns ; ++c){
                const float u = static_cast<float>(c + 0.5f) / static_cast<float>(columns);
                const float v = static_cast<float>(r + 0.5f) / static_cast<float>(rows);

                const float centerX = (1.0f - u) * (1.0f - v) * corners.topLeft.x + 
                                    u * (1.0f - v) * corners.topRight.x + 
                                    (1.0f - u) * v * corners.bottomLeft.x + 
                                    u * v * corners.bottomRight.x;
                const float centerY = (1.0f - u) * (1.0f - v) * corners.topLeft.y + 
                                    u * (1.0f - v) * corners.topRight.y + 
                                    (1.0f - u) * v * corners.bottomLeft.y + 
                                    u * v * corners.bottomRight.y;  
                
                const int centerPixelX = std::lround(centerX);
                const int centerPixelY = std::lround(centerY);
                
                DrawRedRectangle(image, width , height, centerPixelX - roiHalfSize, centerPixelY - roiHalfSize, centerPixelX + roiHalfSize, centerPixelY + roiHalfSize);

            }
        }
        

    }

    Matrix3x3 Multiply3x3(const Matrix3x3& lMat, const Matrix3x3& rMat){
        
        Matrix3x3 result{};

        for(int row = 0; row < 3 ; ++row){
            for(int col = 0 ; col < 3 ; ++col){
                for(int k = 0 ; k < 3 ; ++k){
                    result[row][col] += lMat[row][k] * rMat[k][col];
                }
            }
        }
        return result;
    }

    Matrix3x3 Inverse3x3(const Matrix3x3& mat){
        // calculate det(mat)

        const double determinant = mat[0][0] * (mat[1][1]*mat[2][2] - mat[1][2]*mat[2][1])-
                                   mat[0][1] * (mat[1][0]*mat[2][2] - mat[1][2]*mat[2][0])+
                                   mat[0][2] * (mat[1][0]*mat[2][1] - mat[1][1]*mat[2][0]);
        if(std::abs(determinant) < 1e-12){
            throw std::runtime_error("Cannot invert singular 3x3 matrix.");
        }

        Matrix3x3 inverse{};

        inverse[0][0] = (mat[1][1]*mat[2][2] - mat[1][2]*mat[2][1])/determinant;
        inverse[0][1] = -(mat[0][1]*mat[2][2] - mat[0][2]*mat[2][1])/determinant;
        inverse[0][2] = (mat[0][1]*mat[1][2] - mat[0][2]*mat[1][1])/determinant;

        inverse[1][0] = -(mat[1][0]*mat[2][2] - mat[1][2]*mat[2][0])/determinant;
        inverse[1][1] = (mat[0][0]*mat[2][2] - mat[0][2]*mat[2][0])/determinant;
        inverse[1][2] = -(mat[0][0]*mat[1][2] - mat[0][2]*mat[1][0])/determinant;

        inverse[2][0] = (mat[1][0]*mat[2][1] - mat[1][1]*mat[2][0])/determinant;
        inverse[2][1] = -(mat[0][0]*mat[2][1] - mat[0][1]*mat[2][0])/determinant;
        inverse[2][2] = (mat[0][0]*mat[1][1] - mat[0][1]*mat[1][0])/determinant;

        return inverse;
    }

    Matrix3x3 CalculateCCM(const std::array<PatchColorDouble, 24>& measuredColors, const std::array<PatchColorDouble, 24>& referenceColors, double whiteLevel){
        if (whiteLevel <= 0.0) {
            throw std::invalid_argument("White level must be positive.");
        }

        Matrix3x3 xx{}; //sum(x*xT)
        Matrix3x3 yx{}; //sum(y*xT)

        for(int measuredIdx = 0 ; measuredIdx < 24 ; ++measuredIdx){
            // remove dark and bright patches
            if(measuredIdx == 0 || measuredIdx == 5) continue;
            const int referenceIdx = 23 - measuredIdx;

            const auto& measured = measuredColors[measuredIdx];
            const auto& target = referenceColors[referenceIdx];

            // The measured values are sensor code values; references are already
            // linear RGB values normalized to [0, 1].
            const std::array<double, 3> x{
                measured.r / whiteLevel,
                measured.g / whiteLevel,
                measured.b / whiteLevel
            };

            const std::array<double, 3> y{
                target.r,
                target.g,
                target.b
            };

            // Update xx and yx matrices
            for(int row = 0 ; row < 3 ; ++row){
                for(int col = 0 ; col < 3 ; ++col){
                    xx[row][col] += x[row]*x[col];
                    yx[row][col] += y[row]*x[col];
                }
            }

        }

        // Small Tikhonov regularization keeps the normal matrix invertible.
        constexpr double epsilon = 1e-8;
        for(int i = 0 ; i < 3 ; ++i){
            xx[i][i] += epsilon;
        }

        const Matrix3x3 inverseXX = Inverse3x3(xx);
        return Multiply3x3(yx, inverseXX);
    }

    PatchColorDouble ApplyCCM(const PatchColorDouble& input, const Matrix3x3& ccm){
        return {
            ccm[0][0] * input.r + ccm[0][1] * input.g + ccm[0][2] * input.b,
            ccm[1][0] * input.r + ccm[1][1] * input.g + ccm[1][2] * input.b,
            ccm[2][0] * input.r + ccm[2][1] * input.g + ccm[2][2] * input.b
        };
    }

    void ApplyCCMImage(const std::vector<RGBFormat16>& input, std::vector<RGBFloatFormat>& output, const Matrix3x3& ccm , double whiteLevel){
        
        if(whiteLevel <= 0.0){
            throw std::invalid_argument("white level must be positive.");
        }

        output.resize(static_cast<int>(input.size()));

        for(int i = 0 ; i < static_cast<int>(input.size()); ++i){
            // normalization 
            const PatchColorDouble normalized{
                input[i].r/whiteLevel,
                input[i].g/whiteLevel,
                input[i].b/whiteLevel
            };

            const auto corrected = ApplyCCM(normalized, ccm);

            output[i] = {
                static_cast<float>(corrected.r),
                static_cast<float>(corrected.g),
                static_cast<float>(corrected.b)
            };
        }
    }

}
