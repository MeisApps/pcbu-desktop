#include <WinCred.h>
#include <Windows.h>
#include <iostream>

#pragma comment(lib, "credui.lib")
#pragma comment(lib, "Secur32.lib")

int main() {
  std::cout << "Hello World!\n";

  BOOL save = false;
  DWORD authPackage = 0;
  LPVOID authBuffer;
  ULONG authBufferSize = 0;
  CREDUI_INFOW credUiInfo;

  credUiInfo.pszCaptionText = L"VBoxCaption";
  credUiInfo.pszMessageText = L"VBoxMessage";
  credUiInfo.cbSize = sizeof(credUiInfo);
  credUiInfo.hbmBanner = nullptr;
  credUiInfo.hwndParent = nullptr;

  DWORD rc = CredUIPromptForWindowsCredentialsW(&(credUiInfo), 0, &(authPackage), nullptr, 0, &authBuffer, &authBufferSize, &(save), 0);
  printf("Test returned %ld\n", rc);
  return rc;
}
