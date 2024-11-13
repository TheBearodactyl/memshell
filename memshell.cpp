#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

struct FileEntry {
  std::string name;
  size_t offset;
  size_t size;
  bool isDirectory;
  size_t parent;
};

class MemoryConsole {
private:
  std::unique_ptr<uint8_t[]> memory;
  std::map<std::string, std::string> envVars;
  bool running;
  size_t currentPosition;
  size_t memorySize;
  std::vector<FileEntry> fileTable;
  size_t currentDir;
  size_t dataStart;

  void initializeFileSystem() {
    fileTable.clear();

    FileEntry root = {"", 0, 0, true, 0};
    fileTable.push_back(root);

    currentDir = 0;
    dataStart = 1024 * 1024;
  }

  std::string getFullPath(size_t index) {
    if (index == 0)
      return "/";

    std::vector<std::string> parts;
    size_t current = index;

    while (current != 0) {
      parts.push_back(fileTable[current].name);
      current = fileTable[current].parent;
    }

    std::string result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
      result += "/" + *it;
    }

    return result.empty() ? "/" : result;
  }

  size_t findFreeSpace(size_t size) {
    std::vector<std::pair<size_t, size_t>> usedRanges;

    for (const auto &file: fileTable) {
      if (! file.isDirectory && file.size > 0) {
        usedRanges.push_back({file.offset, file.offset + file.size});
      }
    }

    std::sort(usedRanges.begin(), usedRanges.end());

    size_t current = dataStart;
    for (const auto &range: usedRanges) {
      if (range.first - current >= size) {
        return current;
      }

      current = range.second;
    }

    if (memorySize - current >= size) {
      return current;
    }

    return SIZE_MAX;
  }

  size_t findFile(const std::string &name, size_t parentDir) {
    for (size_t i = 0; i < fileTable.size(); i++) {
      if (fileTable[i].name == name && fileTable[i].parent == parentDir) {
        return i;
      }
    }

    return SIZE_MAX;
  }

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
        << "exit           - Exit the console\n"
        << "\nFile System Commands:\n"
        << "ls             - List files in current directory\n"
        << "cd <path>      - Change directory\n"
        << "pwd            - Print working directory\n"
        << "mkdir <name>   - Create directory\n"
        << "touch <name>   - Create empty file\n"
        << "write <name> <content> - Write content to file\n"
        << "cat <name>     - Display file content\n"
        << "rm <name>      - Remove file or directory\n"
        << "df             - Show free space\n"
        << "exit           - Exit the console\n";
  }

  size_t parseSize(const std::string &sizeStr) {
    std::istringstream iss(sizeStr);
    double value;
    std::string unit;
    iss >> value >> unit;

    // Convert to uppercase for comparison
    for (char &c: unit) {
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
      for (const auto &pair: envVars) {
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
    } else if (command == "ls") {
      std::cout << "Contents of " << getFullPath(currentDir) << ":\n";
      for (size_t i = 0; i < fileTable.size(); i++) {
        if (fileTable[i].parent == currentDir) {
          std::cout << (fileTable[i].isDirectory ? "d " : "f ") << std::setw(10)
                    << fileTable[i].size << " " << fileTable[i].name << "\n";
        }
      }
    } else if (command == "cd") {
      std::string path;
      iss >> path;

      if (path == "/") {
        currentDir = 0;
      } else if (path == "..") {
        if (currentDir != 0) {
          currentDir = fileTable[currentDir].parent;
        }
      } else {
        size_t targetDir = findFile(path, currentDir);
        if (targetDir != SIZE_MAX && fileTable[targetDir].isDirectory) {
          currentDir = targetDir;
        } else {
          std::cout << "Directory not found\n";
        }
      }
    } else if (command == "pwd") {
      std::cout << getFullPath(currentDir) << std::endl;
    } else if (command == "mkdir") {
      std::string name;
      iss >> name;

      if (findFile(name, currentDir) == SIZE_MAX) {
        FileEntry dir = {name, 0, 0, true, currentDir};
        fileTable.push_back(dir);
        std::cout << "Directory created\n";
      } else {
        std::cout << "Name already exists\n";
      }
    } else if (command == "touch") {
      std::string name;
      iss >> name;

      if (findFile(name, currentDir) == SIZE_MAX) {
        FileEntry file = {name, 0, 0, false, currentDir};
        fileTable.push_back(file);
        std::cout << "File created\n";
      } else {
        std::cout << "File already exists\n";
      }
    } else if (command == "write") {
      std::string name;
      std::string content;
      iss >> name;
      std::getline(iss >> std::ws, content);

      size_t fileIndex = findFile(name, currentDir);
      if (fileIndex != SIZE_MAX && ! fileTable[fileIndex].isDirectory) {
        size_t offset = findFreeSpace(content.length());
        if (offset != SIZE_MAX) {
          std::copy(content.begin(), content.end(), memory.get() + offset);
          fileTable[fileIndex].offset = offset;
          fileTable[fileIndex].size = content.length();
          std::cout << "Content written\n";
        } else {
          std::cout << "Not enough space\n";
        }
      } else {
        std::cout << "File not found or is a directory\n";
      }
    } else if (command == "cat") {
      std::string name;
      iss >> name;

      size_t fileIndex = findFile(name, currentDir);
      if (fileIndex != SIZE_MAX && ! fileTable[fileIndex].isDirectory) {
        if (fileTable[fileIndex].size > 0) {
          std::cout.write(reinterpret_cast<char *>(memory.get() +
                                                   fileTable[fileIndex].offset),
                          fileTable[fileIndex].size);
          std::cout << std::endl;
        }
      } else {
        std::cout << "File not found or is a directory\n";
      }
    } else if (command == "rm") {
      std::string name;
      iss >> name;

      size_t fileIndex = findFile(name, currentDir);
      if (fileIndex != SIZE_MAX) {
        if (! fileTable[fileIndex].isDirectory) {
          fileTable.erase(fileTable.begin() + fileIndex);
          std::cout << "File removed\n";
        } else {
          // Check if directory is empty
          bool isEmpty = true;
          for (const auto &entry: fileTable) {
            if (entry.parent == fileIndex) {
              isEmpty = false;
              break;
            }
          }
          if (isEmpty) {
            fileTable.erase(fileTable.begin() + fileIndex);
            std::cout << "Directory removed\n";
          } else {
            std::cout << "Directory not empty\n";
          }
        }
      } else {
        std::cout << "File or directory not found\n";
      }
    } else if (command == "df") {
      size_t usedSpace = 0;
      for (const auto &file: fileTable) {
        if (! file.isDirectory) {
          usedSpace += file.size;
        }
      }
      std::cout << "Total space: " << formatSize(memorySize) << "\n"
                << "Used space:  " << formatSize(usedSpace) << "\n"
                << "Free space:  " << formatSize(memorySize - usedSpace)
                << "\n";

    } else if (command == "exit") {
      running = false;
    } else {
      std::cout << "Unknown command. Type 'help' for available commands.\n";
    }
  }

public:
  MemoryConsole(size_t initialSize = 2ULL * 1024 * 1024 * 1024)
      : running(true), currentPosition(0), memorySize(0) {
    if (! reallocateMemory(initialSize)) {
      throw std::runtime_error("Failed to allocate initial memory");
    }
    loadEnvironmentVariables();
    initializeFileSystem();
  }

  void run() {
    std::cout << "Memory Console (Initially allocated: "
              << formatSize(memorySize) << ")\n"
              << "Type 'help' for available commands\n";

    std::string cmdLine;
    while (running) {
      std::cout << "\nmc> ";
      std::getline(std::cin, cmdLine);
      if (! cmdLine.empty()) {
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
