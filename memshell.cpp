#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <windows.h>

class MemoryConsole {
private:
  std::unique_ptr<uint8_t[]> memory;
  std::map<std::string, std::string> envVars;
  bool running;
  size_t currentPosition;
  size_t memorySize;

  void loadEnvironmentVariables() {
    LPWCH envStrings = GetEnvironmentStringsW();
    if (envStrings != nullptr) {
      LPWCH current = envStrings;
      while (*current) {
        std::wstring envPair(current);
        size_t pos = envPair.find(L'=');
        if (pos != std::wstring::npos) {
          std::string name(envPair.begin(), envPair.begin() + pos);
          std::string value(envPair.begin() + pos + 1, envPair.end());
          envVars[name] = value;
        }
        current += wcslen(current) + 1;
      }
      FreeEnvironmentStringsW(envStrings);
    }
  }

  bool reallocateMemory(size_t newSize) {
    try {
      std::unique_ptr<uint8_t[]> newMemory =
          std::make_unique<uint8_t[]>(newSize);
      std::fill_n(newMemory.get(), newSize, 0);

      if (memory) {
        const size_t copySize = (std::min)(memorySize, newSize);
        std::copy_n(memory.get(), copySize, newMemory.get());
      }

      memory = std::move(newMemory);
      memorySize = newSize;
      return true;
    } catch (const std::bad_alloc &e) {
      std::cerr << "Failed to allocate memory: " << e.what() << std::endl;
      return false;
    }
  }

  std::string formatSize(size_t bytes) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;

    while (size >= 1024 && unit < 4) {
      size /= 1024;
      unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    return oss.str();
  }

  void displayHelp() {
    std::cout
        << "Available commands:\n"
        << "help           - Display this help message\n"
        << "env            - Display environment variables\n"
        << "peek <offset>  - Display memory content at offset\n"
        << "poke <offset> <value> - Write byte value at offset\n"
        << "system <cmd>   - Execute system command\n"
        << "memsize        - Display current memory allocation\n"
        << "resize <size>  - Resize memory allocation (e.g., '1GB', '512MB')\n"
        << "exit           - Exit the console\n";
  }

  size_t parseSize(const std::string &sizeStr) {
    std::istringstream iss(sizeStr);
    double value;
    std::string unit;
    iss >> value >> unit;

    // Convert to uppercase for comparison
    for (char &c : unit) {
      c = toupper(c);
    }

    size_t multiplier = 1;
    if (unit == "KB" || unit == "K")
      multiplier = 1024ULL;
    else if (unit == "MB" || unit == "M")
      multiplier = 1024ULL * 1024;
    else if (unit == "GB" || unit == "G")
      multiplier = 1024ULL * 1024 * 1024;
    else if (unit == "TB" || unit == "T")
      multiplier = 1024ULL * 1024 * 1024 * 1024;

    return static_cast<size_t>(value * multiplier);
  }

  void executeCommand(const std::string &cmdLine) {
    std::istringstream iss(cmdLine);
    std::string command;
    iss >> command;

    if (command == "help") {
      displayHelp();
    } else if (command == "env") {
      for (const auto &pair : envVars) {
        std::cout << pair.first << "=" << pair.second << std::endl;
      }
    } else if (command == "peek") {
      size_t offset;
      iss >> offset;
      if (offset < memorySize) {
        std::cout << "Memory at offset " << offset << ": "
                  << static_cast<int>(memory[offset]) << std::endl;
      } else {
        std::cout << "Invalid offset\n";
      }
    } else if (command == "poke") {
      size_t offset;
      int value;
      iss >> offset >> value;
      if (offset < memorySize) {
        memory[offset] = static_cast<uint8_t>(value);
        std::cout << "Written value " << value << " at offset " << offset
                  << std::endl;
      } else {
        std::cout << "Invalid offset\n";
      }
    } else if (command == "system") {
      std::string cmd;
      std::getline(iss >> std::ws, cmd);
      system(cmd.c_str());
    } else if (command == "memsize") {
      std::cout << "Current memory allocation: " << formatSize(memorySize)
                << std::endl;
    } else if (command == "resize") {
      std::string sizeStr;
      std::getline(iss >> std::ws, sizeStr);
      size_t newSize = parseSize(sizeStr);

      std::cout << "Attempting to resize memory to " << formatSize(newSize)
                << "..." << std::endl;

      if (reallocateMemory(newSize)) {
        std::cout << "Memory successfully resized to " << formatSize(memorySize)
                  << std::endl;
      } else {
        std::cout << "Failed to resize memory. Current size remains at "
                  << formatSize(memorySize) << std::endl;
      }
    } else if (command == "exit") {
      running = false;
    } else {
      std::cout << "Unknown command. Type 'help' for available commands.\n";
    }
  }

public:
  MemoryConsole(size_t initialSize = 2ULL * 1024 * 1024 * 1024)
      : running(true), currentPosition(0), memorySize(0) {
    if (!reallocateMemory(initialSize)) {
      throw std::runtime_error("Failed to allocate initial memory");
    }
    loadEnvironmentVariables();
  }

  void run() {
    std::cout << "Memory Console (Initially allocated: "
              << formatSize(memorySize) << ")\n"
              << "Type 'help' for available commands\n";

    std::string cmdLine;
    while (running) {
      std::cout << "\nmc> ";
      std::getline(std::cin, cmdLine);
      if (!cmdLine.empty()) {
        executeCommand(cmdLine);
      }
    }
  }
};

int main() {
  try {
    MemoryConsole console;
    console.run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
