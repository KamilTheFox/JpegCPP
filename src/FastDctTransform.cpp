#include "FastDctTransform.h"
#include <cmath>
#include <vector>

using namespace std;

// Быстрое 1D DCT преобразование (алгоритм AAN)
void FastDctTransform::fastDct1d(const double* input, double* output) {
    double tmp0, tmp1, tmp2, tmp3;
    double tmp10, tmp11, tmp12, tmp13;
    double z1, z2, z3, z4, z5, z11, z13;
    
    // Этап 1
    tmp0 = input[0] + input[7];
    tmp1 = input[1] + input[6];
    tmp2 = input[2] + input[5];
    tmp3 = input[3] + input[4];
    tmp10 = input[0] - input[7];
    tmp11 = input[1] - input[6];
    tmp12 = input[2] - input[5];
    tmp13 = input[3] - input[4];
    
    // Этап 2
    output[0] = tmp0 + tmp1 + tmp2 + tmp3;
    output[2] = (tmp0 - tmp3) * 1.3870398453221475;  // c2
    output[4] = (tmp1 - tmp2) * 1.3065629648763766;  // c4
    output[6] = (tmp0 + tmp3) - (tmp1 + tmp2);
    output[6] *= 0.5411961001461969;  // c6
    
    z1 = tmp12 + tmp13;
    z1 *= 0.5411961001461969;  // c6
    output[1] = z1 + tmp13 * 1.8477590650225735;   // c1-c3
    output[3] = z1 + tmp12 * -0.7653668647301797;  // c3+c7
    output[5] = tmp11 * 1.1758756024193586 + tmp10 * 0.2758993792829431;   // c5
    output[7] = tmp10 * -1.1758756024193586 + tmp11 * 0.2758993792829431;  // c7
}

// 2D DCT через два 1D преобразования
void FastDctTransform::fastDct2d(const vector<vector<double>>& input, 
                                vector<vector<double>>& output) {
    double row[8], col[8];
    double intermediate[8][8];
    
    // DCT по строкам
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            row[j] = input[i][j];
        }
        fastDct1d(row, intermediate[i]);
    }
    
    // DCT по столбцам
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            col[i] = intermediate[i][j];
        }
        fastDct1d(col, row);
        for (int i = 0; i < 8; i++) {
            output[i][j] = row[i] * 0.125; // Масштабирующий коэффициент
        }
    }
}

vector<vector<double>> FastDctTransform::forwardDct(const vector<vector<double>>& block) {
    vector<vector<double>> result(8, vector<double>(8));
    fastDct2d(block, result);
    return result;
}

// Базовая SIMD-совместимая версия (можно заменить на интринсики)
vector<vector<double>> FastDctTransform::forwardDctSimd(const vector<vector<double>>& block) {
    return forwardDct(block); // Заглушка - можно реализовать с интринсиками
}