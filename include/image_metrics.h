#ifndef IMAGE_METRICS_H
#define IMAGE_METRICS_H

#include "image_types.h"
#include <string>

class ImageMetrics {
public:
    // PSNR (Peak Signal-to-Noise Ratio) in dB
    static double peakSignalToNoiseRatio(const RgbImage& original, const RgbImage& reconstructed);
    
    // SSIM (Structural Similarity Index)
    static double structuralSimilarityIndex(const RgbImage& original, const RgbImage& reconstructed);
    
    // MSE (Mean Squared Error)
    static double meanSquaredError(const RgbImage& original, const RgbImage& reconstructed);
    
    // Compression ratio
    static double compressionRatio(const RgbImage& original, size_t compressedSize);
    
    // Print comprehensive comparison report
    static void printComparisonReport(const RgbImage& original, 
                                     const RgbImage& reconstructed,
                                     size_t compressedSize,
                                     const std::string& testName);
};

#endif
