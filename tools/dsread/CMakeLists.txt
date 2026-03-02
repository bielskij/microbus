cmake_minimum_required(VERSION 3.28)

project(dsread)

find_package(spdlog REQUIRED)

add_executable(dsread 
    src/main.cpp
)

target_include_directories(dsread
    PRIVATE
        ../../kernel/include
)

target_link_libraries(dsread
	PRIVATE
    	spdlog::spdlog_header_only
)