# src/error_correction/

add_library(error_correction STATIC
    error_correction.cpp
)

target_include_directories(error_correction 
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src/error_correction
)

target_link_libraries(error_correction PUBLIC utils)