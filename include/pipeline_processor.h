// pipeline_processors.h
#ifndef PIPELINE_PROCESSORS_H
#define PIPELINE_PROCESSORS_H

#include "interfaces.h"
#include "dct_math.h"
#include "color_math.h"
#include "huffman_math.h"
#include "bit_writer.h"
#include <vector>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

using namespace std;

// ????????? ??? ???????? ?????? ????? ??????? ?????????
struct PipelineBlock {
    vector<vector<double>> block;
    int x, y, component;
};

struct DctBlock {
    vector<vector<double>> dctCoeffs;
    int x, y, component;
};

struct QuantizedBlockData {
    vector<vector<int>> quantized;
    int x, y, component;
};

// ??????????? ?????????? ??????
class PipelineBlockProcessor : public IBlockProcessor {
private:
    unique_ptr<IDctTransform> dct;
    unique_ptr<IQuantizer> quantizer;
    
    vector<vector<double>> extractBlock(const YCbCrImage& image, int x, int y, int component);

public:
    PipelineBlockProcessor(unique_ptr<IDctTransform> dctTransform, 
                          unique_ptr<IQuantizer> quantizer);
    vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
};

// ??????????? ????????? ??????
class PipelineColorConverter : public IColorConverter {
public:
    YCbCrImage convert(const RgbImage& image) override;
};

// ??????????? DCT ??????????????
class PipelineDctTransform : public IDctTransform {
public:
    vector<vector<double>> forwardDct(const vector<vector<double>>& block) override;
};

// ??????????? ?????????? ????????
class PipelineHuffmanEncoder : public IHuffmanEncoder {
private:
    struct ComponentData {
        vector<QuantizedBlock> blocks;
        unordered_map<int, pair<int, int>> huffmanTable;
    };
    
    int lastDc = 0;
    
    void encodeBlock(BitWriter& writer, const vector<int>& zigzag,
                    const unordered_map<int, pair<int, int>>& dcTable,
                    const unordered_map<int, pair<int, int>>& acTable);
    
    static int getCategory(int value);
    static int getMagnitude(int value, int category);
    
    unordered_map<int, pair<int, int>> buildHuffmanTable(const vector<QuantizedBlock>& blocks);
    ComponentData processComponent(const vector<QuantizedBlock>& blocks, const string& name);

public:
    JpegEncodedData encode(const vector<QuantizedBlock>& blocks, 
                          int width, int height, 
                          const vector<vector<int>>& quantTable) override;
};

// ??????????? ????????????
class PipelineQuantizer : public IQuantizer {
private:
    vector<vector<int>> quantizationTable;

    static vector<vector<int>> generateQuantizationTable(int quality);

public:
    PipelineQuantizer(int quality = 50);
    vector<vector<int>> quantize(const vector<vector<double>>& dctBlock) override;
    
    static vector<vector<int>> defaultQuantizationTable();
    const vector<vector<int>>& getQuantizationTable() const { return quantizationTable; }
};

// ???????? ???????? ??? ??????????? ???????
class ProcessingPipeline {
private:
    struct StageData {
        atomic<bool> finished{false};
        condition_variable cv;
        mutex mtx;
    };

    YCbCrImage ycbcrImage;
    vector<QuantizedBlock> finalBlocks;
    
    // ??????? ??? ???????? ?????? ????? ???????
    queue<PipelineBlock> extractionQueue;
    queue<DctBlock> dctQueue;
    queue<QuantizedBlockData> quantizationQueue;
    
    // ???????? ? ???????? ??????????
    mutex extractionMutex, dctMutex, quantizationMutex, finalMutex;
    condition_variable extractionCV, dctCV, quantizationCV;
    atomic<bool> extractionFinished{false}, dctFinished{false}, quantizationFinished{false};
    
    // ??????
    vector<thread> extractionThreads;
    vector<thread> dctThreads;
    vector<thread> quantizationThreads;
    
    unique_ptr<IDctTransform> dct;
    unique_ptr<IQuantizer> quantizer;
    int numThreads;

    void extractionStage();
    void dctStage();
    void quantizationStage();
    void processBlockBatch(int startX, int startY, int endX, int endY, int component, int blockSize);

public:
    ProcessingPipeline(unique_ptr<IDctTransform> dctTransform,
                      unique_ptr<IQuantizer> quantizerObj,
                      int threadCount = thread::hardware_concurrency());
    ~ProcessingPipeline();
    
    vector<QuantizedBlock> processImage(const YCbCrImage& image);
};

// ??????????? JPEG ???????
class PipelineJpegEncoder {
private:
    unique_ptr<IColorConverter> colorConverter;
    unique_ptr<ProcessingPipeline> pipeline;
    unique_ptr<IHuffmanEncoder> encoder;

public:
    PipelineJpegEncoder(unique_ptr<IColorConverter> colorConv,
                       unique_ptr<IDctTransform> dctTransform,
                       unique_ptr<IQuantizer> quantizer,
                       unique_ptr<IHuffmanEncoder> huffmanEnc,
                       int threadCount = thread::hardware_concurrency());
    
    JpegEncodedData encode(const RgbImage& image);
};

#endif