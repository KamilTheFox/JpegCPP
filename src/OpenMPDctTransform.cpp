#include "OpenMPDctTransform.h"
#include "dct_math.h"
#include <omp.h>

using namespace std;

vector<vector<double>> OpenMPDctTransform::forwardDct(const vector<vector<double>>& block) {
    vector<vector<double>> result(8, vector<double>(8));
    
    // Параллелизируем с SIMD
    #pragma omp parallel for simd collapse(2)
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            result[u][v] = DctMath::computeDctCoefficient(block, u, v);
        }
    }
    
    return result;
}

vector<vector<vector<double>>> 
OpenMPDctTransform::forwardDctBatch(const vector<vector<vector<double>>>& blocks) {
    vector<vector<vector<double>>> results(blocks.size());
    
    // Параллелизируем только по блокам, без вложенного параллелизма
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < blocks.size(); i++) {
        vector<vector<double>> result(8, vector<double>(8));
        
        // Внутри блока используем SIMD без дополнительного параллелизма
        #pragma omp simd collapse(2)
        for (int u = 0; u < 8; u++) {
            for (int v = 0; v < 8; v++) {
                result[u][v] = DctMath::computeDctCoefficient(blocks[i], u, v);
            }
        }
        
        results[i] = move(result);
    }
    
    return results;
}
