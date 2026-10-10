#include <csignal>
#include <unistd.h>

#import <AppKit/AppKit.h>
#include <spdlog/spdlog.h>

#include "AgentController.h"
#include "storage/LoggingSystem.h"

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    signal(SIGPIPE, SIG_IGN);
    LoggingSystem::Init("mac_agent", false);
    auto mode = getuid() == 0 ? AgentMode::LOGIN_WINDOW : AgentMode::LOCK_SCREEN;
    spdlog::info("Agent started. (Mode={}, UID={})", mode == AgentMode::LOGIN_WINDOW ? "LoginWindow" : "LockScreen", getuid());

    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;
    auto session = new AgentController(mode);
    session->Start();

    signal(SIGTERM, SIG_IGN);
    auto termSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
    dispatch_source_set_event_handler(termSource, ^{
      spdlog::info("Agent stopping.");
      session->Stop();
      LoggingSystem::Destroy();
      exit(0);
    });
    dispatch_resume(termSource);

    [NSApp run];
  }
  return 0;
}
