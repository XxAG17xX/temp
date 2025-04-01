cmake_minimum_required(VERSION 3.20)

project(computer_networks)

# Set CPP Standards
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(MSVC)
    set(WARNING_FLAGS /W4)
  else()
    set(WARNING_FLAGS -Wall -Wextra -Wpedantic)
  endif()
endif()

# Macro to get Windows NT version in hex (Source: https://stackoverflow.com/questions/9742003/platform-detection-in-cmake)
macro(get_win_hex outvar)
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" ${outvar} ${CMAKE_SYSTEM_VERSION})
  math(EXPR ${outvar} "(${CMAKE_MATCH_1} << 8) + ${CMAKE_MATCH_2}" OUTPUT_FORMAT HEXADECIMAL)
endmacro()

# If Windows, set _WIN32_WINNT
if(WIN32)
  get_win_hex(winver)
  add_compile_definitions(_WIN32_WINNT=${winver})
endif()

# compile_commands.json enables LSP to actually see where things are
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add ASIO Standalone
include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-32-0
)

FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR})


# Add Source directory
add_subdirectory(src)

# Add Test directory
enable_testing()
add_subdirectory(test)

# Set CPP Standard
set(TARGETS earth error_correction error_correction_test health terrain_gen rover utils)

foreach(target IN LISTS TARGETS)
  set_target_properties(${target} PROPERTIES 
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON 
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON)
  target_compile_options(${target} PRIVATE ${WARNING_FLAGS})
endforeach()
