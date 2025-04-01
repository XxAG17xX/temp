#include "terrain_gen.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

TerrainGenerator::TerrainGenerator(double rockProbability, int seed) : rockProbability(std::clamp(rockProbability, 0.0, 1.0)), seed(seed) {};

std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> TerrainGenerator::getChunk(int chunk_x, int chunk_y)
{
    std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> chunk{};

    // Use seed, x, and y coords to generate random values with Mersenne Twister
    std::seed_seq seedSeq{seed, chunk_x, chunk_y};
    std::mt19937 random_generation(seedSeq);

    // Uniform distribution is used to clamp mt19937 type to (0.0,1.0)
    std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);

    // For every square in the chunk, set whether it's a rock or not
    for (int i = 0; i < CHUNK_SIZE; ++i)
    {
        for (int j = 0; j < CHUNK_SIZE; ++j)
        {
            chunk[i][j] = uniform_dist(random_generation) < rockProbability;
        }
    }

    // Clear out area around spawn
    if (chunk_x == 0 && chunk_y == 0) {
        for(int i = 0; i < CHUNK_SIZE; ++i) {
            for(int j = 0; j < CHUNK_SIZE; ++j) {
                chunk[i][j] = false;
            }
        }
    }

    return chunk;
}

std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> TerrainGenerator::getTerrain(int x, int y)
{
    // Cache chunks from last call (to avoid regeneration on every call)

    // Determine the center chunk on this call
    int current_chunk_x = (x < 0) ? ((x + 1) / CHUNK_SIZE) - 1 : (x / CHUNK_SIZE);
    int current_chunk_y = (y < 0) ? ((y + 1) / CHUNK_SIZE) - 1 : (y / CHUNK_SIZE);

    // If this function hasn't been called yet, generate the initial grid of 9 chunks
    if (!terrain_initialized)
    {
        chunk_top_left = getChunk(current_chunk_x - 1, current_chunk_y - 1);
        chunk_top_middle = getChunk(current_chunk_x, current_chunk_y - 1);
        chunk_top_right = getChunk(current_chunk_x + 1, current_chunk_y - 1);

        chunk_middle_left = getChunk(current_chunk_x - 1, current_chunk_y);
        chunk_middle_middle = getChunk(current_chunk_x, current_chunk_y);
        chunk_middle_right = getChunk(current_chunk_x + 1, current_chunk_y);

        chunk_bottom_left = getChunk(current_chunk_x - 1, current_chunk_y + 1);
        chunk_bottom_middle = getChunk(current_chunk_x, current_chunk_y + 1);
        chunk_bottom_right = getChunk(current_chunk_x + 1, current_chunk_y + 1);

        // Stop this block from being run again
        terrain_initialized = true;

        // Prevent regeneration of chunks on first call
        prev_chunk_x = current_chunk_x;
        prev_chunk_y = current_chunk_y;
    }

    // If the center chunk isn't the current one
    if (current_chunk_x != prev_chunk_x || current_chunk_y != prev_chunk_y)
    {
        // Shift chunks
        if (current_chunk_x > prev_chunk_x)
        { // Moved right

            // middle chunks -> left chunks
            chunk_top_left = chunk_top_middle;
            chunk_middle_left = chunk_middle_middle;
            chunk_bottom_left = chunk_bottom_middle;

            // right chunks -> middle chunks
            chunk_top_middle = chunk_top_right;
            chunk_middle_middle = chunk_middle_right;
            chunk_bottom_middle = chunk_bottom_right;

            // new right chunks -> right chunks
            chunk_top_right = getChunk(current_chunk_x + 1, current_chunk_y - 1);
            chunk_middle_right = getChunk(current_chunk_x + 1, current_chunk_y);
            chunk_bottom_right = getChunk(current_chunk_x + 1, current_chunk_y + 1);
        }
        else if (current_chunk_x < prev_chunk_x)
        { // Moved left

            // middle chunks -> right chunks
            chunk_top_right = chunk_top_middle;
            chunk_middle_right = chunk_middle_middle;
            chunk_bottom_right = chunk_bottom_middle;

            // left chunks -> middle chunks
            chunk_top_middle = chunk_top_left;
            chunk_middle_middle = chunk_middle_left;
            chunk_bottom_middle = chunk_bottom_left;

            // new left chunks -> left chunks
            chunk_top_left = getChunk(current_chunk_x - 1, current_chunk_y - 1);
            chunk_middle_left = getChunk(current_chunk_x - 1, current_chunk_y);
            chunk_bottom_left = getChunk(current_chunk_x - 1, current_chunk_y + 1);
        }

        if (current_chunk_y > prev_chunk_y)
        { // Moved down

            // middle chunks -> top chunks
            chunk_top_left = chunk_middle_left;
            chunk_top_middle = chunk_middle_middle;
            chunk_top_right = chunk_middle_right;

            // bottom chunks -> middle chunks
            chunk_middle_left = chunk_bottom_left;
            chunk_middle_middle = chunk_bottom_middle;
            chunk_middle_right = chunk_bottom_right;

            // new bottom chunks -> bottom chunks
            chunk_bottom_left = getChunk(current_chunk_x - 1, current_chunk_y + 1);
            chunk_bottom_middle = getChunk(current_chunk_x, current_chunk_y + 1);
            chunk_bottom_right = getChunk(current_chunk_x + 1, current_chunk_y + 1);
        }
        else if (current_chunk_y < prev_chunk_y)
        { // Moved up

            // middle chunks -> bottom chunks
            chunk_bottom_left = chunk_middle_left;
            chunk_bottom_middle = chunk_middle_middle;
            chunk_bottom_right = chunk_middle_right;

            // top chunks -> middle chunks
            chunk_middle_left = chunk_top_left;
            chunk_middle_middle = chunk_top_middle;
            chunk_middle_right = chunk_top_right;

            // new top chunks -> top chunks
            chunk_top_left = getChunk(current_chunk_x - 1, current_chunk_y - 1);
            chunk_top_middle = getChunk(current_chunk_x, current_chunk_y - 1);
            chunk_top_right = getChunk(current_chunk_x + 1, current_chunk_y - 1);
        }

        // Update prev_chunks
        prev_chunk_x = current_chunk_x;
        prev_chunk_y = current_chunk_y;
    }

    // Find local position within chunk (ensures 0-4 even for negatives)
    int local_x = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int local_y = ((y % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    // Stiched grid of nine surrounding chunks
    std::array<std::array<bool, CHUNK_SIZE>, CHUNK_SIZE> terrain{};

    for (int i = 0; i < CHUNK_SIZE; ++i)
    {
        for (int j = 0; j < CHUNK_SIZE; ++j)
        {

            // Conditions for which chunk to pull from
            bool right = (local_x + j - 2) >= CHUNK_SIZE;
            bool left = (local_x + j - 2) < 0;
            bool down = (local_y + i - 2) >= CHUNK_SIZE;
            bool up = (local_y + i - 2) < 0;

            // calculate x/y coordinates for tile in correct chunk to copy to terrain
            int source_x = (local_x + j - 2 + CHUNK_SIZE) % CHUNK_SIZE;
            int source_y = (local_y + i - 2 + CHUNK_SIZE) % CHUNK_SIZE;

            // Use correct chunk's tile in final terrain grid
            if (up) // Top chunks
            {
                if (left)
                    terrain[i][j] = chunk_top_left[source_y][source_x];
                else if (right)
                    terrain[i][j] = chunk_top_right[source_y][source_x];
                else
                    terrain[i][j] = chunk_top_middle[source_y][source_x];
            }
            else if (down) // Bottom chunks
            {
                if (left)
                    terrain[i][j] = chunk_bottom_left[source_y][source_x];
                else if (right)
                    terrain[i][j] = chunk_bottom_right[source_y][source_x];
                else
                    terrain[i][j] = chunk_bottom_middle[source_y][source_x];
            }
            else // Middle chunks
            {
                if (left)
                    terrain[i][j] = chunk_middle_left[source_y][source_x];
                else if (right)
                    terrain[i][j] = chunk_middle_right[source_y][source_x];
                else
                    terrain[i][j] = chunk_middle_middle[source_y][source_x];
            }
        }
    }

    return terrain;
}

void TerrainGenerator::printTerrain(int x, int y)
{
    auto terrain = getTerrain(x, y);
    int mid = CHUNK_SIZE / 2;

    for (int i = 0; i < CHUNK_SIZE; ++i)
    {
        for (int j = 0; j < CHUNK_SIZE; ++j)
        {
            if (i == mid && j == mid)
            {
                std::cout << "R ";
            }
            else
            {
                std::cout << (terrain[i][j] ? "#" : ".") << " ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}