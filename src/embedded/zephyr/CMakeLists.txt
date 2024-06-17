cmake_minimum_required(VERSION 3.16)

# Zephyr library dependency
# This must be before any other CMake includes/projects (it sets up the Zephyr toolchain)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Sessionless library dependency
include(FetchContent)
FetchContent_Declare(
  sessionless

  GIT_REPOSITORY https://github.com/planet-nine-app/sessionless.git
  GIT_TAG        cpp-client-lib-0.2
  SOURCE_SUBDIR  src/cpp/client/lib

  # To use a local folder comment out hte lines above and use this instead
  # SOURCE_DIR     ${CMAKE_CURRENT_LIST_DIR}/../sessionless/src/cpp/client/lib
)
set(CMAKE_C_FLAGS -mlongcalls)
FetchContent_MakeAvailable(sessionless)

# MAGIC executable
set(NAME app)

project(${NAME})

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
target_sources(${NAME} PRIVATE ${SRC_FILES})
target_link_libraries(${NAME} PRIVATE sessionless)
target_compile_options(${NAME} PRIVATE -Wall)
