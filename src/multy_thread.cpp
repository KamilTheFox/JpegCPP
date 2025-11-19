#include "multy_thread.h"
#include <algorithm>
#include <optional>

// ===== MultiThreadColorConverter =====

MultiThreadColorConverter::MultiThreadColorConverter(int numThreads)
    : numThreads(numThreads > 0 ? numThreads : 1) {}

YCbCrImage MultiThreadColorConverter::convert(const RgbImage& image) {
    YCbCrImage result(image.getWidth(), image.getHeight());

    const int width  = image.getWidth();
    const int height = image.getHeight();

    int threads = std::min(numThreads, height);
    if (threads <= 0) {
        threads = 1;
    }

    std::vector<std::thread> workers;
    workers.reserve(threads);

    int rowsPerThread = (height + threads - 1) / threads;

    auto worker = [&](int tid) {
        int yStart = tid * rowsPerThread;
        int yEnd   = std::min(height, yStart + rowsPerThread);

        for (int y = yStart; y < yEnd; ++y) {
            for (int x = 0; x < width; ++x) {
                auto rgb   = image.getPixel(x, y);
                auto ycbcr = ColorMath::rgbToYCbCr(std::get<0>(rgb),
                                                   std::get<1>(rgb),
                                                   std::get<2>(rgb));
                result.setPixel(x, y,
                                std::get<0>(ycbcr),
                                std::get<1>(ycbcr),
                                std::get<2>(ycbcr));
            }
        }
    };

    for (int t = 0; t < threads; ++t) {
        workers.emplace_back(worker, t);
    }
    for (auto& th : workers) {
        th.join();
    }

    return result;
}

// ===== MultiThreadBlockProcessor =====

MultiThreadBlockProcessor::MultiThreadBlockProcessor(std::unique_ptr<IDctTransform> dctTransform,
                                                     std::unique_ptr<IQuantizer>    quantizer,
                                                     int numThreads)
    : dct(std::move(dctTransform))
    , quantizer(std::move(quantizer))
    , numThreads(numThreads > 0 ? numThreads : 1) {}

std::vector<std::vector<double>> MultiThreadBlockProcessor::extractBlock(
    const YCbCrImage& image, int x, int y, int component) {

    std::vector<std::vector<double>> block(8, std::vector<double>(8));

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            int px = std::min(x + j, image.getWidth()  - 1);
            int py = std::min(y + i, image.getHeight() - 1);
            auto pixel = image.getPixel(px, py);

            switch (component) {
                case 0: // Y
                    block[i][j] = std::get<0>(pixel) - 128;
                    break;
                case 1: // Cb
                    block[i][j] = std::get<1>(pixel) - 128;
                    break;
                case 2: // Cr
                    block[i][j] = std::get<2>(pixel) - 128;
                    break;
                default:
                    block[i][j] = 0;
                    break;
            }
        }
    }

    return block;
}

std::vector<QuantizedBlock> MultiThreadBlockProcessor::processBlocks(const YCbCrImage& image) {
    const int width  = image.getWidth();
    const int height = image.getHeight();

    // Кол-во блоков по каждой компоненте (как в последовательной версии)
    int nxY = (width  + 7)  / 8;
    int nyY = (height + 7)  / 8;
    int yBlocksCount = nxY * nyY;

    int nxC = (width  + 15) / 16;
    int nyC = (height + 15) / 16;
    int cbBlocksCount = nxC * nyC;
    int crBlocksCount = nxC * nyC;

    const int totalBlocks = yBlocksCount + cbBlocksCount + crBlocksCount;
    // Храним временно в optional, т.к. у QuantizedBlock нет дефолтного конструктора
    std::vector<std::optional<QuantizedBlock>> tmpBlocks(totalBlocks);

    auto processComponent = [&](int component,
                                int blocksX, int blocksY,
                                int step,
                                int offset) {
        int total = blocksX * blocksY;
        if (total == 0) return;

        int threads = std::min(numThreads, total);
        if (threads <= 0) threads = 1;

        std::vector<std::thread> workers;
        workers.reserve(threads);

        auto worker = [&](int tid) {
            for (int index = tid; index < total; index += threads) {
                int byIndex = index / blocksX;
                int bxIndex = index % blocksX;

                int bx = bxIndex * step;
                int by = byIndex * step;

                auto block     = extractBlock(image, bx, by, component);
                auto dctBlock  = dct->forwardDct(block);
                auto quantizat = quantizer->quantize(dctBlock);

                tmpBlocks[offset + index].emplace(quantizat, bxIndex, byIndex, component);
            }
        };

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back(worker, t);
        }
        for (auto& th : workers) {
            th.join();
        }
    };

    // Порядок тот же, что и в SequentialBlockProcessor:
    // сначала все Y, затем все Cb, затем все Cr.
    processComponent(0, nxY, nyY, 8,  0);
    processComponent(1, nxC, nyC, 16, yBlocksCount);
    processComponent(2, nxC, nyC, 16, yBlocksCount + cbBlocksCount);

    std::vector<QuantizedBlock> blocks;
    blocks.reserve(totalBlocks);
    for (auto &opt : tmpBlocks) {
        // Все элементы должны быть заполнены
        blocks.push_back(std::move(opt.value()));
    }

    return blocks;
}

