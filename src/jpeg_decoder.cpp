#include "jpeg_decoder.h"
#include "sequential_processors.h"
#include "dct_math.h"
#include "color_math.h"
#include <cmath>
#include <algorithm>

using namespace std;

// Inverse DCT implementation
vector<vector<double>> SequentialDctInverseTransform::inverseDct(const vector<vector<int>>& quantizedBlock) {
    vector<vector<double>> result(8, vector<double>(8));
    
    // Precompute cosines for efficiency
    static vector<vector<double>> cosineCache;
    static bool cacheInitialized = false;
    
    if (!cacheInitialized) {
        cosineCache.resize(8, vector<double>(8));
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                cosineCache[i][j] = cos((2 * i + 1) * j * M_PI / 16.0);
            }
        }
        cacheInitialized = true;
    }
    
    // Inverse DCT formula
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double sum = 0.0;
            
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    double alpha_u = (u == 0) ? 1.0 / sqrt(2) : 1.0;
                    double alpha_v = (v == 0) ? 1.0 / sqrt(2) : 1.0;
                    
                    sum += alpha_u * alpha_v * quantizedBlock[u][v] * 
                           cosineCache[x][u] * cosineCache[y][v];
                }
            }
            
            result[x][y] = sum / 4.0;
        }
    }
    
    return result;
}

// JpegDecoder implementation
JpegDecoder::JpegDecoder(vector<vector<int>> quantTable)
    : idct(make_unique<SequentialDctInverseTransform>()),
      quantizationTable(move(quantTable)) {}

vector<vector<double>> JpegDecoder::dequantize(const vector<vector<int>>& quantized) {
    vector<vector<double>> result(8, vector<double>(8));
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            result[i][j] = quantized[i][j] * quantizationTable[i][j];
        }
    }
    
    return result;
}

RgbImage JpegDecoder::ycbcrToRgb(const YCbCrImage& ycbcr) {
    int width = ycbcr.getWidth();
    int height = ycbcr.getHeight();
    RgbImage rgb(width, height);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto [yVal, cbVal, crVal] = ycbcr.getPixel(x, y);
            
            // YCbCr to RGB conversion
            double yD = yVal;
            double cbD = cbVal - 128;
            double crD = crVal - 128;
            
            double r = yD + 1.402 * crD;
            double g = yD - 0.344136 * cbD - 0.714136 * crD;
            double b = yD + 1.772 * cbD;
            
            // Clamp to 0-255
            r = max(0.0, min(255.0, r));
            g = max(0.0, min(255.0, g));
            b = max(0.0, min(255.0, b));
            
            rgb.setPixel(x, y, 
                        static_cast<unsigned char>(r),
                        static_cast<unsigned char>(g),
                        static_cast<unsigned char>(b));
        }
    }
    
    return rgb;
}

RgbImage JpegDecoder::decode(const JpegEncodedData& encodedData) {
    int width = encodedData.width;
    int height = encodedData.height;
    
    // Create YCbCr image
    YCbCrImage ycbcr(width, height);
    
    // Decode blocks (simplified - assumes blocks are in order)
    // In real implementation, would need to decode Huffman stream
    // For now, we'll use a placeholder that reconstructs from quantized blocks
    
    // This is a simplified version - real decoder would parse Huffman stream
    // For demonstration, we assume blocks are available in some form
    
    // Process each component
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            // Create empty quantized block (placeholder)
            vector<vector<int>> quantizedBlock(8, vector<int>(8, 0));
            
            // Dequantize
            auto dequantized = dequantize(quantizedBlock);
            
            // Inverse DCT
            auto spatial = idct->inverseDct(quantizedBlock);
            
            // Place back into image (add 128 offset back)
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    int px = min(bx + j, width - 1);
                    int py = min(by + i, height - 1);
                    
                    double val = spatial[i][j] + 128;
                    val = max(0.0, min(255.0, val));
                    
                    // Simplified: only Y component for now
                    ycbcr.setPixel(px, py, 
                                 static_cast<unsigned char>(val), 128, 128);
                }
            }
        }
    }
    
    // Convert YCbCr to RGB
    return ycbcrToRgb(ycbcr);
}

// Helper functions
unique_ptr<JpegEncoder> createJpegEncoder(int quality) {
    auto colorConverter = make_unique<SequentialColorConverter>();
    auto dctTransform = make_unique<SequentialDctTransform>();
    auto quantizer = make_unique<SequentialQuantizer>(quality);
    auto blockProcessor = make_unique<SequentialBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    return make_unique<JpegEncoder>(
        move(colorConverter), move(blockProcessor), move(huffmanEncoder));
}

unique_ptr<JpegDecoder> createJpegDecoder(const vector<vector<int>>& quantTable) {
    return make_unique<JpegDecoder>(quantTable);
}
