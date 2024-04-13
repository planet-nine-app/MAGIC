cmake_minimum_required(VERSION 3.8.2)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(magic)

set(TARGET_NAME app)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
target_sources(${TARGET_NAME} PRIVATE ${SRC_FILES})

target_compile_options(${TARGET_NAME} PRIVATE 
    -Wall
)
