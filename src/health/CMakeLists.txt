add_library(health STATIC
    health.cpp
)

target_include_directories(health PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(health PRIVATE utils)

# Build the test executable
add_executable(health_test main.cpp)

target_include_directories(health_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${asio_SOURCE_DIR}/asio/include  # Same as utils
)

target_link_libraries(health_test PRIVATE health)
