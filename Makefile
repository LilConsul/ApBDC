# Makefile for code formatting
# Usage: make -f Makefile.format

# Project directories
SRC_DIR := src
INCLUDE_DIR := include

# Find all C/C++ source and header files
SRC_FILES := $(shell find $(SRC_DIR) -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \))
HEADER_FILES := $(shell find $(INCLUDE_DIR) -type f \( -name "*.h" -o -name "*.hpp" \) 2>/dev/null)
ALL_FILES := $(SRC_FILES) $(HEADER_FILES)

.PHONY: format

format:
	@echo "Formatting source files..."
	@clang-format -i $(ALL_FILES)
	@echo "Done! Formatted $(words $(ALL_FILES)) files."