EMXX ?= em++

CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Iinclude
EMFLAGS ?= -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=ChessEngineModule -s ALLOW_MEMORY_GROWTH=1 \
	-s EXPORTED_FUNCTIONS='["_get_best_move","_malloc","_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]'

SRC := \
	src/api.cpp \
	src/eval.cpp \
	src/fen.cpp \
	src/movegen.cpp \
	src/search.cpp

BUILD_DIR := ../build
TARGET_JS := $(BUILD_DIR)/engine.js
TARGET_WASM := $(BUILD_DIR)/engine.wasm

.PHONY: all clean

all: $(TARGET_JS)

$(TARGET_JS): $(SRC) | $(BUILD_DIR)
	$(EMXX) $(CXXFLAGS) $(SRC) -o $(TARGET_JS) $(EMFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET_JS) $(TARGET_WASM)
