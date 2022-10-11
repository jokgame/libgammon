test: build
	./build/bin/gammon_test ./data/tdgammon.onnx

.PHONY: build
build:
	mkdir -p build
	cd build && cmake .. && make
