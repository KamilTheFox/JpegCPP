#ifndef ANALYSIS_SCRIPTS_H
#define ANALYSIS_SCRIPTS_H

#include "image_types.h"
#include "sequential_processors.h"
#include "image_metrics.h"
#include <vector>

class AnalysisScripts {
public:
    static void testFullCycle(int width, int height, int quality);
    static void qualitySweepTest(int width, int height);
    static void compareDifferentSizes();
    static void generateQualityReport();
};

#endif