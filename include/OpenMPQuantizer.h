#ifndef OPENMP_QUANTIZER_H
#define OPENMP_QUANTIZER_H

#include "interfaces.h"
#include <vector>

class OpenMPQuantizer : public IQuantizer {
private:
    std::vector<std::vector<int>> quantizationTable;
    
    std::vector<std::vector<int>> generateQuantizationTable(int quality);

public:
    OpenMPQuantizer(int quality = 50);
    std::vector<std::vector<int>> quantize(const std::vector<std::vector<double>>& dctBlock) override;
    
    // Пакетная обработка
    static std::vector<std::vector<std::vector<int>>> 
    quantizeBatch(const std::vector<std::vector<std::vector<double>>>& dctBlocks,
                  const std::vector<std::vector<int>>& quantTable);
    
    static std::vector<std::vector<int>> defaultQuantizationTable();
    const std::vector<std::vector<int>>& getQuantizationTable() const { return quantizationTable; }
};

#endif