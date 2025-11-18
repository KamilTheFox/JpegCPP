#ifndef QUANTIZED_BLOCK_H
#define QUANTIZED_BLOCK_H

#include <vector>

class QuantizedBlock {
private:
    std::vector<int> coefficients; // 64 квантованных коэффициента
    int blockX;
    int blockY;
    int component; // 0 = Y, 1 = Cb, 2 = Cr
    
    static const std::vector<int> zigzagIndices;
    static std::vector<int> zigzagScan(const std::vector<int>& coefficients);

public:
    QuantizedBlock(const std::vector<std::vector<int>>& coefficients, 
                  int blockX = 0, int blockY = 0, int component = 0);
    
    int getBlockX() const { return blockX; }
    int getBlockY() const { return blockY; }
    int getComponent() const { return component; }
    int getCoefficient(int i, int j) const;
    
    std::vector<int> getZigzagOrder() const;
    static std::pair<int, int> zigzagToRowCol(int zigzagIndex);
    std::vector<std::vector<int>> toArray() const;
};

#endif