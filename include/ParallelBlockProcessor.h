#ifndef PARALLEL_BLOCK_PROCESSOR_H
#define PARALLEL_BLOCK_PROCESSOR_H

#include "interfaces.h"
#include "ParallelDctTransform.h"
#include "ParallelQuantizer.h"
#include <vector>
#include <future>

class ParallelBlockProcessor : public IBlockProcessor {
private:
    std::unique_ptr<ParallelDctTransform> dct;
    std::unique_ptr<ParallelQuantizer> quantizer;

    std::vector<std::vector<double>> extractBlock(const YCbCrImage& image, int x, int y, int component);

public:
    ParallelBlockProcessor(std::unique_ptr<ParallelDctTransform> dctTransform, 
                         std::unique_ptr<ParallelQuantizer> quantizer,
                         int threadCount = std::thread::hardware_concurrency());
    
    std::vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
    
    // Pipeline-ориентированная версия
    std::vector<std::future<QuantizedBlock>> processBlocksAsync(const YCbCrImage& image);
};

#endif