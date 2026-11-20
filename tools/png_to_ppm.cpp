#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <png.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: png_to_ppm input.png output.ppm\n";
        return 1;
    }

    png_image image{};
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_file(&image, argv[1])) {
        std::cerr << "Cannot read PNG: " << image.message << '\n';
        return 1;
    }

    image.format = PNG_FORMAT_RGB;
    std::vector<std::uint8_t> pixels(PNG_IMAGE_SIZE(image));

    if (!png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr)) {
        std::cerr << "Cannot decode PNG: " << image.message << '\n';
        png_image_free(&image);
        return 1;
    }

    std::ofstream output(argv[2], std::ios::binary);
    if (!output) {
        std::cerr << "Cannot create PPM: " << argv[2] << '\n';
        png_image_free(&image);
        return 1;
    }

    output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));

    png_image_free(&image);
    return output ? 0 : 1;
}
