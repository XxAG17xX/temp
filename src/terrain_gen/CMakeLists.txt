# src/terrain_gen

add_library(terrain_gen
    terrain_gen.h terrain_gen.cpp)

add_executable(terrain_test main.cpp)

target_link_libraries(terrain_test PRIVATE terrain_gen)