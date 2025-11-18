#ifndef DCT_MATH_H
#define DCT_MATH_H

#include <vector>

namespace DctMath {
    double alpha(int u);
    double computeDctCoefficient(const std::vector<std::vector<double>>& block, int u, int v);
}

#endif