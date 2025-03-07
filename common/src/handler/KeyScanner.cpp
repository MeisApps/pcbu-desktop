#include "KeyScanner.h"

#include <spdlog/spdlog.h>

#ifdef WINDOWS
#include <Windows.h>
#elif LINUX
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <linux/input.h>
#include <thread>
#include <unistd.h>
#elif APPLE
#include <Carbon/Carbon.h>
#endif

KeyScanner::~KeyScanner() {
  Stop();
}

bool KeyScanner::GetKeyState(int key) {
#ifdef _WIN32
  return GetAsyncKeyState(key) < 0;
#endif
#ifdef LINUX
  std::lock_guard<std::mutex> lock(m_ScanMutex);
  for(auto keyMap : m_KeyMaps) {
    if(keyMap.count(key) && keyMap[key])
      return true;
  }
  return false;
#endif
#ifdef APPLE
  unsigned char keyMap[16];
  GetKeys((BigEndianUInt32 *)&keyMap);
  return (0 != ((keyMap[key >> 3] >> (key & 7)) & 1));
#endif
}

std::map<int, bool> KeyScanner::GetAllKeys() {
#ifdef _WIN32
  auto map = std::map<int, bool>();
  for(int i = 0; i < 0xA6; i++) {
    map[i] = GetKeyState(i);
  }
  return map;
#endif
#ifdef LINUX
  std::lock_guard<std::mutex> lock(m_ScanMutex);
  if(m_KeyMaps.empty())
    return {};
  return m_KeyMaps[0]; // ToDo
#endif
#ifdef APPLE
  auto map = std::map<int, bool>();
  for(int i = 0; i < 0x7E; i++) {
    map[i] = GetKeyState(i);
  }
  return map;
#endif
}

void KeyScanner::Start() {
#ifdef LINUX
  if(m_IsRunning)
    return;

  m_IsRunning = true;
  for(const auto &keyboard : GetKeyboards()) {
    int kbd = open(keyboard.c_str(), O_RDONLY);
    if(kbd == -1) {
      spdlog::error("Keyboard open() failed. (Code={})", errno);
      continue;
    }
    int flags = fcntl(kbd, F_GETFL, 0);
    if(flags == -1) {
      spdlog::error("Keyboard fcntl() failed. (Code={})", errno);
      continue;
    }
    if(fcntl(kbd, F_SETFL, flags | O_NONBLOCK) == -1) {
      spdlog::error("Keyboard fcntl(O_NONBLOCK) failed. (Code={})", errno);
      continue;
    }
    m_KeyboardFds.push_back(kbd);
  }
  if(m_KeyboardFds.empty()) {
    spdlog::error("No keyboards found.");
    return;
  }
  m_KeyMaps.clear();
  for(int i = 0; i < m_KeyboardFds.size(); i++) {
    m_KeyMaps.emplace_back();
    m_ScanThreads.emplace_back(&KeyScanner::ScanThread, this, i);
  }
#endif
}

void KeyScanner::Stop() {
#ifdef LINUX
  if(!m_IsRunning)
    return;

  m_IsRunning = false;
  for(auto fd : m_KeyboardFds)
    close(fd);
  for(auto &thread : m_ScanThreads)
    if(thread.joinable())
      thread.join();
  m_KeyboardFds.clear();
  m_ScanThreads.clear();
#endif
}

#ifdef LINUX
void KeyScanner::ScanThread(int idx) {
  while(m_IsRunning) {
    struct input_event ie{};
    if(read(m_KeyboardFds[idx], &ie, sizeof(ie)) == sizeof(ie)) {
      m_ScanMutex.lock();
      m_KeyMaps[idx][ie.code] = ie.value != 0;
      m_ScanMutex.unlock();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

std::vector<std::string> KeyScanner::GetKeyboards() {
  auto keyboards = std::vector<std::string>();
  for(const auto &entry : std::filesystem::directory_iterator("/dev/input/by-path/")) {
    if(entry.path().filename().string().ends_with("-event-kbd"))
      keyboards.push_back(entry.path().string());
  }
  return keyboards;
}
#endif
