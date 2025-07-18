#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <fnmatch.h>

#define DEBUG 0 // set to 0 to disable debug output

namespace fs = std::filesystem;

int mountUSB(bool mount, std::string UUID) { // true for mount, false for unmount
  int result;
  if (mount) {
    std::cout << "Mounting USB drive..." << std::endl;

    if (!fs::is_directory("/mnt/usbbackup")) {
      fs::create_directories("/mnt/usbbackup");
    }

    result = std::system(("sudo mount -U " + UUID + " /mnt/usbbackup").c_str());
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

      fs::path relative = fs::relative(entry.path(), source);

      for (const fs::path& excludedPath : excludePaths) {
        if (fnmatch(excludedPath.string().c_str(), relative.string().c_str(), 0) == 0) continue;
      }

      fs::path destinationPath = destination / relative;
      fs::create_directories(destinationPath.parent_path());

      if (!fs::exists(destinationPath) || fs::last_write_time(entry.path()) > fs::last_write_time(destinationPath)) {
        fs::copy_file(entry.path(), destinationPath, fs::copy_options::overwrite_existing);

        filesCopied++;
        std::cout << "\rFiles copied: " << filesCopied << std::flush;
      }
    }

    std::cout << std::endl;

    if (filesCopied == 0) {
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

int chooseUSB() {
  FILE* pipe = popen("lsblk -o VENDOR,MODEL,NAME | grep USB | awk '{print $NF}'", "r");
  if (!pipe) {
    perror("popen failed");    
    return 1;
  }


  std::vector<std::string> devices;
  std::string buffer; // buffer holds output

  char temp[256];
  while (fgets(temp, sizeof(temp), pipe) != nullptr) {
    buffer = temp;
    size_t len = buffer.length();
    if (len > 0 && buffer[len-1] == '\n') {
        buffer.erase(len-1, 1);
    }

    devices.push_back(buffer);
    
  }

  pclose(pipe);

  std::string selectedDevice;

  if (devices.empty()) {
    std::cerr << "No USB devices found." << std::endl;
    return 1;
  } else if (devices.size() == 1) {

    std::cout << "One USB device found: " << std::endl;
    std::cout << '\n';
    std::system("lsblk -o MODEL,VENDOR,NAME | grep USB");
    std::cout << '\n';

    selectedDevice = devices[0];
  } else if (devices.size() > 1) {
    int choice = 0;
    std::cout << "Multiple USB devices found: " << '\n' << std::endl;
    std::system("lsblk -o MODEL,VENDOR,NAME | grep USB");
    std::cout << '\n' << "Please choose a USB device." << std::endl;
    std::cout<< "(Pick a number from 1 to " << devices.size() << "): ";

    while (choice < 1 || choice > devices.size()) {
      std::cin >> choice;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << std::endl;
    }
    selectedDevice = devices[choice - 1];
    
  }
  
  devices.clear();

  pipe = popen(("lsblk -o NAME,UUID | grep -E " + selectedDevice + "[0-9]+" + " | awk '{print $NF}'").c_str(), "r");
  if (!pipe) {
    perror("popen failed");
    return 1;
  }

  while (fgets(temp, sizeof(temp), pipe) != nullptr) {
    buffer = temp;
    size_t len = buffer.length();
    if (len > 0 && buffer[len-1] == '\n') {
        buffer.erase(len-1, 1);
    }

    devices.push_back(buffer);
  }

  pclose(pipe);

  if (devices.empty()) { // TODO handle case where there's 1 partition
    std::cerr << "No partitions found for the selected USB device." << std::endl;
    return 1;
  } else if (devices.size() == 1) {
    std::cout << "One partition found: " << devices[0] << std::endl;
    std::cout << '\n';
  } else if (devices.size() > 1) {
    int choice = 0;

    std::cout << "Multiple partitions found: " << '\n';
    std::system(("lsblk -o NAME,FSTYPE,SIZE | grep -E " + selectedDevice + "[0-9]+").c_str());
    std::cout << '\n';

    std::cout << "Please choose a partition." << std::endl;
    std::cout << "(Pick a number from 1 to " << devices.size() << "): ";

    while (choice < 1 || choice > devices.size()) {
      std::cin >> choice;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << std::endl;
    }
    selectedDevice = devices[choice - 1];
  }

  devices.clear();

  std::ofstream configFile("/etc/usbsync/usbsync.conf", std::ios::app);

  configFile << selectedDevice << std::endl;

  configFile.close();
  
  std::cout << "USB device saved to configuration file." << std::endl;

  return 0;
}

void generateConfigFile() {

  if (fs::exists("/etc/usbsync/usbsync.conf")) return;
  
  std::cout << "Generating configuration file usbsync.conf..." << std::endl;
  std::cout << "Enter your linux username: ";

  std::string username;
    
  std::cin >> username;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
  std::cout << std::endl;

  std::cout << "Enter files or directories to exclude, blank line to finish. " << std::endl;

  std::vector<std::string> excludes;
  std::string exclude;
  while (true) {
    std::getline(std::cin, exclude);
    excludes.push_back(exclude);
    if (exclude.empty()) {
      break;
    }
  }

  std::cout << std::endl;

  fs::create_directories("/etc/usbsync/");
  std::ofstream configFile("/etc/usbsync/usbsync.conf");

  for (const auto& ex : excludes) {
    configFile << ex << std::endl;
  }

  configFile << "/home/" << username << "/Documents" << std::endl;

  configFile.close();

  if (chooseUSB() != 0) {
    std::cerr << "Error: Failed to choose USB device." << std::endl;
    fs::remove("/etc/usbsync/usbsync.conf");
    return;
  }

  std::cout << "Configuration file created.\n" << std::endl;  
}

std::vector<fs::path> readConfigFile() {

  std::vector<fs::path> paths;
  std::ifstream configFile("/etc/usbsync/usbsync.conf");

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

  std::string usbUUID = excludePaths.back();
  excludePaths.pop_back();

  fs::path documentsPath = excludePaths.back();
  excludePaths.pop_back();

  if (DEBUG) return 0; // debug

  if (mountUSB(true, usbUUID) != 0) {
    std::cerr << "Error: Failed to mount USB drive." << std::endl;
    mountUSB(false, usbUUID);
    return 1;
  }

  fs::path usbPath = "/mnt/usbbackup/USBSync";
  
  if (copyToUSB(documentsPath, usbPath, excludePaths) != 0) {
    std::cerr << "Error: Failed to copy files to USB drive. " << std::endl;
    mountUSB(false, usbUUID);
    return 1;
  }

  mountUSB(false, usbUUID);
  std::cout << "\nBackup completed successfully." << std::endl;
  return 0;
}
