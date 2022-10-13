.PHONY: build
build:
	@mkdir -p build
	cd build && cmake .. -DENABLE_TEST=ON && make

mobile: ios android

.PHONY: ios
ios:
	@mkdir -p build/ios
	cd build/ios \
		&& cmake ../.. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../../ios.toolchain.cmake -DPLATFORM=OS64 -DENABLE_BITCODE=FALSE \
		&& cmake --build . --config Release

.PHONY: android
android: check_android_toolchain_cmake
	@mkdir -p build/android
	cd build/android \
		&& cmake ../.. -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

.PHONY: check_android_toolchain_cmake
check_android_toolchain_cmake:
ifndef ANDROID_TOOLCHAIN_CMAKE
	$(error ANDROID_TOOLCHAIN_CMAKE is undefined)
endif

.PHONY: test
test: build
	./build/bin/gammon_test ./data/tdgammon.onnx

