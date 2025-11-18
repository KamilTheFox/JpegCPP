#include "quantized_block.h"
#include <stdexcept>

using namespace std;

const vector<int> QuantizedBlock::zigzagIndices = {
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

QuantizedBlock::QuantizedBlock(const vector<vector<int>>& inputCoefficients, 
                              int blockX, int blockY, int component) 
    : blockX(blockX), blockY(blockY), component(component), coefficients(64) {
    
    if (inputCoefficients.size() != 8 || inputCoefficients[0].size() != 8) {
        throw invalid_argument("Block must be 8x8");
    }
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            coefficients[i * 8 + j] = inputCoefficients[i][j];
        }
    }
}

int QuantizedBlock::getCoefficient(int i, int j) const {
    return coefficients[i * 8 + j];
}

vector<int> QuantizedBlock::getZigzagOrder() const {
    return zigzagScan(coefficients);
}

vector<int> QuantizedBlock::zigzagScan(const vector<int>& coeffs) {
    vector<int> result(64);
    for (int i = 0; i < 64; i++) {
        result[i] = coeffs[zigzagIndices[i]];
    }
    return result;
}

pair<int, int> QuantizedBlock::zigzagToRowCol(int zigzagIndex) {
    int index = zigzagIndices[zigzagIndex];
    return make_pair(index / 8, index % 8);
}

vector<vector<int>> QuantizedBlock::toArray() const {
    vector<vector<int>> result(8, vector<int>(8));
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            result[i][j] = coefficients[i * 8 + j];
        }
    }
    return result;
}