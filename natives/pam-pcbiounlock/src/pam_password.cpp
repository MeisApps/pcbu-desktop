#define PAM_SM_PASSWORD
#include <security/pam_modules.h>

int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv) {
  return PAM_AUTHTOK_ERR;
}