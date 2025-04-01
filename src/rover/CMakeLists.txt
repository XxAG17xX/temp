# src/rover/

add_executable(
    rover
    main.cpp
    rover.cpp
    rover.h
)

target_include_directories(rover PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${asio_SOURCE_DIR}/asio/include)

target_link_libraries(rover PRIVATE error_correction health terrain_gen utils)
