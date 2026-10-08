#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <string>
#include <unistd.h>
#include <vector>

#define DEBUG 0 // set to 1 to skip actual backup process

// TODO: what happens when:

    // the USB disappears halfway through
    // a file is deleted during synchronization
    // the destination runs out of space
    // the process gets killed halfway through
    // the same file exists on both sides
    // the USB is read-only

namespace fs = std::filesystem;

bool isUSBConnected(std::string UUID) {
  fs::path devicePath = "/dev/disk/by-uuid/" + UUID;
  return fs::exists(devicePath);
}

std::string chooseUSB() {
  FILE *pipe =
      popen("lsblk -no VENDOR,MODEL,NAME | grep USB | awk '{print $NF}'", "r");
  if (!pipe) {
    perror("popen failed");
    return {};
  }

  std::vector<std::string> devices;
  std::string buffer;

  char temp[256];
  while (fgets(temp, sizeof(temp), pipe) != nullptr) {
    buffer = temp;
    size_t len = buffer.length();
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer.erase(len - 1, 1);
    }

    devices.push_back(buffer);
  }

  pclose(pipe);

  std::string selectedDevice;

  if (devices.empty()) {
    std::cerr << "No USB devices found." << std::endl;
    return {};
  } else if (devices.size() == 1) {

    std::cout << "One USB device found: " << std::endl;
    std::cout.put('\n');
    std::system("lsblk -no MODEL,VENDOR,NAME | grep USB");
    std::cout.put('\n');
    std::cout.put('\n');

    selectedDevice = devices[0];
  } else if (devices.size() > 1) {
    int choice = 0;
    std::cout << "Multiple USB devices found: \n" << std::endl;
    std::system("lsblk -no MODEL,VENDOR,NAME | grep USB");
    std::cout.put('\n');
    std::cout.put('\n');
    std::cout << "Please choose a USB device." << std::endl;
    std::cout << "(Pick a number from 1 to " << devices.size() << "): ";

    while (choice < 1 || choice > devices.size()) {
      if (!(std::cin >> choice)) {
        std::cin.clear();
      }
      std::cin.ignore(1000, '\n');
      std::cout.put('\n');
    }
    selectedDevice = devices[choice - 1];
  }

  devices.clear();

  pipe = popen(("lsblk -no NAME,UUID | grep -E " + selectedDevice + "[0-9]+" +
                " | awk '{print $NF}'")
                   .c_str(),
               "r");
  if (!pipe) {
    perror("popen failed");
    return {};
  }

  while (fgets(temp, sizeof(temp), pipe) != nullptr) {
    buffer = temp;
    size_t len = buffer.length();
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer.erase(len - 1, 1);
    }

    devices.push_back(buffer);
  }

  pclose(pipe);

  std::string selectedUUID;

  if (devices.empty()) {
    std::cerr << "No partitions found for the selected USB device."
              << std::endl;
    return {};
  } else if (devices.size() == 1) {
    selectedUUID = devices[0];

    std::cout << "One partition found: " << std::endl;
    std::cout.put('\n');
    std::cout << selectedUUID << std::endl;
    std::cout.put('\n');
    std::cout.put('\n');
  } else if (devices.size() > 1) {
    int choice = 0;

    std::cout << "Multiple partitions found: " << '\n';
    std::system(
        ("lsblk -no NAME,FSTYPE,SIZE | grep -E " + selectedDevice + "[0-9]+")
            .c_str());
    std::cout.put('\n');
    std::cout.put('\n');

    std::cout << "Please choose a partition." << std::endl;
    std::cout << "(Pick a number from 1 to " << devices.size() << "): ";

    while (choice < 1 || choice > devices.size()) {
      if (!(std::cin >> choice)) {
        std::cin.clear();
      }
      std::cin.ignore(1000, '\n');
      std::cout.put('\n');
    }
    selectedUUID = devices[choice - 1];
  }

  devices.clear();

  return selectedUUID;
}

int generateConfigFile() {

  if (fs::exists("/etc/usbsync/usbsync.conf"))
    return 0;

  std::cout << "Generating configuration file usbsync.conf..." << std::endl;

  std::cout << "Enter files or directories to back up, blank line to finish. "
            << std::endl;

  std::vector<std::string> includes;
  std::string include;
  while (true) {
    std::getline(std::cin, include);
    if (include.empty()) {
      break;
    }
    includes.push_back(include);
  }

  std::cout.put('\n');

  fs::create_directories("/etc/usbsync/");
  std::ofstream configFile("/etc/usbsync/usbsync.conf");

  if (!configFile) {
    std::cerr << "Error: Could not open configuration file." << std::endl;
    return 1;
  }

  configFile << "# USBSync configuration file" << std::endl;
  configFile << "# Directories to back up:" << std::endl;

  for (const auto &path : includes) {
    configFile << path << std::endl;
  }

  std::string UUID = chooseUSB();

  if (UUID.empty()) {
    std::cerr << "Error: Failed to choose USB device." << std::endl;
    configFile.close();
    fs::remove("/etc/usbsync/usbsync.conf");
    return 1;
  }

  configFile << "# USB device partition UUID:" << std::endl;
  configFile << UUID << std::endl;
  configFile.close();

  std::cout << "USB device saved to configuration file." << std::endl;
  std::cout << "Configuration file created.\n" << std::endl;

  return 0;
}

std::vector<fs::path> readConfigFile() {

  std::vector<fs::path> paths;
  std::ifstream configFile("/etc/usbsync/usbsync.conf");
  std::string line;

  if (!configFile) {
    std::cerr << "Error: Could not open configuration file." << std::endl;
    return {};
  }

  while (std::getline(configFile, line)) {
    if (!line.empty() && line[0] != '#') {
      paths.push_back(line);
    }
  }

  configFile.close();

  return paths;
}

// USBSYNC_USER should be defined in the systemd service file,
// and should be the username of the user to run the backup for.
fs::path getDocumentsPath() {
  const char *username = std::getenv("USBSYNC_USER");
  struct passwd *pw;

  if (username != nullptr) {
    pw = getpwnam(username);
  } else if (std::getenv("SUDO_USER") != nullptr) {
    pw = getpwnam(std::getenv("SUDO_USER"));
  } else {
    pw = getpwuid(getuid());
  }

  if (pw == nullptr) {
    return {};
  }

  return fs::path(pw->pw_dir) / "Documents";
}

fs::path getMountPoint(const std::string &UUID) {
  std::string cmd = "lsblk -no MOUNTPOINT /dev/disk/by-uuid/" + UUID;

  FILE *pipe = popen(cmd.c_str(), "r");

  if (!pipe) {
    return {};
  }

  char buffer[256];

  if (fgets(buffer, sizeof(buffer), pipe) == nullptr) {
    pclose(pipe);
    return {};
  }

  pclose(pipe);

  std::string mountPoint = buffer;

  if (!mountPoint.empty() && mountPoint.back() == '\n') {
    mountPoint.pop_back();
  }

  return mountPoint;
}

int mountUSB(const std::string &UUID, const fs::path &mountPoint) {
  std::cout << "Mounting USB drive...\n" << std::endl;

  if (!fs::is_directory(mountPoint)) {
    fs::create_directories(mountPoint);
  }

  std::string cmd = "mount -U " + UUID + " \"" + mountPoint.string() + "\"";
  return std::system((cmd).c_str());
}

int unmountUSB(const fs::path &mountPoint) {
  std::cout << "\nUnmounting USB drive..." << std::endl;
  std::string cmd = "umount \"" + mountPoint.string() + "\"";
  return std::system(cmd.c_str());
}

// TODO: account for USB removal during copying
// by copying to a temp dir, maybe checking a hash, then moving
// TODO: account for files being deleted from source after last backup
// TODO: account for files being deleted during backup, maybe by checking a hash
// of the source dir before and after
int copyToUSB(const fs::path &source, const fs::path &destination) {
  if (!fs::exists(source)) {
    std::cerr << "Source path does not exist: " << source << std::endl;
    return 1;
  }

  try {
    int filesCopied = 0;
    for (const fs::directory_entry &entry :
         fs::recursive_directory_iterator(source)) {
      if (!entry.is_regular_file())
        continue;

      fs::path relative = fs::relative(entry.path(), source);

      fs::path destinationPath = destination / relative;
      fs::create_directories(destinationPath.parent_path());

      if (!fs::exists(destinationPath) ||
          fs::last_write_time(entry.path()) >
              fs::last_write_time(destinationPath)) {
        fs::copy_file(entry.path(), destinationPath,
                      fs::copy_options::overwrite_existing);

        filesCopied++;
        std::cout << "\rFiles copied: " << filesCopied << std::flush;
      }
    }

    std::cout.put('\n');

    if (filesCopied == 0) {
      std::cout << "No new files to copy.\n" << std::endl;
    }
    return 0;

  } catch (const fs::filesystem_error &e) {
    std::cerr << "Filesystem error: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}

int main() {

  if (generateConfigFile() != 0) {
    std::cerr << "Error: Configuration file could not be generated."
              << std::endl;
    return 1;
  }

  std::vector<fs::path> includePaths = readConfigFile();
  if (includePaths.empty()) {
    std::cerr << "Error: Configuration file could not be opened or is empty."
              << std::endl;
    return 1;
  }

  std::string UUID = includePaths.back();
  includePaths.pop_back();

  if (!isUSBConnected(UUID)) {
    std::cout << "USB drive is not connected. Skipping backup." << std::endl;
    return 0;
  }

  fs::path documentsPath = getDocumentsPath();
  if (documentsPath.empty()) {
    std::cerr << "Error: Could not determine username." << std::endl;
    return 1;
  }

  if (DEBUG)
    return 0; // debug

  fs::path mountPath = getMountPoint(UUID);
  bool wasMounted = !mountPath.empty();

  if (!wasMounted) {
    mountPath = "/mnt/usbsync";
    if (mountUSB(UUID, mountPath) != 0) {
      std::cerr << "Error: Failed to mount USB drive." << std::endl;
      return 1;
    }
  }

  for (const fs::path &relativePath : includePaths) {
    fs::path sourcePath = documentsPath / relativePath;
    fs::path destinationPath = mountPath / "USBSync" / relativePath;

    if (!fs::exists(sourcePath)) {
      std::cerr << "Warning: Source path does not exist: " << sourcePath
                << std::endl;
      continue;
    }

    std::cout << "Backing up: " << sourcePath << std::endl;

    if (copyToUSB(sourcePath, destinationPath) != 0) {
      std::cerr << "Error: Failed to copy " << sourcePath << std::endl;

      if (!wasMounted && unmountUSB(mountPath) != 0) {
        std::cerr << "Error: Failed to unmount USB drive." << std::endl;
      }
      return 1;
    }
  }

  if (!wasMounted && unmountUSB(mountPath) != 0) {
    std::cerr << "Error: Failed to unmount USB drive." << std::endl;
    return 1;
  }

  if (wasMounted) {
    std::cout
        << "Not unmounting as the device was mounted before sync started..."
        << std::endl;
  }
  std::cout << "Backup completed successfully." << std::endl;
  return 0;
}
