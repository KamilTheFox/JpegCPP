#include "multy_thread.h"
#include <algorithm>
#include <optional>

using namespace std;

// ===== MultiThreadColorConverter =====

MultiThreadColorConverter::MultiThreadColorConverter(int numThreads)
    : numThreads(numThreads > 0 ? numThreads : 1) {}

YCbCrImage MultiThreadColorConverter::convert(const RgbImage& image) {
    YCbCrImage result(image.getWidth(), image.getHeight());

    const int width  = image.getWidth();
    const int height = image.getHeight();

    int threads = min(numThreads, height);
    if (threads <= 0) {
        threads = 1;
    }

    vector<thread> workers;
    workers.reserve(threads);

    int rowsPerThread = (height + threads - 1) / threads;

    auto worker = [&](int tid) {
        int yStart = tid * rowsPerThread;
        int yEnd   = min(height, yStart + rowsPerThread);

        for (int y = yStart; y < yEnd; ++y) {
            for (int x = 0; x < width; ++x) {
                auto rgb   = image.getPixel(x, y);
                auto ycbcr = ColorMath::rgbToYCbCr(get<0>(rgb),
                                                   get<1>(rgb),
                                                   get<2>(rgb));
                result.setPixel(x, y,
                                get<0>(ycbcr),
                                get<1>(ycbcr),
                                get<2>(ycbcr));
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

MultiThreadBlockProcessor::MultiThreadBlockProcessor(unique_ptr<IDctTransform> dctTransform,
                                                     unique_ptr<IQuantizer>    quantizer,
                                                     int numThreads)
    : dct(move(dctTransform))
    , quantizer(move(quantizer))
    , numThreads(numThreads > 0 ? numThreads : 1) {}

vector<vector<double>> MultiThreadBlockProcessor::extractBlock(
    const YCbCrImage& image, int x, int y, int component) {

    vector<vector<double>> block(8, vector<double>(8));

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            int px = min(x + j, image.getWidth()  - 1);
            int py = min(y + i, image.getHeight() - 1);
            auto pixel = image.getPixel(px, py);

            switch (component) {
                case 0: // Y
                    block[i][j] = get<0>(pixel) - 128;
                    break;
                case 1: // Cb
                    block[i][j] = get<1>(pixel) - 128;
                    break;
                case 2: // Cr
                    block[i][j] = get<2>(pixel) - 128;
                    break;
                default:
                    block[i][j] = 0;
                    break;
            }
        }
    }

    return block;
}

vector<QuantizedBlock> MultiThreadBlockProcessor::processBlocks(const YCbCrImage& image) {
    const int width  = image.getWidth();
    const int height = image.getHeight();

    // ÐšÐ¾Ð»-Ð²Ð¾ Ð±Ð»Ð¾ÐºÐ¾Ð² Ð¿Ð¾ ÐºÐ°Ð¶Ð´Ð¾Ð¹ ÐºÐ¾Ð¼Ð¿Ð¾Ð½ÐµÐ½Ñ‚Ðµ (ÐºÐ°Ðº Ð² Ð¿Ð¾ÑÐ»ÐµÐ´Ð¾Ð²Ð°Ñ‚ÐµÐ»ÑŒÐ½Ð¾Ð¹ Ð²ÐµÑ€ÑÐ¸Ð¸)
    int nxY = (width  + 7)  / 8;
    int nyY = (height + 7)  / 8;
    int yBlocksCount = nxY * nyY;

    int nxC = (width  + 15) / 16;
    int nyC = (height + 15) / 16;
    int cbBlocksCount = nxC * nyC;
    int crBlocksCount = nxC * nyC;

    const int totalBlocks = yBlocksCount + cbBlocksCount + crBlocksCount;
    // Ð¥Ñ€Ð°Ð½Ð¸Ð¼ Ð²Ñ€ÐµÐ¼ÐµÐ½Ð½Ð¾ Ð² optional, Ñ‚.Ðº. Ñƒ QuantizedBlock Ð½ÐµÑ‚ Ð´ÐµÑ„Ð¾Ð»Ñ‚Ð½Ð¾Ð³Ð¾ ÐºÐ¾Ð½ÑÑ‚Ñ€ÑƒÐºÑ‚Ð¾Ñ€Ð°
    vector<optional<QuantizedBlock>> tmpBlocks(totalBlocks);

    auto processComponent = [&](int component,
                                int blocksX, int blocksY,
                                int step,
                                int offset) {
        int total = blocksX * blocksY;
        if (total == 0) return;

        int threads = min(numThreads, total);
        if (threads <= 0) threads = 1;

        vector<thread> workers;
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

    // ÐŸÐ¾Ñ€ÑÐ´Ð¾Ðº Ñ‚Ð¾Ñ‚ Ð¶Ðµ, Ñ‡Ñ‚Ð¾ Ð¸ Ð² SequentialBlockProcessor:
    // ÑÐ½Ð°Ñ‡Ð°Ð»Ð° Ð²ÑÐµ Y, Ð·Ð°Ñ‚ÐµÐ¼ Ð²ÑÐµ Cb, Ð·Ð°Ñ‚ÐµÐ¼ Ð²ÑÐµ Cr.
    processComponent(0, nxY, nyY, 8,  0);
    processComponent(1, nxC, nyC, 16, yBlocksCount);
    processComponent(2, nxC, nyC, 16, yBlocksCount + cbBlocksCount);

    vector<QuantizedBlock> blocks;
    blocks.reserve(totalBlocks);
    for (auto &opt : tmpBlocks) {
        // Ð’ÑÐµ ÑÐ»ÐµÐ¼ÐµÐ½Ñ‚Ñ‹ Ð´Ð¾Ð»Ð¶Ð½Ñ‹ Ð±Ñ‹Ñ‚ÑŒ Ð·Ð°Ð¿Ð¾Ð»Ð½ÐµÐ½Ñ‹
        blocks.push_back(move(opt.value()));
    }

    return blocks;
}