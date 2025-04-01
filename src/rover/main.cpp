#include "rover.h"

#include <asio.hpp>
#include <iostream>

const char *EARTH_IP = "127.0.0.1";

int main() {
  // Initialize Rover Class
  asio::io_context io_context;
  Rover rover(io_context, EARTH_IP);

  // Start executable loop
  std::cout << "Attempting connection with Houston..." << std::endl;
  rover.start();
  rover.printCurrentTerrain();
  io_context.run();

  // Keep the program running (all threads are detached)
  while (1) {
  }

  return 0;
}
