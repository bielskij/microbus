cmake_minimum_required(VERSION 3.28)

project(microbusctl)

include(${CMAKE_CURRENT_LIST_DIR}/../../common/CMakeLists.txt)

find_package(spdlog REQUIRED)

add_executable(microbusctl
    src/main.cpp
)

target_include_directories(microbusctl
    PRIVATE
        ../../kernel/include
)

target_link_libraries(microbusctl
	PRIVATE
        protocol
        spdlog::spdlog_header_only
)