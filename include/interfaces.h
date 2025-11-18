#ifndef INTERFACES_H
#define INTERFACES_H

#include "image_types.h"
#include "quantized_block.h"
#include <vector>
#include <unordered_map>

class IBlockProcessor {
public:
    virtual ~IBlockProcessor() = default;
    virtual std::vector<QuantizedBlock> processBlocks(const YCbCrImage& image) = 0;
};

class IColorConverter {
public:
    virtual ~IColorConverter() = default;
    virtual YCbCrImage convert(const RgbImage& image) = 0;
};

class IDctTransform {
public:
    virtual ~IDctTransform() = default;
    virtual std::vector<std::vector<double>> forwardDct(const std::vector<std::vector<double>>& block) = 0;
};

class IQuantizer {
public:
    virtual ~IQuantizer() = default;
    virtual std::vector<std::vector<int>> quantize(const std::vector<std::vector<double>>& dctBlock) = 0;
};

struct HuffmanTable {
    std::unordered_map<int, std::pair<int, int>> dcLuminanceTable;
    std::unordered_map<int, std::pair<int, int>> acLuminanceTable;
};

class IHuffmanEncoder {
public:
    virtual ~IHuffmanEncoder() = default;
    virtual JpegEncodedData encode(const std::vector<QuantizedBlock>& blocks, 
                                  int width, int height, 
                                  const std::vector<std::vector<int>>& quantTable) = 0;
};

#endif