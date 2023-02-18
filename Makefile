.PHONY: build
build:
	@mkdir -p build/default
	cd build/default && cmake ../.. -Dbackgammon_BUILD_TEST=ON && make

.PHONY: all
all: build ios android

.PHONY: ios
ios:
	@mkdir -p build/ios
	cd build/ios \
		&& cmake ../.. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../../cmake/ios.toolchain.cmake -DPLATFORM=OS64 -DENABLE_BITCODE=FALSE \
		&& cmake --build . --config Release

.PHONY: android
android: android-armeabi-v7a android-arm64-v8a android-x86 android-x86_64

android-armeabi-v7a: _check_android_toolchain_cmake
	@mkdir -p build/android/armeabi-v7a
	cd build/android/armeabi-v7a \
		&& cmake ../../.. -DANDROID_ABI=armeabi-v7a -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

android-arm64-v8a: _check_android_toolchain_cmake
	@mkdir -p build/android/arm64-v8a
	cd build/android/arm64-v8a \
		&& cmake ../../.. -DANDROID_ABI=arm64-v8a -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

android-x86: _check_android_toolchain_cmake
	@mkdir -p build/android/x86
	cd build/android/x86 \
		&& cmake ../../.. -DANDROID_ABI=x86 -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

android-x86_64: _check_android_toolchain_cmake
	@mkdir -p build/android/x86_64
	cd build/android/x86_64 \
		&& cmake ../../.. -DANDROID_ABI=x86_64 -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
		&& cmake --build . --config Release

.PHONY: _check_android_toolchain_cmake
_check_android_toolchain_cmake:
ifndef ANDROID_TOOLCHAIN_CMAKE
	$(error ANDROID_TOOLCHAIN_CMAKE is undefined)
endif

.PHONY: py
py:
	pip3 install -e .

.PHONY: test
test: build
	codesign -s - -f --entitlements codesign.entitlements ./build/default/bin/gammon_test >/dev/null
	./build/default/bin/gammon_test ./data/tdgammon.onnx

.PHONY: clean
clean:
	rm -rf build dist *.egg-info
