#ifndef JPEG_DECODER_H
#define JPEG_DECODER_H

#include "image_types.h"
#include "interfaces.h"
#include "quantized_block.h"
#include "sequential_processors.h"
#include <vector>
#include <memory>
#include <unordered_map>

// Интерфейс обратного DCT
class IDctInverseTransform {
public:
    virtual ~IDctInverseTransform() = default;
    virtual std::vector<std::vector<double>> inverseDct(const std::vector<std::vector<int>>& quantizedBlock) = 0;
};

// Последовательная реализация обратного DCT
class SequentialDctInverseTransform : public IDctInverseTransform {
public:
    std::vector<std::vector<double>> inverseDct(const std::vector<std::vector<int>>& quantizedBlock) override;
};

// Huffman декодер
class HuffmanDecoder {
private:
    // Построение обратной таблицы: (code, length) -> symbol
    struct DecodingEntry {
        int symbol;
        int codeLength;
    };
    
    std::unordered_map<int, DecodingEntry> buildDecodingTable(
        const std::unordered_map<int, std::pair<int, int>>& encodingTable);

public:
    // Декодирует поток обратно в коэффициенты
    std::vector<std::vector<std::vector<int>>> decode(
        const std::vector<unsigned char>& compressedData,
        const std::unordered_map<int, std::pair<int, int>>& huffmanTable,
        int expectedBlocks);
};

// Основной JPEG декодер
class JpegDecoder {
private:
    std::unique_ptr<IDctInverseTransform> idct;
    std::vector<std::vector<int>> quantizationTable;
    
    // Деквантизация блока
    std::vector<std::vector<int>> dequantize(const std::vector<std::vector<int>>& quantized);
    
    // Обратный zigzag scan
    static std::vector<std::vector<int>> inverseZigzag(const std::vector<int>& zigzagData);
    
    // Конвертация YCbCr -> RGB
    RgbImage ycbcrToRgb(const YCbCrImage& ycbcr);
    
    // Помещение блока обратно в изображение
    void placeBlock(YCbCrImage& image, const std::vector<std::vector<double>>& block,
                   int blockX, int blockY, int component);

public:
    explicit JpegDecoder(std::vector<std::vector<int>> quantTable);
    
    // Декодирование из закодированных данных
    RgbImage decode(const JpegEncodedData& encodedData);
    
    // Декодирование напрямую из квантованных блоков (для тестирования)
    RgbImage decodeFromBlocks(const std::vector<QuantizedBlock>& blocks, int width, int height);
};

// Расширенная структура для хранения промежуточных данных (для тестирования)
struct JpegTestData {
    JpegEncodedData encodedData;
    std::vector<QuantizedBlock> quantizedBlocks;  // Сохраняем для прямого декодирования
};

// Фабричные функции
std::unique_ptr<JpegEncoder> createJpegEncoder(int quality = 75);
std::unique_ptr<JpegDecoder> createJpegDecoder(const std::vector<std::vector<int>>& quantTable);

#endif
