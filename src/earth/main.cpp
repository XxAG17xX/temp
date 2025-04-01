#include "earth.h"
#include "utils.h"

#include <asio.hpp>
#include <asio/ts/buffer.hpp>   //memory movement
#include <asio/ts/internet.hpp> //internet
#include <iostream>
#include <regex>

#include "protocols.h"
using asio::ip::udp;

// String to enum helper function
DIRECTION get_direction_from_str(std::string &s) {
  if (s == "left")
    return DIRECTION::LEFT;
  if (s == "right")
    return DIRECTION::RIGHT;
  if (s == "up")
    return DIRECTION::UP;
  if (s == "down")
    return DIRECTION::DOWN;
  // This is internal and should never trigger
  return DIRECTION::UP;
}

// This function takes in the command inputted by the user, and uses regex to
// parse out the command
int executeCommand(std::string &command, EarthBase &base) {
  // Command Regexes
  static const std::regex exit_command("^exit\\s*$");
  static const std::regex health_command("^(health)\\s+([0-9]+)\\s*$");
  static const std::regex help_command("^help\\s*$");
  static const std::regex move_command(
      "^(move)\\s+([0-9]+)\\s+(left|right|up|down)\\s*$");
  static const std::regex terrain_command("^(terrain)\\s+([0-9]+)\\s*$");

  std::smatch match;

  // The dreaded TUI chain of ifs
  if (std::regex_match(command, exit_command)) { // Exit Command
    exit(0);
  } else if (std::regex_match(command, help_command)) { // Help Command
    std::cout << "Commands:\n"
              << "help - lists available commands\n"
              << "move [id] [left/right/up/down] - move an available rover a "
                 "given direction\n"
              << "terrain [id] - display the terrain of a given rover\n"
              << "health [id] - check health status of a given rover\n"
              << "exit - exit the program\n";
  } else if (std::regex_match(command, match, move_command)) { // Move Command

    // Parse command
    uint32_t idx = std::stoi(match[2].str());
    std::string direction = match[3].str();

    // Debug msg
    std::cout << "Requesting rover " << idx << " to move " << direction << "\n";

    // Send request to rover
    base.send_movment_command(idx, get_direction_from_str(direction));
  } else if (std::regex_match(command, match,
                              terrain_command)) { // Terrain Command

    // Parse command
    int id = std::stoi(match[2].str());

    // Debug msg
    std::cout << "Requesting terrain from rover " << id << "\n";

    // TODO: Terrain Logic
  } else if (std::regex_match(command, match,
                              health_command)) { // Health Command
    int rover_id = std::stoi(match[2].str());

    std::cout << "Requesting health report from rover " << rover_id << "...\n";
    base.request_health_report(rover_id);

  } else { // The user either typed in a command that doesn't exist, or with bad
           // arguments
    std::cout << "Error: malformed command\n";
    return 1;
  }

  return 0;
}

int main() {
  try {
    // Set up networking
    asio::io_context io_context;
    EarthBase earthBase(io_context);
    earthBase.start();
    io_context.run();

    // TUI
    std::cout << "WELCOME BASE COMMAND OPERATOR\n";
    std::cout << "(type 'help' for a list of commands)\n";
    std::string command;

    while (1) { // Continuously prompt the user to run a command
      std::cout << "\n>";
      std::getline(std::cin, command);
      executeCommand(command, earthBase);
    }
  } catch (std::exception &e) { // To catch possible errors from the networking
    // construction and exit gracefully
    std::cerr << "Exception: " << e.what() << std::endl;
  }

  return 0;
}
