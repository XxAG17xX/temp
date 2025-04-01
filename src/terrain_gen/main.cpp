#include "terrain_gen.h"
#include <iostream>

TerrainGenerator tgen(0.2, 107);

// THIS EXECUTABLE IS JUST TO TEST THE LIBRARY, IT WILL EVENTUALLY BE DELETED WHEN IMPLEMENTED IN ROVER/EARTH
int main()
{
    int x = 0, y = 0;
    char direction;
    while (1)
    {
        tgen.printTerrain(x, y);
        std::cout << "Input direction (u/d/l/r):";
        std::cin >> direction;
        switch(direction)
        {
            case 'u':
                y -= 1;
                break;
            case 'd':
                y += 1;
                break;
            case 'l':
                x -= 1;
                break;
            case 'r':
                x += 1;
                break;
            default:
                std::cout << "Invalid Character!\n";
        }
    }
}