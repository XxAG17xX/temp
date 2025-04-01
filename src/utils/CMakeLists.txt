# src/utils/

add_library(utils STATIC
    utils.cpp
)

target_include_directories(utils PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
    ${asio_SOURCE_DIR}/asio/include)
