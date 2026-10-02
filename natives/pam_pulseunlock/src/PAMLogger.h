#ifndef PAMLOGGER_H
#define PAMLOGGER_H

#include <string>
#include <syslog.h>

#include <security/pam_appl.h>
#ifdef LINUX
#include <security/pam_ext.h>
#endif

class PAMLogger {
public:
  static void Error(pam_handle_t *pamh, const std::string &message) {
#ifdef LINUX
    pam_syslog(pamh, LOG_ERR, "%s", message.c_str());
#else
    syslog(LOG_AUTHPRIV | LOG_ERR, "pam_pulseunlock: %s", message.c_str());
#endif
  }

private:
  PAMLogger() = default;
};

#endif
