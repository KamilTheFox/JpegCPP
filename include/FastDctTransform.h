#ifndef FAST_DCT_TRANSFORM_H
#define FAST_DCT_TRANSFORM_H

#include "interfaces.h"
#include <vector>

class FastDctTransform : public IDctTransform {
private:
    static void fastDct1d(const double* input, double* output);
    static void fastDct2d(const std::vector<std::vector<double>>& input, 
                         std::vector<std::vector<double>>& output);

public:
    std::vector<std::vector<double>> forwardDct(const std::vector<std::vector<double>>& block) override;
    
    // SIMD-ускоренная версия (если доступно)
    static std::vector<std::vector<double>> forwardDctSimd(const std::vector<std::vector<double>>& block);
};

#endif