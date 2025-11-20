#include "image_metrics.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace std;

double ImageMetrics::meanSquaredError(const RgbImage& original, const RgbImage& reconstructed) {
    if (original.getWidth() != reconstructed.getWidth() || 
        original.getHeight() != reconstructed.getHeight()) {
        throw invalid_argument("Images must have same dimensions");
    }
    
    double mse = 0.0;
    int pixelCount = original.getPixelCount();
    
    for (int y = 0; y < original.getHeight(); y++) {
        for (int x = 0; x < original.getWidth(); x++) {
            auto [r1, g1, b1] = original.getPixel(x, y);
            auto [r2, g2, b2] = reconstructed.getPixel(x, y);
            
            double diffR = r1 - r2;
            double diffG = g1 - g2;
            double diffB = b1 - b2;
            
            mse += (diffR * diffR + diffG * diffG + diffB * diffB);
        }
    }
    
    return mse / (pixelCount * 3); // Divide by 3 for RGB channels
}

double ImageMetrics::peakSignalToNoiseRatio(const RgbImage& original, const RgbImage& reconstructed) {
    double mse = meanSquaredError(original, reconstructed);
    
    if (mse < 1e-10) {
        return 100.0; // Perfect reconstruction
    }
    
    double maxPixelValue = 255.0;
    double psnr = 10.0 * log10((maxPixelValue * maxPixelValue) / mse);
    
    return psnr;
}

double ImageMetrics::structuralSimilarityIndex(const RgbImage& original, const RgbImage& reconstructed) {
    if (original.getWidth() != reconstructed.getWidth() || 
        original.getHeight() != reconstructed.getHeight()) {
        throw invalid_argument("Images must have same dimensions");
    }
    
    // Simplified SSIM calculation (luminance only for speed)
    // Full SSIM would use sliding windows
    
    double mean1 = 0.0, mean2 = 0.0;
    double variance1 = 0.0, variance2 = 0.0;
    double covariance = 0.0;
    int pixelCount = original.getPixelCount();
    
    // Calculate means
    for (int y = 0; y < original.getHeight(); y++) {
        for (int x = 0; x < original.getWidth(); x++) {
            auto [r1, g1, b1] = original.getPixel(x, y);
            auto [r2, g2, b2] = reconstructed.getPixel(x, y);
            
            // Convert to grayscale (luminance)
            double lum1 = 0.299 * r1 + 0.587 * g1 + 0.114 * b1;
            double lum2 = 0.299 * r2 + 0.587 * g2 + 0.114 * b2;
            
            mean1 += lum1;
            mean2 += lum2;
        }
    }
    
    mean1 /= pixelCount;
    mean2 /= pixelCount;
    
    // Calculate variances and covariance
    for (int y = 0; y < original.getHeight(); y++) {
        for (int x = 0; x < original.getWidth(); x++) {
            auto [r1, g1, b1] = original.getPixel(x, y);
            auto [r2, g2, b2] = reconstructed.getPixel(x, y);
            
            double lum1 = 0.299 * r1 + 0.587 * g1 + 0.114 * b1;
            double lum2 = 0.299 * r2 + 0.587 * g2 + 0.114 * b2;
            
            double diff1 = lum1 - mean1;
            double diff2 = lum2 - mean2;
            
            variance1 += diff1 * diff1;
            variance2 += diff2 * diff2;
            covariance += diff1 * diff2;
        }
    }
    
    variance1 /= pixelCount;
    variance2 /= pixelCount;
    covariance /= pixelCount;
    
    // SSIM formula with stability constants
    double C1 = 6.5025;    // (0.01 * 255)^2
    double C2 = 58.5225;   // (0.03 * 255)^2
    
    double numerator = (2 * mean1 * mean2 + C1) * (2 * covariance + C2);
    double denominator = (mean1 * mean1 + mean2 * mean2 + C1) * (variance1 + variance2 + C2);
    
    return numerator / denominator;
}

double ImageMetrics::compressionRatio(const RgbImage& original, size_t compressedSize) {
    size_t originalSize = static_cast<size_t>(original.getWidth()) * original.getHeight() * 3;
    return static_cast<double>(originalSize) / compressedSize;
}

void ImageMetrics::printComparisonReport(const RgbImage& original, 
                                        const RgbImage& reconstructed,
                                        size_t compressedSize,
                                        const string& testName) {
    cout << "\n=== " << testName << " ===" << endl;
    cout << "Image dimensions: " << original.getWidth() << "x" << original.getHeight() << endl;
    
    size_t originalSize = static_cast<size_t>(original.getWidth()) * original.getHeight() * 3;
    cout << "Original size: " << originalSize << " bytes" << endl;
    cout << "Compressed size: " << compressedSize << " bytes" << endl;
    
    double ratio = compressionRatio(original, compressedSize);
    cout << "Compression ratio: " << fixed << setprecision(2) << ratio << ":1" << endl;
    
    double psnr = peakSignalToNoiseRatio(original, reconstructed);
    cout << "PSNR: " << fixed << setprecision(2) << psnr << " dB" << endl;
    
    double ssim = structuralSimilarityIndex(original, reconstructed);
    cout << "SSIM: " << fixed << setprecision(4) << ssim << endl;
    
    double mse = meanSquaredError(original, reconstructed);
    cout << "MSE: " << fixed << setprecision(2) << mse << endl;
    
    // Quality assessment
    cout << "Quality: ";
    if (psnr > 40) {
        cout << "Excellent (minimal loss)" << endl;
    } else if (psnr > 30) {
        cout << "Good (acceptable for most uses)" << endl;
    } else if (psnr > 20) {
        cout << "Fair (noticeable artifacts)" << endl;
    } else {
        cout << "Poor (significant artifacts)" << endl;
    }
}
