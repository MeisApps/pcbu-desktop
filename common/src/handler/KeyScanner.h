#ifndef PAM_PCBIOUNLOCK_KEYSCANNER_H
#define PAM_PCBIOUNLOCK_KEYSCANNER_H

#include <map>
#include <thread>
#include <vector>

#include "utils/Utils.h"

class KeyScanner {
public:
  ~KeyScanner();

  bool GetKeyState(int key);
  std::map<int, bool> GetAllKeys();

  void Start();
  void Stop();

private:
#ifdef LINUX
  static std::vector<std::string> GetKeyboards();
  void ScanThread(int idx);

  std::vector<int> m_KeyboardFds{};
  std::vector<std::thread> m_ScanThreads{};
  std::mutex m_ScanMutex{};
  bool m_IsRunning{};

  std::vector<std::map<int, bool>> m_KeyMaps{};
#endif
};

#endif // PAM_PCBIOUNLOCK_KEYSCANNER_H
