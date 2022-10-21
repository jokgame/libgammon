cmake_minimum_required(VERSION 3.0)

project(libgammon)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "-std=c++17 ${CMAKE_CXX_FLAGS}")
set(CMAKE_C_FLAGS "-std=c17 ${CMAKE_C_FLAGS}")

option(ENABLE_TEST "Enable test." OFF)

# build library
aux_source_directory(./src/backgammon src_libgammon)
add_library(gammon_static STATIC ${src_libgammon})
set_target_properties(gammon_static PROPERTIES OUTPUT_NAME gammon)
if (${CMAKE_SYSTEM_NAME} MATCHES "Android" OR ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_library(gammon_shared SHARED ${src_libgammon})
	set_target_properties(gammon_shared PROPERTIES OUTPUT_NAME gammon)
endif()

# build gammon_test
if (ENABLE_TEST)
	aux_source_directory(./src/test src_test)
	add_executable(gammon_test ${src_test})
	target_link_libraries(gammon_test gammon onnxruntime)
endif()
