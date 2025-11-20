CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -fopenmp -Iinclude
OBJDIR = obj
BINDIR = bin
SRCDIR = src
INCDIR = include

# Исходники
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jpeg_compressor

# Создание директорий
$(shell mkdir -p $(OBJDIR) $(BINDIR))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET) -lpthread

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Зависимости
$(OBJDIR)/main.o: $(INCDIR)/sequential_processors.h $(INCDIR)/pipeline_processor.h $(INCDIR)/interfaces.h $(INCDIR)/image_types.h $(INCDIR)/OpenMPBlockProcessor.h $(INCDIR)/OpenMPDctTransform.h $(INCDIR)/OpenMPQuantizer.h $(INCDIR)/multy_thread.h
$(OBJDIR)/sequential_processors.o: $(INCDIR)/sequential_processors.h $(INCDIR)/interfaces.h $(INCDIR)/dct_math.h $(INCDIR)/color_math.h $(INCDIR)/huffman_math.h $(INCDIR)/bit_writer.h $(INCDIR)/quantized_block.h $(INCDIR)/image_types.h
$(OBJDIR)/pipeline_processor.o: $(INCDIR)/pipeline_processor.h $(INCDIR)/interfaces.h
$(OBJDIR)/OpenMPBlockProcessor.o: $(INCDIR)/OpenMPBlockProcessor.h $(INCDIR)/interfaces.h $(INCDIR)/OpenMPDctTransform.h $(INCDIR)/OpenMPQuantizer.h $(INCDIR)/image_types.h $(INCDIR)/quantized_block.h
$(OBJDIR)/OpenMPDctTransform.o: $(INCDIR)/OpenMPDctTransform.h $(INCDIR)/interfaces.h $(INCDIR)/dct_math.h
$(OBJDIR)/OpenMPQuantizer.o: $(INCDIR)/OpenMPQuantizer.h $(INCDIR)/interfaces.h
$(OBJDIR)/multy_thread.o: $(INCDIR)/multy_thread.h $(INCDIR)/interfaces.h $(INCDIR)/color_math.h $(INCDIR)/image_types.h $(INCDIR)/quantized_block.h
$(OBJDIR)/bit_writer.o: $(INCDIR)/bit_writer.h
$(OBJDIR)/color_math.o: $(INCDIR)/color_math.h
$(OBJDIR)/dct_math.o: $(INCDIR)/dct_math.h
$(OBJDIR)/huffman_math.o: $(INCDIR)/huffman_math.h
$(OBJDIR)/image_types.o: $(INCDIR)/image_types.h
$(OBJDIR)/quantized_block.o: $(INCDIR)/quantized_block.h

.PHONY: clean run debug all

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)