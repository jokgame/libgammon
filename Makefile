.PHONY: default
default:
	@mkdir -p build/default
	cd build/default && cmake ../.. -DENABLE_TEST=ON && make

all: default ios android

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
	./build/default/bin/gammon_test ./data/tdgammon.onnx

windows: win32 win64

win32:
	@mkdir -p windows/win32
	cd windows/win32 \
	&& cmake -G "Visual Studio 16 2019" -A Win32 -S ../..

win64:
	@mkdir -p windows/win64
	cd windows/win64 \
	&& cmake -G "Visual Studio 16 2019" -A x64 -S ../..

clean:
	rm -rf build
