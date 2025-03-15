cmake_minimum_required(VERSION 3.10)

# Set project name
project(bootloader_can_bridge)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Enable floating point support for printf
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")

# Add include paths
include_directories(
    "./inc",
    "./inc/bridge",
    "./inc/common",
    "./inc/error"
)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find and add all src files
file(GLOB SOURCES "src/*.cpp")

# Add executables
add_executable(bootloader_can_bridge ${SOURCES})

# Link directories
target_link_libraries(bootloader_can_bridge Shlwapi)

# Add dll file to bin
add_custom_command(TARGET bootloader_can_bridge POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/inc/STLinkUSBDriver.dll
    $<TARGET_FILE_DIR:bootloader_can_bridge>
)
