// === main.cpp ===
#include "health.h"
#include <cstdlib>
#include <ctime>
#include <asio.hpp>

int main() {
    asio::io_context io;
    listen_for_health_requests(io, 9090);
    return 0;
}
