#include <string>
#include <boost/process.hpp>
#include <spdlog/spdlog.h>
#define PAM_SM_AUTH
#include <security/pam_modules.h>

#ifdef APPLE
#include <security/pam_appl.h>
#endif

#define PCBU_AUTH_PATH "/usr/local/sbin/pcbu_auth"

static int print_pam(struct pam_conv *conv, const std::string& message) {
    struct pam_message msg{};
    struct pam_response *resp = nullptr;
    const struct pam_message* pMsg = &msg;
    if(!conv)
        return PAM_CONV_ERR;

    msg.msg = (char *)message.c_str();
    msg.msg_style = PAM_TEXT_INFO;
    conv->conv(1, &pMsg, &resp, conv->appdata_ptr);
    return PAM_SUCCESS;
}

int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    const char* userName = nullptr;
    const char* serviceName = nullptr;
    struct pam_conv *conv = nullptr;
    auto statusCode = pam_get_item(pamh, PAM_SERVICE, (const void **)&serviceName);
    if (statusCode == PAM_SUCCESS) {
        statusCode = pam_get_user(pamh, (const char **)&userName, nullptr);
        if(statusCode == PAM_SUCCESS) {
            statusCode = pam_get_item(pamh, PAM_CONV, (const void **) &conv);
        }
    }
    if (statusCode != PAM_SUCCESS) {
        //spdlog::error(I18n::Get("error_pam"));
        return PAM_IGNORE;
    }

    std::vector<std::string> args{};
    args.emplace_back(userName);
    args.emplace_back(serviceName);
    try {
        boost::process::ipstream outStream{};
        boost::process::child proc(PCBU_AUTH_PATH, args,
                                   boost::process::std_out > outStream);
        std::string line{};
        while (outStream && std::getline(outStream, line) && !line.empty())
            print_pam(conv, line);

        proc.wait();
        auto result = proc.exit_code();
        if(result == 0)
            return PAM_SUCCESS;
        else if(result == 1)
            return PAM_AUTH_ERR;
    } catch(const std::exception& ex) {
        spdlog::error("Installation is corrupt. {}", ex.what());
    }
    return PAM_IGNORE;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    return PAM_SUCCESS;
}
