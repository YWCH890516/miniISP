cmake_minimum_required(VERSION 3.20)

project(miniISP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw)

# Homebrew LibRaw 0.22 advertises the obsolete `-lstdc++` on macOS.
# Apple Clang already links libc++, so remove only that redundant item.
if(APPLE)
    get_target_property(LIBRAW_INTERFACE_LIBRARIES
        PkgConfig::LIBRAW INTERFACE_LINK_LIBRARIES)
    list(FILTER LIBRAW_INTERFACE_LIBRARIES EXCLUDE REGEX "stdc\\+\\+")
    set_target_properties(PkgConfig::LIBRAW PROPERTIES
        INTERFACE_LINK_LIBRARIES "${LIBRAW_INTERFACE_LIBRARIES}")
endif()

add_library(miniisp_core
    src/InputData.cpp
    src/BayerUtils.cpp
    src/BlackLevelCorrection.cpp
    src/DefectivePixelCorrection.cpp
    src/AutoWhiteBalance.cpp
    src/Demosaic.cpp
    src/LensShadingCorrection.cpp
    src/HighDynamicRange.cpp
    src/ColorCalibration.cpp
    src/ImageMath.cpp
)

target_include_directories(miniisp_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(miniisp_core
    PUBLIC
        PkgConfig::LIBRAW
)

target_compile_options(miniisp_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(miniISP main.cpp)
target_link_libraries(miniISP PRIVATE miniisp_core)
target_compile_options(miniISP PRIVATE -Wall -Wextra -Wpedantic)

# Reusable command-line demo kept separate from the experimental main.cpp.
add_executable(miniisp_cli tools/miniisp_cli.cpp)
target_link_libraries(miniisp_cli PRIVATE miniisp_core)
target_compile_options(miniisp_cli PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(miniisp_tests tests/miniisp_tests.cpp)
    target_link_libraries(miniisp_tests PRIVATE miniisp_core)
    target_compile_options(miniisp_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME miniisp_tests COMMAND miniisp_tests)
    add_test(NAME miniisp_cli_help COMMAND miniisp_cli --help)
endif()
