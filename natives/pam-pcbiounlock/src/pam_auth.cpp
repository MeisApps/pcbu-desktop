#include <boost/process/v1.hpp>
#include <spdlog/spdlog.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

#ifdef APPLE
#include <security/pam_appl.h>
#endif

constexpr auto PCBU_AUTH_PATH = "/usr/local/sbin/pcbu_auth";
constexpr int PASSWORD_PIPE = 3;

static int print_pam(struct pam_conv *conv, const std::string &message) {
  struct pam_message msg{};
  struct pam_response *resp = nullptr;
  const struct pam_message *pMsg = &msg;
  if(!conv)
    return PAM_CONV_ERR;

  msg.msg = const_cast<char *>(message.c_str());
  msg.msg_style = PAM_TEXT_INFO;
  conv->conv(1, &pMsg, &resp, conv->appdata_ptr);
  return PAM_SUCCESS;
}

int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
  const char *userName = nullptr;
  const char *serviceName = nullptr;
  struct pam_conv *conv = nullptr;
  auto statusCode = pam_get_item(pamh, PAM_SERVICE, reinterpret_cast<const void **>(&serviceName));
  if(statusCode == PAM_SUCCESS) {
    statusCode = pam_get_user(pamh, (const char **)&userName, nullptr);
    if(statusCode == PAM_SUCCESS) {
      statusCode = pam_get_item(pamh, PAM_CONV, (const void **)&conv);
    }
  }
  if(statusCode != PAM_SUCCESS) {
    spdlog::error("Failed to get PAM user info.");
    return PAM_IGNORE;
  }

  int pipeFd[2]{};
  bool hasPipe = false;
  if(!pipe(pipeFd)) {
    fcntl(pipeFd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeFd[1], F_SETFD, FD_CLOEXEC);
    hasPipe = true;
  } else {
    spdlog::error("Failed to create password pipe.");
  }

  int pamResult = PAM_IGNORE;
  try {
    std::vector<std::string> args{};
    args.emplace_back(userName);

    boost::process::v1::ipstream outStream{};
    boost::process::v1::child proc;
    if(hasPipe) {
      proc = boost::process::v1::child(PCBU_AUTH_PATH, args, boost::process::v1::std_out > outStream,
                                       boost::process::v1::posix::fd.bind(PASSWORD_PIPE, pipeFd[1]));
    } else {
      proc = boost::process::v1::child(PCBU_AUTH_PATH, args, boost::process::v1::std_out > outStream);
    }
    std::string line{};
    while(outStream && std::getline(outStream, line) && !line.empty())
      print_pam(conv, line);
    proc.wait();

    std::string password{};
    if(hasPipe) {
      close(pipeFd[1]);
      char buffer[512]{};
      ssize_t bytesRead{};
      while((bytesRead = read(pipeFd[0], buffer, sizeof(buffer))) > 0) {
        password.append(buffer, static_cast<size_t>(bytesRead));
      }
      close(pipeFd[0]);
    }

    auto result = proc.exit_code();
    if(result == 0) {
      if(!password.empty() && password[0] != '\0') {
        pam_set_item(pamh, PAM_AUTHTOK, password.c_str());
      } else {
        pamResult = PAM_SUCCESS;
      }
    }
    if(result == 1) {
      pamResult = PAM_AUTH_ERR;
    }
  } catch(const std::exception &ex) {
    spdlog::error("Installation is corrupt. {}", ex.what());
  }
  return pamResult;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
  return PAM_SUCCESS;
}
