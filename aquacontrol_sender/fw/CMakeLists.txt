# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ../components/lora)

# Get the current Git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Optionally, get the current Git tag (if any)
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Define the version information as macros
set(APP_VERSION "v${GIT_TAG} - ${GIT_COMMIT_HASH}")

# Pass the version to your application source code
add_definitions(-DAPP_VERSION="${APP_VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lora_send VERSION 1.0.0)
