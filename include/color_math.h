#include <tuple>
#ifndef COLOR_MATH_H
#define COLOR_MATH_H

namespace ColorMath {
    std::tuple<unsigned char, unsigned char, unsigned char> rgbToYCbCr(
        unsigned char r, unsigned char g, unsigned char b);
}

#endif