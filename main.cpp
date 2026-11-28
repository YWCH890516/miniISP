#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "miniISP/InputData.h"
#include "miniISP/Demosaic.h"
#include "miniISP/AutoWhiteBalance.h"
#include "miniISP/BlackLevelCorrection.h"
#include "miniISP/DefectivePixelCorrection.h"
#include "miniISP/LensShadingCorrection.h"
#include "miniISP/HighDynamicRange.h"
#include "miniISP/ColorCalibration.h"
#include "miniISP/ImageMath.h"



template<typename Func>
double BenchmarkDemosaic(Func DemosaicFunc, int runs) {
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < runs; ++i) {
        DemosaicFunc();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count() / runs; // average time per run
}

void TestHDR(){
    std::string lowExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_low_srgb.ppm)";
    std::string midExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_mid_srgb.ppm)";
    std::string highExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_high_srgb.ppm)";

    std::vector<mini_isp::RGBFormat> lowExposure, midExposure, highExposure;
    int width, height, maxColorValuelow, maxColorValuemid, maxColorValuehigh;
    mini_isp::ReadPPM(lowExpImg, lowExposure, width, height, maxColorValuelow);
    mini_isp::ReadPPM(midExpImg, midExposure, width, height, maxColorValuemid);
    mini_isp::ReadPPM(highExpImg, highExposure, width, height, maxColorValuehigh);

    const float lowExposureTime = 1.0f / 6.0f;  // Example exposure time for low exposure
    const float midExposureTime = 1.3f;  // Exposure metadata for this test image
    const float highExposureTime = 5.0f;  // Example exposure time for high exposure
    std::vector<mini_isp::RGBFloatFormat> hdrImage;
    mini_isp::ApplyHighDynamicRange(lowExposure, midExposure, highExposure, hdrImage, width, height, lowExposureTime, midExposureTime, highExposureTime);

    std::vector<mini_isp::RGBFormat> ldrImage;
    mini_isp::ToneMapReinhard(hdrImage, ldrImage, 0.18f);

    std::string outputFile = R"(/Users/otuschen/Documents/C++/Test/Image/hdr_output2.ppm)";
    mini_isp::SavePPM(outputFile, ldrImage, width, height);

}

void TestAdaptiveHDR(){
    std::string lowExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_low_srgb.ppm)";
    std::string midExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_mid_srgb.ppm)";
    std::string highExpImg = R"(/Users/otuschen/Documents/C++/Test/Image/srgb_hdr/waterfall_high_srgb.ppm)";

    std::vector<mini_isp::RGBFormat> lowExposure, midExposure, highExposure;
    int width, height, maxColorValuelow, maxColorValuemid, maxColorValuehigh;
    mini_isp::ReadPPM(lowExpImg, lowExposure, width, height, maxColorValuelow);
    mini_isp::ReadPPM(midExpImg, midExposure, width, height, maxColorValuemid);
    mini_isp::ReadPPM(highExpImg, highExposure, width, height, maxColorValuehigh);

    std::vector<mini_isp::RGBFormat> hdrImage;
    mini_isp::AdaptiveExposureFusion(lowExposure, midExposure, highExposure, hdrImage, width, height);

    //std::vector<mini_isp::RGBFormat> ldrImage;
    //mini_isp::ToneMapReinhard(hdrImage, ldrImage, 0.18f);

    std::string outputFile = R"(/Users/otuschen/Documents/C++/Test/Image/adaptive_hdr_bilinear_penalty_output.ppm)";
    mini_isp::SavePPM(outputFile, hdrImage, width, height);
}

void TestColorCalibration(){
    // Vendor provide ColorChecker Information
    constexpr std::array<mini_isp::PatchColorDouble, 24>
    referenceEncodedRGB{{
        { 94.0,  28.0,  13.0}, //  1 Dark Skin
        {241.0, 149.0, 108.0}, //  2 Light Skin
        { 97.0, 119.0, 171.0}, //  3 Blue Sky
        { 90.0, 103.0,  39.0}, //  4 Foliage
        {164.0, 131.0, 196.0}, //  5 Blue Flower
        {140.0, 253.0, 153.0}, //  6 Bluish Green

        {255.0, 116.0,  21.0}, //  7 Orange
        {  7.0,  47.0, 122.0}, //  8 Purplish Blue
        {222.0,  29.0,  42.0}, //  9 Moderate Red
        { 69.0,   0.0,  68.0}, // 10 Purple
        {187.0, 253.0,  19.0}, // 11 Yellow Green
        {255.0, 142.0,   0.0}, // 12 Orangey Yellow

        {  0.0,   0.0, 142.0}, // 13 Blue
        { 64.0, 173.0,  38.0}, // 14 Green
        {203.0,   0.0,   0.0}, // 15 Red
        {255.0, 217.0,   0.0}, // 16 Yellow
        {207.0,   3.0, 124.0}, // 17 Magenta
        {  0.0, 148.0, 189.0}, // 18 Cyan

        {255.0, 255.0, 255.0}, // 19 White
        {249.0, 249.0, 249.0}, // 20 Neutral 8
        {180.0, 180.0, 180.0}, // 21 Neutral 6.5
        {117.0, 117.0, 117.0}, // 22 Neutral 5
        { 53.0,  53.0,  53.0}, // 23 Neutral 3.5
        {  0.0,   0.0,   0.0}  // 24 Black
    }};

    const std::string& dngPath = R"(/Users/otuschen/Documents/C++/Test/Image/colorChart400-1:15.DNG)";
    mini_isp::RawImage raw;
    bool readSuccess = mini_isp::ReadDNG(dngPath, raw);
    if (!readSuccess) {
        std::cerr << "Error: Failed to load DNG file." << std::endl;
        return;
    }

    // Black Level Correction
    std::vector<uint16_t> blcOutput;
    mini_isp::BayerBlackLevel blcChannel{
        raw.blackLevel,
        raw.blackLevel,
        raw.blackLevel,
        raw.blackLevel
    };
    mini_isp::ApplyBlackLevelCorrection16_v2(raw.visible, blcOutput, raw.width, raw.height, raw.pattern, blcChannel);

    const auto correctedWhiteLevel = static_cast<std::uint16_t>(raw.whiteLevel - raw.blackLevel);
    // Auto White Balance (AWB)
    //std::vector<uint16_t> awbOutput;
    //mini_isp::AWB_GrayWorldV2_16(blcOutput, awbOutput, raw.width, raw.height, raw.pattern, correctedWhiteLevel);
    // 24 color patches in the Macbeth ColorChecker
    mini_isp::ColorChartCorners corners{
        {1240.0F, 1060.0F}, // top-left
        {2825.0F, 1030.0F}, // top-right
        {1230.0F, 2170.0F}, // bottom-left
        {2845.0F, 2200.0F}  // bottom-right
    };


    std::array<mini_isp::PatchColorDouble, 24> patchColors = mini_isp::GetColorChartPatchColors(
        blcOutput, raw.width, raw.height, raw.pattern, corners);
    
    auto balancedPathcolor = patchColors; // Copy for balanced colors
    
    

    // use the neutral gray path (1-4) to calculate the average gray value
    double neutralR = 0.0, neutralG = 0.0, neutralB = 0.0;
    for(int i = 1 ; i < 5 ; ++i){
        neutralR += patchColors[i].r;
        neutralG += patchColors[i].g;
        neutralB += patchColors[i].b;
    }
    neutralR /= 4.0;
    neutralG /= 4.0;
    neutralB /= 4.0;

    const double gainR = neutralG / neutralR;
    const double gainG = 1.0; // reference
    const double gainB = neutralG / neutralB;

    std::cout << "Calculated AWB Gains: R Gain = " << gainR << ", G Gain = " << gainG << ", B Gain = " << gainB << std::endl;


    for(int i = 0 ; i < 24 ; ++i){
        auto& color = balancedPathcolor[i];
        color.r *= gainR;
        color.g *= gainG;
        color.b *= gainB;
        std::cout << "Patch " << i << ": R=" << color.r << ", G=" << color.g << ", B=" << color.b <<'\n'<<std::endl;
    }
    std::vector<uint16_t> awbOutput;
    mini_isp::ApplyWhiteBalanceGains16(blcOutput, awbOutput, raw.width, raw.height, raw.pattern, gainR, gainG, gainB, correctedWhiteLevel);

    // linear reference
    std::array<mini_isp::PatchColorDouble,24> referenceLinearRGB{};
    for(int i = 0 ; i < static_cast<int>(referenceEncodedRGB.size()); ++i){
        referenceLinearRGB[i].r = mini_isp::AdobeRGBToLinear(static_cast<float>(referenceEncodedRGB[i].r / 255.0));
        referenceLinearRGB[i].g = mini_isp::AdobeRGBToLinear(static_cast<float>(referenceEncodedRGB[i].g / 255.0));
        referenceLinearRGB[i].b = mini_isp::AdobeRGBToLinear(static_cast<float>(referenceEncodedRGB[i].b / 255.0));

        std::cout << "LinearRGB " << i << ": R=" << referenceLinearRGB[i].r << ", G=" << referenceLinearRGB[i].g << ", B=" << referenceLinearRGB[i].b <<'\n'<<std::endl;
    }
    
    //CCM 
    const mini_isp::Matrix3x3 ccm = mini_isp::CalculateCCM(balancedPathcolor, referenceLinearRGB, correctedWhiteLevel);
    for (const auto& row : ccm) {
    std::cout<<"CCM::"
        << row[0] << ' '
        << row[1] << ' '
        << row[2] << '\n';
    }

    for(int i = 0 ; i < 24 ; ++i){
        const mini_isp::PatchColorDouble normalizedColor{
            balancedPathcolor[i].r / correctedWhiteLevel,
            balancedPathcolor[i].g / correctedWhiteLevel,
            balancedPathcolor[i].b / correctedWhiteLevel
        };
        const auto color = mini_isp::ApplyCCM(normalizedColor, ccm);
        std::cout << "Patch " << i << ": R=" << color.r << ", G=" << color.g << ", B=" << color.b <<'\n'<<std::endl;
    }


    // Demosaic the image to visualize the color chart and draw rectangles around the detected patches
    std::vector<mini_isp::RGBFormat16> rgb16;
    mini_isp::DemosaicBilinear16(awbOutput, rgb16, raw.width, raw.height, raw.pattern, correctedWhiteLevel);
    std::vector<mini_isp::RGBFloatFormat> adobeccm;
    mini_isp::ApplyCCMImage(rgb16,adobeccm,ccm, correctedWhiteLevel);
    std::vector<mini_isp::RGBFormat> rgb8;
    mini_isp::ConvertLinearAdobeRGBToSRGB(adobeccm, rgb8);

    //mini_isp::ConvertRGB16ToRGB8(rgb16, rgb8, correctedWhiteLevel);
    //mini_isp::DrawColorChartROIs(rgb8, raw.width, raw.height, corners, 30);

    //std::string outputFile = R"(/Users/otuschen/Documents/C++/Test/Image/color_chart_visualization.ppm)";
    std::string outputFile = R"(/Users/otuschen/Documents/C++/Test/Image/ImageCCM.ppm)";
    mini_isp::SavePPM(outputFile, rgb8, raw.width, raw.height);
    std::cout << "Saved color chart visualization to " << outputFile << std::endl;
}

void testBLC(){
    // read image 
    std::string dngPath = R"(/Users/otuschen/Documents/C++/Test/Image/Map.DNG)";
    mini_isp::RawImage rawImg;
    bool readSuccess = mini_isp::ReadDNG(dngPath, rawImg);
    if(!readSuccess) {std::cout<<"error : load data fail!!" << std::endl; return;}

    std::vector<std::uint16_t> blc16;
     mini_isp::BayerBlackLevel blcChannel{
         rawImg.blackLevel,
         rawImg.blackLevel,
         rawImg.blackLevel,
         rawImg.blackLevel
     };
    mini_isp::ApplyBlackLevelCorrection16_v2(rawImg.visible, blc16, rawImg.width, rawImg.height, rawImg.pattern, blcChannel);
    std::uint16_t correctedWhiteLevel = static_cast<std::uint16_t>(rawImg.whiteLevel - rawImg.blackLevel);
    std::vector<std::uint16_t> awb16;
    mini_isp::AWB_GrayWorldV2_16(blc16, awb16, rawImg.width, rawImg.height, rawImg.pattern, correctedWhiteLevel);
    std::vector<mini_isp::RGBFormat16> rgb16;
    mini_isp::DemosaicEdgeAware16(awb16,rgb16,rawImg.width,rawImg.height,rawImg.pattern, correctedWhiteLevel);
    std::string outputFile(R"(/Users/otuschen/Documents/C++/Test/Image/ReportImage/MapYesBLCYesAWBYessRGBEdgeDemosaic.ppm)");
    std::vector<mini_isp::RGBFormat> rgb8;
    rgb8.resize(rgb16.size());

    const float scale = 1.0F / correctedWhiteLevel;

    for (std::size_t i = 0; i < rgb16.size(); ++i) {
        rgb8[i] = {
            mini_isp::LinearSRGBToUint8(rgb16[i].r * scale),
            mini_isp::LinearSRGBToUint8(rgb16[i].g * scale),
            mini_isp::LinearSRGBToUint8(rgb16[i].b * scale)
        };
    }

    mini_isp::SavePPM(
    outputFile, rgb8, rawImg.width, rawImg.height);

    std::cout<<"Read DNG is "<< readSuccess << std::endl;

}

int main(){

    // // ======== Raw Domain Processing ======== //
    // //std::string dng_path = R"(/Users/otuschen/Documents/C++/Test/Image/black.DNG)"; 
    // std::string dng_path = R"(/Users/otuschen/Documents/C++/Test/Image/lsc_flatfield_synthetic.dng)";
    // mini_isp::RawImage raw;
    // bool readSuccess = mini_isp::ReadDNG(dng_path,raw);
    // if(!readSuccess) {
    //     std::cout<<"error : load data fail!!" << std::endl;
    //     return -1;
    // }
    // std::vector<mini_isp::SyntheticDefect> defects = mini_isp::GenerateRandomHotPixel(raw.width,raw.height,1000);

    // // // create defect to simulate 
    // // std::vector<uint16_t> defect16;
    // // mini_isp::InjectSyntheticDefect16(raw.visible, defect16, raw.width, raw.height, raw.blackLevel, raw.whiteLevel, defects);
    // // // Defect Pixel Correction 
    // // std::vector<uint16_t> dpc16;
    // // mini_isp::DefectivePixelCorrection16_v1(defect16, dpc16, raw.width, raw.height, raw.pattern, 500);

    // std::vector<uint16_t> blc16;
    // //ApplyBlackLevelCorrection16_v1(raw.visible,blc16,raw.width,raw.height,raw.blackLevel);
    // mini_isp::BayerBlackLevel blcChannel{
    //     raw.blackLevel,
    //     raw.blackLevel,
    //     raw.blackLevel,
    //     raw.blackLevel
    // };
    // mini_isp::ApplyBlackLevelCorrection16_v2(raw.visible,blc16,raw.width,raw.height,raw.pattern, blcChannel);

    // const auto correctedWhiteLevel =static_cast<std::uint16_t>(raw.whiteLevel - raw.blackLevel);

    // mini_isp::LensShadingCorrectionMap lscMap;
    // mini_isp::BuildLensShadingMap(blc16, raw.width, raw.height, raw.pattern, 17, 13, lscMap);
    // std::vector<uint16_t> lsc16;
    // mini_isp::ApplyLensShadingCorrectionV2(blc16, lsc16, raw.width, raw.height, raw.pattern, correctedWhiteLevel, lscMap);

    // std::vector<uint16_t> awb16;
    // //mini_isp::AWB_GrayWorldV2_16(blc16, awb16, raw.width, raw.height, raw.pattern, correctedWhiteLevel);
    // std::vector<mini_isp::RGBFormat16> rgb16;
    // mini_isp::DemosaicBilinear16(lsc16,rgb16,raw.width,raw.height,raw.pattern, correctedWhiteLevel);

    // std::string outputFile(R"(/Users/otuschen/Documents/C++/Test/Image/LSC2.ppm)");
    // std::vector<mini_isp::RGBFormat> rgb8;
    // mini_isp::ConvertRGB16ToRGB8(rgb16,rgb8,correctedWhiteLevel);
    // mini_isp::SavePPM(outputFile,rgb8,raw.width,raw.height);

    // std::cout<<"Read DNG is "<< readSuccess << std::endl;
    // // =========== //

    // ======== HDR Processing ======== //
    // TestAdaptiveHDR();
    // 
    // ======== Color Calibration ======== //
    //TestColorCalibration();

    // =========Test Report ==============//
    testBLC();
    
    

    // std::string filePath = R"(/Users/otuschen/Documents/C++/Test/Image/tree.ppm)";
    // std::vector<RGBFormat> pixels;
    // int width, height, maxColorValue;
    // ReadPPM(filePath, pixels, width, height, maxColorValue);
    // std::vector<uint8_t> bayerRaw;
    // RGBToBayerRaw(pixels, bayerRaw, width, height);
    // std::vector<RGBFormat> demosaicedPixels;
    // DemosaicByBayer_Optimal(bayerRaw, demosaicedPixels, width, height);
    // std::string outfilePath = R"(/Users/otuschen/Documents/C++/Test/Image/tree_demosaiced.ppm)";
    // SavePPM(outfilePath, demosaicedPixels, width, height);
    // DemosaicByBayer_EdgeAware(bayerRaw, demosaicedPixels, width, height);
    // std::string outfilePathEdgeAware = R"(/Users/otuschen/Documents/C++/Test/Image/tree_demosaiced_edge_aware.ppm)";
    // SavePPM(outfilePathEdgeAware, demosaicedPixels, width, height);
    // DemosaicByBayer_EdgeAwareV2(bayerRaw,demosaicedPixels, width, height);
    // std::string outfilePathEdgeAwareV2 = R"(/Users/otuschen/Documents/C++/Test/Image/tree_demosaiced_edge_awareV2.ppm)";
    // SavePPM(outfilePathEdgeAwareV2, demosaicedPixels, width, height);
    

    // // Test AWB 
    // std::vector<uint8_t> bayerAWB;
    // AWB_GrayWorld(bayerRaw, bayerAWB, width, height);
    // DemosaicByBayer_EdgeAwareV2(bayerAWB, demosaicedPixels, width, height);
    // std::string outfilePathEdgeAwareV2BYAWB = R"(/Users/otuschen/Documents/C++/Test/Image/tree_demosaiced_edge_awareV2AWB.ppm)";
    // SavePPM(outfilePathEdgeAwareV2BYAWB, demosaicedPixels, width, height);
    // // Test Gary World V2 
    // AWB_GrayWorldV2(bayerRaw, bayerAWB, width, height);
    // DemosaicByBayer_EdgeAwareV2(bayerAWB, demosaicedPixels, width, height);
    // std::string outfilePathEdgeAwareV2BYAWBV2 = R"(/Users/otuschen/Documents/C++/Test/Image/tree_demosaiced_edge_awareV2AWBV2.ppm)";
    // SavePPM(outfilePathEdgeAwareV2BYAWBV2, demosaicedPixels, width, height);





    // double pixelArratTime = BenchmarkDemosaic([&](){ DemosaicByBayer(bayerRaw, demosaicedPixels, width, height); }, 100);
    // double pointerTime = BenchmarkDemosaic([&](){ DemosaicByBayer_Pointer(bayerRaw, demosaicedPixels, width, height); }, 100);
    // double optimalTime = BenchmarkDemosaic([&](){ DemosaicByBayer_Optimal(bayerRaw, demosaicedPixels, width, height); }, 100);
    // double edgeAwareTime = BenchmarkDemosaic([&](){ DemosaicByBayer_EdgeAware(bayerRaw, demosaicedPixels, width, height); }, 100);
    // double edgeAwareV2Time = BenchmarkDemosaic([&](){ DemosaicByBayer_EdgeAwareV2(bayerRaw,demosaicedPixels, width, height); }, 100);
    // std::cout << "Average time for DemosaicByBayer: " << pixelArratTime << " seconds\n";
    // std::cout << "Average time for DemosaicByBayer_Pointer: " << pointerTime << " seconds\n";
    // std::cout << "Average time for DemosaicByBayer_Optimal: " << optimalTime << " seconds\n";
    // std::cout << "Average time for DemosaicByBayer_EdgeAware: " << edgeAwareTime << " seconds\n";
    // std::cout << "Average time for DemosaicByBayer_EdgeAwareV2: " << edgeAwareV2Time << " seconds\n";
    // std::cout << "Width: " << width << '\n';
    // std::cout << "Height: " << height << '\n';
    // std::cout << "Max Color Value: " << maxColorValue << '\n';
    // std::cout << "pixels Count: " << pixels.size() << '\n';
    // std::cout <<"RGB Format Size: " << sizeof(RGBFormat) << '\n';
    // std::cout << "Bayer Raw pixel Count: " << bayerRaw.size() << '\n';
    // std::cout << "Bayer Raw pixel Size: " << sizeof(uint8_t) << '\n';

    return 0;
}
