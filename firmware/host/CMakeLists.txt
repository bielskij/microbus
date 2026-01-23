cmake_minimum_required(VERSION 3.16)

project(host-emu)

find_package(spdlog REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/../../common/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/../common/CMakeLists.txt)

set(EXECUTABLE_NAME host-emu)

set(src_path "${CMAKE_CURRENT_LIST_DIR}/src/")

add_executable(${EXECUTABLE_NAME}
	${src_path}/main.cpp
)

target_include_directories(${EXECUTABLE_NAME}
	PRIVATE
		include
)

target_link_libraries(${EXECUTABLE_NAME}
	PRIVATE
		firmware-common
		protocol

		spdlog::spdlog_header_only
)