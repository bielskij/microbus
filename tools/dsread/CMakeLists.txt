cmake_minimum_required(VERSION 3.28)

project(dsread)

find_package(spdlog REQUIRED)

include(${CMAKE_CURRENT_LIST_DIR}/../../common/CMakeLists.txt)

add_executable(dsread 
    src/main.cpp
)

target_include_directories(dsread
    PRIVATE
        ../../kernel/include
)

target_link_libraries(dsread
	PRIVATE
        protocol
    	spdlog::spdlog_header_only
)