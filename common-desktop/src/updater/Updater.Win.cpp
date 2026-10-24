#include "Updater.h"

std::string Updater::GetUpdateFileName() {
  return "Update-PulseUnlock.exe";
}

std::string Updater::InstallUpdate(const boost::filesystem::path &downloadPath) {
  return downloadPath.string();
}
