#ifndef BIT_READER_H
#define BIT_READER_H

#include <vector>
#include <stdexcept>

class BitReader {
private:
    const std::vector<unsigned char>& stream;
    size_t bytePosition;
    int bitPosition; // 0-7, позиция в текущем байте (от старшего к младшему)
    
public:
    explicit BitReader(const std::vector<unsigned char>& data) 
        : stream(data), bytePosition(0), bitPosition(0) {}
    
    // Читает указанное количество бит и возвращает значение
    int readBits(int bitCount) {
        if (bitCount <= 0 || bitCount > 32) {
            throw std::invalid_argument("Bit count must be 1-32");
        }
        
        int result = 0;
        
        for (int i = 0; i < bitCount; i++) {
            if (bytePosition >= stream.size()) {
                throw std::runtime_error("End of stream reached");
            }
            
            // Читаем бит от старшего к младшему
            int bit = (stream[bytePosition] >> (7 - bitPosition)) & 1;
            result = (result << 1) | bit;
            
            bitPosition++;
            if (bitPosition == 8) {
                bitPosition = 0;
                bytePosition++;
                
                // Пропускаем stuff byte (0x00 после 0xFF)
                if (bytePosition < stream.size() && 
                    bytePosition > 0 && 
                    stream[bytePosition - 1] == 0xFF && 
                    stream[bytePosition] == 0x00) {
                    bytePosition++;
                }
            }
        }
        
        return result;
    }
    
    // Читает один бит
    int readBit() {
        return readBits(1);
    }
    
    // Проверяет, достигнут ли конец потока
    bool isEnd() const {
        return bytePosition >= stream.size();
    }
    
    // Возвращает текущую позицию в битах
    size_t getPosition() const {
        return bytePosition * 8 + bitPosition;
    }
    
    // Сбрасывает позицию в начало
    void reset() {
        bytePosition = 0;
        bitPosition = 0;
    }
};

#endif
