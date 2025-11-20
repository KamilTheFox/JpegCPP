#ifndef OPENMP_DCT_TRANSFORM_H
#define OPENMP_DCT_TRANSFORM_H

#include "interfaces.h"
#include <vector>

class OpenMPDctTransform : public IDctTransform {
public:
    std::vector<std::vector<double>> forwardDct(const std::vector<std::vector<double>>& block) override;
    
    // Пакетная обработка с OpenMP
    static std::vector<std::vector<std::vector<double>>> 
    forwardDctBatch(const std::vector<std::vector<std::vector<double>>>& blocks);
};

#endif
