// Report-only diagnostic: calls the existing 16-bit demosaicker unchanged.
// Build from the project root; output is isolated from Image/ and main.cpp.
#include "miniISP/Demosaic.h"
#include "miniISP/BayerUtils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using Pixel = std::array<double, 3>;
constexpr int width = 128, height = 96;
constexpr double peak = 65535.0;
constexpr double pi = 3.14159265358979323846;
const std::filesystem::path directory = "docs/reports/demosaic_false_color/data";

void save(const std::string& name, const std::vector<Pixel>& pixels) {
    std::ofstream file(directory / (name + ".ppm"), std::ios::binary);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (const auto& pixel : pixels) {
        for (double c : pixel) {
            c = std::clamp(c, 0.0, 1.0);
            const double encoded = c <= 0.0031308 ? 12.92 * c
                : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
            const auto code = static_cast<unsigned char>(std::lround(255.0 * encoded));
            file.write(reinterpret_cast<const char*>(&code), 1);
        }
    }
    if (!file) throw std::runtime_error("Cannot save report image");
}

// Illustrative chroma-only post-filter, not a new production ISP stage.
// Preserve G; independently median-filter signed R-G and B-G, once, 3x3.
std::vector<Pixel> chromaMedian(const std::vector<Pixel>& input) {
    auto output = input;
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            std::array<double, 9> dr{}, db{};
            int n = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const auto& p = input[(y + dy) * width + x + dx];
                    dr[n] = p[0] - p[1];
                    db[n++] = p[2] - p[1];
                }
            }
            std::sort(dr.begin(), dr.end());
            std::sort(db.begin(), db.end());
            auto& p = output[y * width + x];
            p[0] = p[1] + dr[4];
            p[2] = p[1] + db[4];
        }
    }
    return output;
}

int main() {
    std::filesystem::create_directories(directory);
    std::ofstream csv(directory / "metrics.csv");
    csv << "pattern,method,rgb_rmse_linear,chroma_error_linear,green_mae_linear,out_of_range_channel_pct\n";
    for (const std::string name : {"flat", "edge", "grating", "color_detail"}) {
        std::vector<Pixel> reference(width * height);
        std::vector<uint16_t> bayer(width * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Pixel p{};
                if (name == "flat") p = {0.3, 0.3, 0.3};
                if (name == "edge") {
                    const double v = y > 0.61 * x + 9.3 ? 0.85 : 0.08;
                    p = {v, v, v};
                }
                if (name == "grating") {
                    const double v = 0.45 + 0.35 * std::sin(2 * pi * (0.31 * x + 0.17 * y));
                    p = {v, v, v};
                }
                if (name == "color_detail") {
                    p = {0.3, 0.3, 0.3};
                    // Narrow true-color strokes: a control against indiscriminate desaturation.
                    const int band = (x - y / 2 + 200) % 16;
                    if (band < 2) p = {0.80, 0.12, 0.08};
                    if (band >= 8 && band < 10) p = {0.08, 0.25, 0.85};
                }
                // Reference includes the same 16-bit quantization as the sampled input.
                for (double& c : p) c = std::lround(c * peak) / peak;
                const int idx = y * width + x;
                reference[idx] = p;
                const auto color = mini_isp::GetCFAColor(mini_isp::BayerPattern::RGGB, x, y);
                const int channel = color == mini_isp::CFAColor::Red ? 0
                    : color == mini_isp::CFAColor::Green ? 1 : 2;
                bayer[idx] = static_cast<uint16_t>(std::lround(p[channel] * peak));
            }
        }
        std::vector<mini_isp::RGBFormat16> rgb;
        mini_isp::DemosaicBilinear16(bayer, rgb, width, height,
                                   mini_isp::BayerPattern::RGGB, 65535);
        if (rgb.size() != reference.size()) throw std::runtime_error("Demosaic failed");
        std::vector<Pixel> baseline(rgb.size());
        for (std::size_t i = 0; i < rgb.size(); ++i)
            baseline[i] = {rgb[i].r / peak, rgb[i].g / peak, rgb[i].b / peak};
        const auto filtered = chromaMedian(baseline);
        save(name + "_reference", reference);
        save(name + "_bilinear", baseline);
        save(name + "_median", filtered);
        for (int mode = 0; mode < 2; ++mode) {
            const auto& output = mode == 0 ? baseline : filtered;
            double square = 0, chroma = 0, green = 0, count = 0, outside = 0;
            // Same interior for all methods; exclude 3 pixels on each edge.
            for (int y = 3; y < height - 3; ++y) {
                for (int x = 3; x < width - 3; ++x) {
                    const int i = y * width + x;
                    for (int c = 0; c < 3; ++c) {
                        square += std::pow(output[i][c] - reference[i][c], 2);
                        outside += output[i][c] < 0 || output[i][c] > 1;
                    }
                    chroma += (std::abs((output[i][0] - output[i][1]) -
                                       (reference[i][0] - reference[i][1])) +
                               std::abs((output[i][2] - output[i][1]) -
                                       (reference[i][2] - reference[i][1]))) / 2;
                    green += std::abs(output[i][1] - reference[i][1]);
                    ++count;
                }
            }
            csv << name << ',' << (mode == 0 ? "bilinear" : "median") << ','
                << std::setprecision(10) << std::sqrt(square / (count * 3)) << ','
                << chroma / count << ',' << green / count << ','
                << outside / (3 * count) * 100 << '\n';
        }
    }
    std::cout << "Wrote four diagnostic scenes and their measurements to " << directory << '\n';
}
