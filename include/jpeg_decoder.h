#ifndef JPEG_DECODER_H
#define JPEG_DECODER_H

#include "image_types.h"
#include "interfaces.h"
#include "quantized_block.h"
#include <vector>
#include <memory>

class IDctInverseTransform {
public:
    virtual ~IDctInverseTransform() = default;
    virtual std::vector<std::vector<double>> inverseDct(const std::vector<std::vector<int>>& quantizedBlock) = 0;
};

class SequentialDctInverseTransform : public IDctInverseTransform {
public:
    std::vector<std::vector<double>> inverseDct(const std::vector<std::vector<int>>& quantizedBlock) override;
};

class JpegDecoder {
private:
    std::unique_ptr<IDctInverseTransform> idct;
    std::vector<std::vector<int>> quantizationTable;
    
    std::vector<std::vector<double>> dequantize(const std::vector<std::vector<int>>& quantized);
    RgbImage ycbcrToRgb(const YCbCrImage& ycbcr);
    
public:
    JpegDecoder(std::vector<std::vector<int>> quantTable);
    RgbImage decode(const JpegEncodedData& encodedData);
};

// Helper functions
std::unique_ptr<JpegEncoder> createJpegEncoder(int quality = 75);
std::unique_ptr<JpegDecoder> createJpegDecoder(const std::vector<std::vector<int>>& quantTable);

#endif
