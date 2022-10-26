.PHONY: build
build:
	@mkdir -p build/default
	cd build/default && cmake ../.. -DENABLE_TEST=ON && make

.PHONY: all
all: build ios android

.PHONY: ios
ios:
	@mkdir -p build/ios
	cd build/ios \
		&& cmake ../.. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../../cmake/ios.toolchain.cmake -DPLATFORM=OS64 -DENABLE_BITCODE=FALSE \
		&& cmake --build . --config Release

.PHONY: android
android: _check_android_toolchain_cmake
	@mkdir -p build/android
	cd build/android \
		&& cmake ../.. -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

.PHONY: _check_android_toolchain_cmake
_check_android_toolchain_cmake:
ifndef ANDROID_TOOLCHAIN_CMAKE
	$(error ANDROID_TOOLCHAIN_CMAKE is undefined)
endif

.PHONY: pybind
pybind:
	python3 setup.py install

.PHONY: test
test: build
	./build/default/bin/gammon_test ./data/tdgammon.onnx

.PHONY: clean
clean:
	rm -rf build dist backgammon.egg-info
