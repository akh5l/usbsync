#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <fnmatch.h>

namespace fs = std::filesystem;

int mountUSB(bool arg) { // true for mount, false for unmount
  int result;
  if (arg) {
    std::cout << "Mounting USB drive..." << std::endl;
    // change /dev/sdc1 so that it can find the desired drive and save it in some config file

    const char* cmd = "sudo mount /dev/sdc1 /mnt/usbbackup";
    result = std::system(cmd);
  }
  else {
    std::cout << "Unmounting USB drive..." << std::endl;
    const char* cmd = "sudo umount /mnt/usbbackup";
    result = std::system(cmd);
  }
  return result;
}

int copyToUSB(const fs::path& source, const fs::path& destination, const std::vector<fs::path>& excludePaths) {
  if (!fs::exists(source)) {
      std::cerr << "Source path does not exist: " << source << std::endl;
      return 1;
    }

  try {
    int filesCopied = 0;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(source)) {      
      if (!entry.is_regular_file()) continue;

      bool excluded = false;
      fs::path relative = fs::relative(entry.path(), source);

      for (const fs::path& excludedPath : excludePaths) {
        if (fnmatch(excludedPath.string().c_str(), relative.string().c_str(), 0) == 0) {
          excluded = true;
          break;
        }        
      }
      if (excluded) continue;

      fs::path destinationPath = destination / relative;
      fs::create_directories(destinationPath.parent_path());

      if (!fs::exists(destinationPath) || fs::last_write_time(entry.path()) > fs::last_write_time(destinationPath)) {
        fs::copy_file(entry.path(), destinationPath, fs::copy_options::overwrite_existing);
         std::cout << "Copied: " << entry.path() << " to " << destinationPath << '\n';
         filesCopied++;
      }
    }

    if (filesCopied != 0) {
      std::cout << "Files copied: " << filesCopied << '\n';
    } else {
      std::cout << "No new files to copy." << std::endl;
    }
    return 0;

  } catch (const fs::filesystem_error& e) {
    std::cerr << "Filesystem error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

void generateConfigFile() {
  if (fs::exists("usbsync.conf")) return;

  std::ofstream configFile("usbsync.conf");
  std::cout << "Generating configuration file usbsync.conf..." << std::endl;
  std::cout << "Enter your linux username: ";

  std::string username;
    
  std::cin >> username;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  configFile << "/home/" << username << "/Documents" << std::endl;
    
  std::cout << std::endl;

  std::cout << "Enter files or directories to exclude, blank line to finish. " << std::endl;

  std::string exclude;
  while (true) {
    std::getline(std::cin, exclude);
    configFile << exclude << std::endl;
    if (exclude.empty()) {
      break;
    }
  }

  std::cout << std::endl;

  std::cout << "Configuration file usbsync.conf created." << std::endl;
  configFile.close();
}

std::vector<fs::path> readConfigFile() {

  std::vector<fs::path> paths;
  std::ifstream configFile("usbsync.conf");

  std::string line;

  while (std::getline(configFile, line)) {
    if (!line.empty()) {
        paths.push_back(line);
    }
  }

  configFile.close();

  return paths;
}

int main() {
  generateConfigFile();
  std::vector<fs::path> excludePaths = readConfigFile();

  fs::path documentsPath = excludePaths.front();
  excludePaths.erase(excludePaths.begin());


  if (mountUSB(true) != 0) {
    std::cerr << "Error: Failed to mount USB drive." << std::endl;
    return 1;
  }

  fs::path usbPath = "/mnt/usbbackup/Backup"; // more robust solution?
  
  if (copyToUSB(documentsPath, usbPath, excludePaths) != 0) {
    std::cerr << "Error: Failed to copy files to USB drive. " << std::endl;
    mountUSB(false);
    return 1;
  }

  mountUSB(false);
  std::cout << "Backup completed successfully." << std::endl;
  return 0;
}
