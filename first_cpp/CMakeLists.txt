cmake_minimum_required(VERSION 3.10)

if (WIN32)
	set(CMAKE_GENERATOR_PLATFORM x64) # Force 64 bits
endif()

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

project(simple_calc)

add_subdirectory(src)
