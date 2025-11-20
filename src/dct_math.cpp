#include "dct_math.h"
#include <cmath>
#include <vector>
#include <omp.h>

using namespace std;

namespace DctMath {
    
    static vector<vector<double>> precomputeCosines() {
        vector<vector<double>> cache(8, vector<double>(8));
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                cache[i][j] = cos((2 * i + 1) * j * M_PI / 16.0);
            }
        }
        return cache;
    }
    
    static const vector<vector<double>> cosineCache = precomputeCosines();
    
    double alpha(int u) {
        return u == 0 ? 1.0 / sqrt(2) : 1.0;
    }
    
    double computeDctCoefficient(const vector<vector<double>>& block, int u, int v) {
        double sum = 0.0;
        
        // SIMD векторизация внутренних циклов
        #pragma omp simd reduction(+:sum) collapse(2)
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                sum += block[x][y] * cosineCache[x][u] * cosineCache[y][v];
            }
        }
        
        return 0.25 * alpha(u) * alpha(v) * sum;
    }
}
