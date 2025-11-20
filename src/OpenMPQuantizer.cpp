#include "OpenMPQuantizer.h"
#include <omp.h>
#include <cmath>
#include <algorithm>

using namespace std;

OpenMPQuantizer::OpenMPQuantizer(int quality) 
    : quantizationTable(generateQuantizationTable(quality)) {}

vector<vector<int>> OpenMPQuantizer::quantize(const vector<vector<double>>& dctBlock) {
    vector<vector<int>> result(8, vector<int>(8));
    
    // Параллелизируем оба измерения с SIMD
    #pragma omp parallel for simd collapse(2)
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            result[i][j] = static_cast<int>(round(
                dctBlock[i][j] / quantizationTable[i][j]
            ));
        }
    }
    
    return result;
}

vector<vector<vector<int>>> 
OpenMPQuantizer::quantizeBatch(const vector<vector<vector<double>>>& dctBlocks,
                              const vector<vector<int>>& quantTable) {
    vector<vector<vector<int>>> results(dctBlocks.size());
    
    // Параллелизируем только по блокам
    #pragma omp parallel for schedule(dynamic)
    for (size_t blockIdx = 0; blockIdx < dctBlocks.size(); blockIdx++) {
        vector<vector<int>> result(8, vector<int>(8));
        const auto& block = dctBlocks[blockIdx];
        
        // Используем SIMD внутри блока
        #pragma omp simd collapse(2)
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                result[i][j] = static_cast<int>(round(
                    block[i][j] / quantTable[i][j]
                ));
            }
        }
        
        results[blockIdx] = move(result);
    }
    
    return results;
}

vector<vector<int>> OpenMPQuantizer::defaultQuantizationTable() {
    return vector<vector<int>>{
        {16, 11, 10, 16, 24, 40, 51, 61},
        {12, 12, 14, 19, 26, 58, 60, 55},
        {14, 13, 16, 24, 40, 57, 69, 56},
        {14, 17, 22, 29, 51, 87, 80, 62},
        {18, 22, 37, 56, 68, 109, 103, 77},
        {24, 35, 55, 64, 81, 104, 113, 92},
        {49, 64, 78, 87, 103, 121, 120, 101},
        {72, 92, 95, 98, 112, 100, 103, 99}
    };
}

vector<vector<int>> OpenMPQuantizer::generateQuantizationTable(int quality) {
    auto baseTable = defaultQuantizationTable();
    vector<vector<int>> result(8, vector<int>(8));
    
    double scale = quality < 50 ? 5000.0 / quality : 200.0 - 2 * quality;
    
    #pragma omp parallel for simd collapse(2)
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int value = static_cast<int>((baseTable[i][j] * scale + 50) / 100);
            result[i][j] = max(1, min(255, value));
        }
    }
    
    return result;
}