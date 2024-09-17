ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

CSTD = -std=c2x
CFLAGS = -Og -g -gdwarf -Wall -Wextra -Werror -Wfatal-errors -fstack-protector-strong
CFLAGS += -Wno-error=unused-parameter
CFLAGS += -Wno-error=unused-variable
CFLAGS += -Wno-error=unused-but-set-variable
CFLAGS += -Wno-error=unused-result
CFLAGS += -DDEBUG
CFLAGS += -I$(ROOT_DIR) -I$(ROOT_DIR)/it/vqdll
CXXSTD = -std=c++23
CXXFLAGS =


OBJ = \
	soe/pvrtool/C.o \
	soe/pvrtool/Colour.o \
	soe/pvrtool/CommandLineProcessor.o \
	soe/pvrtool/Image.o \
	soe/pvrtool/PIC.o \
	soe/pvrtool/Picture.o \
	soe/pvrtool/PVR.o \
	soe/pvrtool/PVRTool.o \
	soe/pvrtool/Resample.o \
	soe/pvrtool/Twiddle.o \
	soe/pvrtool/Util.o \
	soe/pvrtool/VQCompressor.o \
	soe/pvrtool/VQF.o \
	soe/pvrtool/VQImage.o \
	it/vqdll/vqcalc.o \
	it/vqdll/vqdll.o \
	findfirst.o \
	stricmp.o \
	strupr.o \
	stb_image.o

all: pvrtool

pvrtool: $(OBJ)
	$(CXX) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXSTD) $(CFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CSTD) $(CFLAGS) -c $< -o $@

clean:
	find -type f -iname '*.o' -exec rm {} \;
	rm -f pvrtool

.SUFFIXES:
.INTERMEDIATE:
.SECONDARY:
.PHONY: all clean
