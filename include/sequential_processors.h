#ifndef SEQUENTIAL_PROCESSORS_H
#define SEQUENTIAL_PROCESSORS_H

#include "interfaces.h"
#include "dct_math.h"
#include "color_math.h"
#include "huffman_math.h"
#include "bit_writer.h"
#include <vector>
#include <memory>
#include <cmath>
#include <unordered_map>

using namespace std;

class SequentialBlockProcessor : public IBlockProcessor {
private:
    unique_ptr<IDctTransform> dct;
    unique_ptr<IQuantizer> quantizer;

    vector<vector<double>> extractBlock(const YCbCrImage& image, int x, int y, int component);

public:
    SequentialBlockProcessor(unique_ptr<IDctTransform> dctTransform, 
                           unique_ptr<IQuantizer> quantizer);
    vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
};

class SequentialColorConverter : public IColorConverter {
public:
    YCbCrImage convert(const RgbImage& image) override;
};

class SequentialDctTransform : public IDctTransform {
public:
    vector<vector<double>> forwardDct(const vector<vector<double>>& block) override;
};

class SequentialHuffmanEncoder : public IHuffmanEncoder {
private:
    int lastDc = 0;
    
    void encodeBlock(BitWriter& writer, const vector<int>& zigzag,
                    const unordered_map<int, pair<int, int>>& dcTable,
                    const unordered_map<int, pair<int, int>>& acTable);
    
    static int getCategory(int value);
    static int getMagnitude(int value, int category);
    
    unordered_map<int, pair<int, int>> buildHuffmanTable(const vector<QuantizedBlock>& blocks);

public:
    JpegEncodedData encode(const vector<QuantizedBlock>& blocks, 
                          int width, int height, 
                          const vector<vector<int>>& quantTable) override;
};

class SequentialQuantizer : public IQuantizer {
private:
    vector<vector<int>> quantizationTable;

    static vector<vector<int>> generateQuantizationTable(int quality);

public:
    SequentialQuantizer(int quality = 50);
    vector<vector<int>> quantize(const vector<vector<double>>& dctBlock) override;
    
    static vector<vector<int>> defaultQuantizationTable();
    const vector<vector<int>>& getQuantizationTable() const { return quantizationTable; }
};

class JpegEncoder {
private:
    unique_ptr<IColorConverter> colorConverter;
    unique_ptr<IBlockProcessor> blockProcessor;
    unique_ptr<IHuffmanEncoder> encoder;

public:
    JpegEncoder(unique_ptr<IColorConverter> colorConv,
                unique_ptr<IBlockProcessor> blockProc,
                unique_ptr<IHuffmanEncoder> huffmanEnc);
    
    JpegEncodedData encode(const RgbImage& image);
};

#endif