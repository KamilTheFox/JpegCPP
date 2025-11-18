#include "bit_writer.h"
#include <stdexcept>
#include <algorithm>

BitWriter::BitWriter() : currentByte(0), bitPosition(0) {}

void BitWriter::writeBits(int value, int bitCount) {
    if (bitCount <= 0 || bitCount > 32) {
        throw std::invalid_argument("Bit count must be 1-32");
    }
    
    for (int i = bitCount - 1; i >= 0; i--) {
        int bit = (value >> i) & 1;
        currentByte = (currentByte << 1) | bit;
        bitPosition++;
        
        if (bitPosition == 8) {
            stream.push_back(currentByte);
            currentByte = 0;
            bitPosition = 0;
        }
    }
}

std::vector<unsigned char> BitWriter::toArray() {
    // Дописываем последний байт если не заполнен
    if (bitPosition > 0) {
        currentByte <<= (8 - bitPosition);
        stream.push_back(currentByte);
    }
    
    // Byte stuffing: после каждого 0xFF вставляем 0x00
    std::vector<unsigned char> stuffedStream;
    
    for (unsigned char b : stream) {
        stuffedStream.push_back(b);
        if (b == 0xFF) {
            stuffedStream.push_back(0x00); // stuff byte
        }
    }
    
    return stuffedStream;
}