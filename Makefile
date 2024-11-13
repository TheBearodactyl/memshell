CXX := clang++
CXXFLAGS := -O3 -flto -std=c++17 -target x86_64-pc-windows-msvc -fuse-ld=lld

SRC := src/main.cpp
BUILD_DIR := build
OBJ := $(BUILD_DIR)/main.o
EXE := $(BUILD_DIR)/main.exe

all: $(EXE)

$(EXE): $(OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ): $(SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

clean:
	del /f /q $(BUILD_DIR)\*

.PHONY: all clean
