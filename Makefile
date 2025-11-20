CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -fopenmp
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jpeg_compressor

# Создание директорий если их нет
$(shell mkdir -p $(OBJDIR) $(BINDIR))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

$(OBJDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Зависимости
$(OBJDIR)/main.o: sequential_processors.h interfaces.h image_types.h
$(OBJDIR)/sequential_processors.o: sequential_processors.h interfaces.h dct_math.h color_math.h huffman_math.h bit_writer.h quantized_block.h image_types.h
$(OBJDIR)/OpenMPBlockProcessor.o: OpenMPBlockProcessor.h interfaces.h OpenMPDctTransform.h OpenMPQuantizer.h image_types.h quantized_block.h
$(OBJDIR)/OpenMPDctTransform.o: OpenMPDctTransform.h interfaces.h dct_math.h
$(OBJDIR)/OpenMPQuantizer.o: OpenMPQuantizer.h interfaces.h
$(OBJDIR)/bit_writer.o: bit_writer.h
$(OBJDIR)/color_math.o: color_math.h
$(OBJDIR)/dct_math.o: dct_math.h
$(OBJDIR)/huffman_math.o: huffman_math.h
$(OBJDIR)/image_types.o: image_types.h
$(OBJDIR)/quantized_block.o: quantized_block.h

.PHONY: clean run debug all

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)
