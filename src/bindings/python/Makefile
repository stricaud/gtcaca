# Convenience wrapper around the CMake build for the GTCaca Python bindings.
#
#   make            configure (first time) + build, then drop the compiled
#                   extension into the gtcaca/ package so `python3 example.py`
#                   works straight from this directory
#   make run        build, then run the demo
#   make install    pip install . into the current environment
#   make clean      remove the CMake build tree and the in-tree extension
#   make distclean  clean + remove pip/scikit-build artefacts

PYTHON      ?= python3
BUILD_DIR   ?= build
PKG_DIR     := gtcaca
PYBIND11_DIR := $(shell $(PYTHON) -m pybind11 --cmakedir 2>/dev/null)

# The built module name (e.g. _gtcaca.cpython-314-darwin.so).
EXT := $(wildcard $(BUILD_DIR)/_gtcaca*.so) $(wildcard $(BUILD_DIR)/_gtcaca*.pyd)

.PHONY: all build configure run install clean distclean

all: build

# Configure once; re-runs are cheap and pick up CMakeLists.txt changes.
configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt:
	@if [ -z "$(PYBIND11_DIR)" ]; then \
	  echo "error: pybind11 not found. Install it with: $(PYTHON) -m pip install pybind11"; \
	  exit 1; \
	fi
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -Dpybind11_DIR="$(PYBIND11_DIR)"

build: configure
	cmake --build $(BUILD_DIR) -j
	@cp $(BUILD_DIR)/_gtcaca*.so $(PKG_DIR)/ 2>/dev/null || cp $(BUILD_DIR)/_gtcaca*.pyd $(PKG_DIR)/ 2>/dev/null || true
	@echo "built extension copied into $(PKG_DIR)/"

run: build
	$(PYTHON) example.py

install:
	$(PYTHON) -m pip install .

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PKG_DIR)/_gtcaca*.so $(PKG_DIR)/_gtcaca*.pyd
	rm -rf $(PKG_DIR)/__pycache__

distclean: clean
	rm -rf dist *.egg-info .scikit-build-core
