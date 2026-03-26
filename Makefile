# SwitchPalace - Convenience Makefile wrapping CMake
#
# APP_TITLE  := SwitchPalace
# APP_AUTHOR := SwitchPalace Contributors
# APP_VERSION := 0.1.0
# BOREALIS_PATH := lib/borealis
#
# This Makefile is a convenience wrapper around CMake.
# The actual build system is CMakeLists.txt.
#
# Usage:
#   make              - Build NRO for Switch (requires devkitPro)
#   make desktop      - Build for desktop (development/testing)
#   make clean        - Clean build directory
#   make docker       - Build via Docker for reproducible builds

APP_TITLE    := SwitchPalace
APP_AUTHOR   := SwitchPalace Contributors
APP_VERSION  := 0.1.0
BOREALIS_PATH := lib/borealis

BUILD_DIR    := build
NPROC        := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all switch desktop clean docker nro

all: switch

switch:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. \
		-DPLATFORM_SWITCH=ON \
		-DCMAKE_BUILD_TYPE=Release
	cd $(BUILD_DIR) && make -j$(NPROC)
	cd $(BUILD_DIR) && make $(APP_TITLE).nro

desktop:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. \
		-DPLATFORM_DESKTOP=ON \
		-DCMAKE_BUILD_TYPE=Debug
	cd $(BUILD_DIR) && make -j$(NPROC)

clean:
	rm -rf $(BUILD_DIR)

docker:
	docker build -t switchpalace-build .
	docker create --name switchpalace-extract switchpalace-build 2>/dev/null || true
	docker cp switchpalace-extract:/build/build/$(APP_TITLE).nro ./$(APP_TITLE).nro
	docker rm switchpalace-extract

nro: switch
