# === Build main earth executable ===
add_executable(
    earth
    main.cpp
    earth.cpp
    earth.h
)

target_include_directories(earth PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${asio_SOURCE_DIR}/asio/include)

target_link_libraries(earth PRIVATE error_correction utils)
