CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -fopenmp -Iinclude
OBJDIR = obj
BINDIR = bin
SRCDIR = src
INCDIR = include

# Все .cpp файлы в папке src
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jpeg_compressor

# Создание директорий
$(shell mkdir -p $(OBJDIR) $(BINDIR))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET) -lpthread

# Компиляция .cpp -> .o
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Зависимости заголовочных файлов
$(OBJDIR)/main.o: $(INCDIR)/sequential_processors.h $(INCDIR)/image_metrics.h $(INCDIR)/analysis_scripts.h
$(OBJDIR)/sequential_processors.o: $(INCDIR)/sequential_processors.h $(INCDIR)/interfaces.h $(INCDIR)/dct_math.h $(INCDIR)/color_math.h $(INCDIR)/huffman_math.h $(INCDIR)/bit_writer.h $(INCDIR)/quantized_block.h $(INCDIR)/image_types.h
$(OBJDIR)/jpeg_decoder.o: $(INCDIR)/jpeg_decoder.h $(INCDIR)/sequential_processors.h $(INCDIR)/dct_math.h $(INCDIR)/color_math.h $(INCDIR)/image_types.h
$(OBJDIR)/image_metrics.o: $(INCDIR)/image_metrics.h $(INCDIR)/image_types.h
$(OBJDIR)/analysis_scripts.o: $(INCDIR)/analysis_scripts.h $(INCDIR)/jpeg_decoder.h $(INCDIR)/image_metrics.h $(INCDIR)/sequential_processors.h
$(OBJDIR)/bit_writer.o: $(INCDIR)/bit_writer.h
$(OBJDIR)/color_math.o: $(INCDIR)/color_math.h
$(OBJDIR)/dct_math.o: $(INCDIR)/dct_math.h
$(OBJDIR)/huffman_math.o: $(INCDIR)/huffman_math.h
$(OBJDIR)/image_types.o: $(INCDIR)/image_types.h
$(OBJDIR)/quantized_block.o: $(INCDIR)/quantized_block.h

.PHONY: clean run debug test all

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	@echo "=== Running JPEG Tests ==="
	./$(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)

# Дополнительная цель для анализа
analysis: $(TARGET)
	@echo "=== Running Quality Analysis ==="
	./$(TARGET) --analysis
