#pragma once
#include <array>

/// @brief Determines the side-length of each terrain chunk
constexpr int CHUNK_SIZE = 5;

// The code breaks if chunk size isn't an odd number
static_assert(CHUNK_SIZE % 2 == 1);

/// @brief Generates terrain using a seeded, uniformly random generation technique (Mersenne-Twister)
class TerrainGenerator
{
private:
    double rockProbability;
    int seed;

    // Internal terrain generation cache variables
    bool terrain_initialized = false;
    int prev_chunk_x = 0, prev_chunk_y = 0;
    std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE>
        chunk_top_left, chunk_top_middle, chunk_top_right,
        chunk_middle_left, chunk_middle_middle, chunk_middle_right,
        chunk_bottom_left, chunk_bottom_middle, chunk_bottom_right = {};

    // Gets chunk of terrain by chunk coordinates
    std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> getChunk(int chunk_x, int chunk_y);

public:
    /// @brief Constructor for Terrain Generator
    /// @param rockProbability Percent chance of a rock appearing on the terrain, clamped between 0 and 1
    /// @param seed Seed for terrain generation
    TerrainGenerator(double rockProbability, int seed);

    /// @brief getTerrain generates a n by n grid of "terrain" which either has
    /// a rock (true) or flat ground (false). Subsequent calls to getTerrain
    /// can take advantage of internal cache variables to avoid constant
    /// regeneration of terrain.
    /// @param x horizontal coordinate (positive is right)
    /// @param y vertical coordinate (positive is down)
    /// @return a square grid of a given random chunk
    std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> getTerrain(int x, int y);

    /// @brief prints a CHUNK_SIZE long, square grid of the current terrain. Calls getTerrain()
    /// @param x horizontal coordinate (positive is right)
    /// @param y vertical coordinate (positive is down)
    void printTerrain(int x, int y);

    TerrainGenerator(const TerrainGenerator &other) = delete;
};