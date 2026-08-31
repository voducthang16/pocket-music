BUILD_DIR := build
APP := $(BUILD_DIR)/classic-ipod
MUSIC_DIR ?= Music
CMAKE ?= cmake
JOBS ?= 4
FORMATTER ?= /Library/Developer/CommandLineTools/usr/bin/clang-format
SOURCES := $(shell find src -type f \( -name '*.cpp' -o -name '*.hpp' \))

.PHONY: help setup build run test format

help:
	@echo "Classic iPod commands:"
	@echo "  make run     Build changes and open the app"
	@echo "  make build   Build without opening the app"
	@echo "  make test    Build and run automated tests"
	@echo "  make format  Format all C++ files"
	@echo ""
	@echo "Custom music folder: make run MUSIC_DIR='/path/to/music'"

setup:
	$(CMAKE) -S . -B $(BUILD_DIR) -DBUILD_TESTING=ON

build: setup
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

run: build
	./$(APP) --music "$(MUSIC_DIR)"

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

format:
	$(FORMATTER) -i $(SOURCES)
