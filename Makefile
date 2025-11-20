CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
SRCDIR = src
OBJDIR = obj
BINDIR = bin

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/jpeg_compressor

# Создание директорий еѝли их нет
$(shell mkdir -p $(OBJDIR) $(BINDIR))

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Завиѝимоѝти
$(OBJDIR)/main.o: include/sequential_processors.h
$(OBJDIR)/sequential_processors.o: include/sequential_processors.h include/interfaces.h include/dct_math.h include/color_math.h include/huffman_math.h include/bit_writer.h
$(OBJDIR)/pipeline_processors.o: include/pipeline_processors.h include/interfaces.h include/dct_math.h include/color_math.h include/huffman_math.h include/bit_writer.h
$(OBJDIR)/bit_writer.o: include/bit_writer.h
$(OBJDIR)/color_math.o: include/color_math.h
$(OBJDIR)/dct_math.o: include/dct_math.h
$(OBJDIR)/huffman_math.o: include/huffman_math.h
$(OBJDIR)/image_types.o: include/image_types.h
$(OBJDIR)/quantized_block.o: include/quantized_block.h

.PHONY: clean run

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)