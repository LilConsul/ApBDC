# Makefile for code formatting
# Usage: make format
# Only processes files in src/ and include/ directories
# System files are NEVER touched

.PHONY: format

format:
	@echo "Formatting code (K&R style) and sorting includes..."
	@find src include -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) 2>/dev/null | while read file; do \
		echo "  $$file"; \
		clang-format -i "$$file"; \
	done
	@echo "Done! Code formatted."