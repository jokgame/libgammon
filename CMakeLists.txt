cmake_minimum_required(VERSION 3.0)

project(libgammon)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "-std=c++17 -Wno-deprecated-declarations ${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "-std=c17 -Wno-deprecated-declarations ${CMAKE_C_FLAGS}")

option(ENABLE_TEST "Enable test." OFF)

# build library
aux_source_directory(./backgammon src_libgammon)
add_library(gammon ${src_libgammon})

# build gammon_test

if (ENABLE_TEST)
	aux_source_directory(./test src_test)
	add_executable(gammon_test ${src_test})
	target_link_libraries(gammon_test gammon onnxruntime)
endif()
