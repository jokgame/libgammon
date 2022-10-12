.PHONY: build
build:
	mkdir -p build
	cd build && cmake .. -DENABLE_TEST=ON && make

.PHONY: ios
ios:
	mkdir -p build/ios
	cd build/ios \
		&& cmake ../.. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../../ios.toolchain.cmake -DPLATFORM=OS64 -DENABLE_BITCODE=FALSE \
		&& cmake --build . --config Release

.PHONY: test
test: build
	./build/bin/gammon_test ./data/tdgammon.onnx

