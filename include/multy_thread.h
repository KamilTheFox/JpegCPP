#ifndef MULTY_THREAD_H
#define MULTY_THREAD_H

#include "interfaces.h"
#include "dct_math.h"
#include "color_math.h"
#include <vector>
#include <memory>
#include <thread>

// Параллельный конвертер RGB -> YCbCr
class MultiThreadColorConverter : public IColorConverter {
private:
    int numThreads;

public:
    explicit MultiThreadColorConverter(int numThreads = std::thread::hardware_concurrency());
    YCbCrImage convert(const RgbImage& image) override;
};

// Параллельный обработчик блоков (DCT + квантование)
class MultiThreadBlockProcessor : public IBlockProcessor {
private:
    std::unique_ptr<IDctTransform> dct;
    std::unique_ptr<IQuantizer>    quantizer;
    int numThreads;

    static std::vector<std::vector<double>> extractBlock(const YCbCrImage& image,
                                                         int x, int y, int component);

public:
    MultiThreadBlockProcessor(std::unique_ptr<IDctTransform> dctTransform,
                              std::unique_ptr<IQuantizer>    quantizer,
                              int numThreads = std::thread::hardware_concurrency());

    std::vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
};

#endif // MULTY_THREAD_H
