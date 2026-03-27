cmake_minimum_required(VERSION 3.16)
project(rndr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(assimp CONFIG QUIET)
if(NOT assimp_FOUND)
	find_package(assimp QUIET)
endif()

set(RNDR_BUNDLED_ASSIMP OFF)
if(NOT assimp_FOUND)
	message(STATUS "assimp not found locally; fetching a bundled copy")
	include(FetchContent)
	set(RNDR_BUNDLED_ASSIMP ON)

	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
	set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
	set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
	set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(
					assimp
					GIT_REPOSITORY https://github.com/assimp/assimp.git
					GIT_TAG v5.4.3
					GIT_SHALLOW TRUE
	)

	FetchContent_MakeAvailable(assimp)
endif()

if(RNDR_BUNDLED_ASSIMP AND TARGET assimp)
	find_path(
		RNDR_MINIZIP_INCLUDE_DIR
		NAMES unzip.h
		PATH_SUFFIXES minizip
	)

	if(RNDR_MINIZIP_INCLUDE_DIR)
		target_include_directories(assimp SYSTEM PRIVATE "${RNDR_MINIZIP_INCLUDE_DIR}")
	endif()
endif()

if(TARGET assimp::assimp)
	set(RNDR_ASSIMP_TARGET assimp::assimp)
elseif(TARGET assimp)
	set(RNDR_ASSIMP_TARGET assimp)
else()
	message(FATAL_ERROR "assimp was found or fetched, but no usable CMake target was exposed")
endif()

add_executable(
				rndr
				src/Main.cpp
				src/common.cpp
				src/assets.cpp
				src/rendering.cpp
				src/stb_image.cpp
)

target_include_directories(
				rndr
				PRIVATE
				${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
)

target_link_libraries(
				rndr
				PRIVATE
				${RNDR_ASSIMP_TARGET}
)
