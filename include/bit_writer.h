#ifndef BIT_WRITER_H
#define BIT_WRITER_H

#include <vector>

class BitWriter {
private:
    std::vector<unsigned char> stream;
    unsigned char currentByte;
    int bitPosition; // 0-7, позиция в текущем байте

public:
    BitWriter();
    
    void writeBits(int value, int bitCount);
    std::vector<unsigned char> toArray();
};

#endif