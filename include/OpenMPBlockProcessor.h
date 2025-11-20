#ifndef OPENMP_BLOCK_PROCESSOR_H
#define OPENMP_BLOCK_PROCESSOR_H

#include "interfaces.h"
#include "OpenMPDctTransform.h"
#include "OpenMPQuantizer.h"
#include <vector>

class OpenMPBlockProcessor : public IBlockProcessor {
private:
    std::unique_ptr<OpenMPDctTransform> dct;
    std::unique_ptr<OpenMPQuantizer> quantizer;

    std::vector<std::vector<double>> extractBlock(const YCbCrImage& image, int x, int y, int component);

public:
    OpenMPBlockProcessor(std::unique_ptr<OpenMPDctTransform> dctTransform, 
                        std::unique_ptr<OpenMPQuantizer> quantizer);
    
    std::vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
};

#endif