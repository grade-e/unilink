# Unilink Quick Start Guide

Get started with unilink in 5 minutes!

## Installation

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt update && sudo apt install -y \
  build-essential cmake libboost-dev libboost-system-dev
```

### Build & Install
```bash
git clone https://github.com/grade-e/interface-socket.git
cd interface-socket
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

---

## Your First TCP Client (30 seconds)

```cpp
#include <iostream>
#include "unilink/unilink.hpp"

int main() {
    // Create a TCP client - it's that simple!
    auto client = unilink::tcp_client("127.0.0.1", 8080)
        .on_connect([]() {
            std::cout << "Connected!" << std::endl;
        })
        .on_data([](const std::string& data) {
            std::cout << "Received: " << data << std::endl;
        })
        .auto_start(true)
        .build();
    
    // Send a message
    if (client->is_connected()) {
        client->send("Hello, Server!");
    }
    
    // Keep running
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
```

**Compile:**
```bash
g++ -std=c++17 my_client.cc -lunilink -lboost_system -pthread -o my_client
./my_client
```

---

## Your First TCP Server (30 seconds)

```cpp
#include <iostream>
#include "unilink/unilink.hpp"

int main() {
    // Create a TCP server
    auto server = unilink::tcp_server(8080)
        .on_connect([](size_t client_id, const std::string& ip) {
            std::cout << "Client " << client_id << " connected from " << ip << std::endl;
        })
        .on_data([](size_t client_id, const std::string& data) {
            std::cout << "Client " << client_id << ": " << data << std::endl;
        })
        .auto_start(true)
        .build();
    
    std::cout << "Server listening on port 8080..." << std::endl;
    
    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
```

---

## Your First Serial Device (30 seconds)

```cpp
#include <iostream>
#include "unilink/unilink.hpp"

int main() {
    // Create serial connection
    auto serial = unilink::serial("/dev/ttyUSB0", 115200)
        .on_connect([]() {
            std::cout << "Serial port opened!" << std::endl;
        })
        .on_data([](const std::string& data) {
            std::cout << "Received: " << data << std::endl;
        })
        .auto_start(true)
        .build();
    
    // Send data
    serial->send("AT\r\n");
    
    // Keep running
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 0;
}
```

---

## Common Patterns

### Pattern 1: Auto-Reconnection
```cpp
auto client = unilink::tcp_client("server.com", 8080)
    .retry_interval(3000)  // Retry every 3 seconds
    .auto_start(true)
    .build();
```

### Pattern 2: Error Handling
```cpp
auto server = unilink::tcp_server(8080)
    .on_error([](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    })
    .enable_port_retry(true, 5, 1000)  // 5 retries, 1 sec interval
    .build();
```

### Pattern 3: Member Function Callbacks
```cpp
class MyApp {
    void on_data(const std::string& data) {
        // Handle data
    }
    
    void start() {
        auto client = unilink::tcp_client("127.0.0.1", 8080)
            .on_data(this, &MyApp::on_data)  // Member function!
            .build();
    }
};
```

### Pattern 4: Single vs Multi-Client Server
```cpp
// Single client only (reject others)
auto server = unilink::tcp_server(8080)
    .single_client()
    .build();

// Multiple clients (default)
auto server = unilink::tcp_server(8080)
    .multi_client()
    .build();
```

---

## Next Steps

1. **Read the API Guide**: `docs/API_GUIDE.md`
2. **Check Examples**: `examples/` directory
3. **Run Tests**: `cd build && ctest`
4. **View Full Docs**: `docs/html/index.html` (run `make docs` first)

---

## Troubleshooting

### Can't connect to server?
```cpp
// Enable logging to see what's happening
unilink::common::Logger::instance().set_level(unilink::common::LogLevel::DEBUG);
unilink::common::Logger::instance().set_console_output(true);
```

### Port already in use?
```cpp
auto server = unilink::tcp_server(8080)
    .enable_port_retry(true, 5, 1000)  // Try 5 times
    .build();
```

### Need independent IO thread?
```cpp
// For testing or isolation
auto client = unilink::tcp_client("127.0.0.1", 8080)
    .use_independent_context(true)
    .build();
```

---

## Support

- **GitHub Issues**: https://github.com/grade-e/interface-socket/issues
- **Documentation**: `docs/` directory
- **Examples**: `examples/` directory

Happy coding! 🚀

