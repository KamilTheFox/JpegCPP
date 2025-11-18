#include "color_math.h"
#include <cmath>

std::tuple<unsigned char, unsigned char, unsigned char> 
ColorMath::rgbToYCbCr(unsigned char r, unsigned char g, unsigned char b) {
    
    double y = 0.299 * r + 0.587 * g + 0.114 * b;
    double cb = 128 - 0.168736 * r - 0.331264 * g + 0.5 * b;
    double cr = 128 + 0.5 * r - 0.418688 * g - 0.081312 * b;
    
    // Ограничиваем значения 0-255
    y = std::max(0.0, std::min(255.0, y));
    cb = std::max(0.0, std::min(255.0, cb));
    cr = std::max(0.0, std::min(255.0, cr));
    
    return {static_cast<unsigned char>(y), 
            static_cast<unsigned char>(cb), 
            static_cast<unsigned char>(cr)};
}