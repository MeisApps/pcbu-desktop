#include "WinFirewallHelper.h"

#include <Windows.h>
#include <netfw.h>
#include <boost/filesystem.hpp>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

bool WinFirewallHelper::RemoveAllRulesForProgram(const std::string& exePath) {
    HRESULT hr = S_OK;
    INetFwPolicy2 *pPolicy{};
    INetFwRules *pFwRules{};

    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        spdlog::error("Failed to initialize com.");
        return false;
    }
    hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), (void**)&pPolicy);
    if (FAILED(hr)) {
        spdlog::error("Failed to initialize firewall policy.");
        CoUninitialize();
        return false;
    }

    hr = pPolicy->get_Rules(&pFwRules);
    if (FAILED(hr)) {
        spdlog::error("Failed to get firewall rules.");
        pPolicy->Release();
        CoUninitialize();
        return false;
    }
    long ruleCount{};
    hr = pFwRules->get_Count(&ruleCount);
    if (FAILED(hr)) {
        spdlog::error("Failed to get firewall rule count.");
        pFwRules->Release();
        pPolicy->Release();
        CoUninitialize();
        return false;
    }

    IUnknown *pEnumerator{};
    IEnumVARIANT *pVariant{};
    hr = pFwRules->get__NewEnum(&pEnumerator);
    if (FAILED(hr)) {
        spdlog::error("Failed to get rules enumerator.");
        pFwRules->Release();
        pPolicy->Release();
        CoUninitialize();
        return false;
    }
    hr = pEnumerator->QueryInterface(IID_IEnumVARIANT, (void**)&pVariant);
    if (FAILED(hr)) {
        spdlog::error("Failed to get IEnumVARIANT interface.");
        pEnumerator->Release();
        pFwRules->Release();
        pPolicy->Release();
        CoUninitialize();
        return false;
    }

    VARIANT var{};
    VariantInit(&var);
    while (pVariant->Next(1, &var, nullptr) == S_OK) {
        INetFwRule *pFwRule{};
        hr = V_DISPATCH(&var)->QueryInterface(__uuidof(INetFwRule), (void**)&pFwRule);
        if (FAILED(hr)) {
            VariantClear(&var);
            continue;
        }

        BSTR fwRuleName{};
        hr = pFwRule->get_Name(&fwRuleName);
        if (FAILED(hr)) {
            pFwRule->Release();
            VariantClear(&var);
            continue;
        }
        NET_FW_RULE_DIRECTION direction{};
        hr = pFwRule->get_Direction(&direction);
        if (FAILED(hr)) {
            SysFreeString(fwRuleName);
            pFwRule->Release();
            VariantClear(&var);
            continue;
        }

        if (direction == NET_FW_RULE_DIR_IN || direction == NET_FW_RULE_DIR_OUT) {
            BSTR program{};
            hr = pFwRule->get_ApplicationName(&program);
            if (SUCCEEDED(hr) && program != nullptr) {
                auto programPath = boost::filesystem::path(std::wstring(program));
                if(boost::filesystem::equivalent(programPath, boost::filesystem::path(exePath))) {
                    auto ruleName = StringUtils::FromWideString(fwRuleName);
                    hr = pFwRules->Remove(fwRuleName);
                    if (FAILED(hr))
                        spdlog::error("Failed to remove firewall rule {} for {}.", ruleName, programPath.string());
                    else
                        spdlog::info("Removed rule {} for {}.", ruleName, programPath.string());
                }
                SysFreeString(program);
            }
        }
        SysFreeString(fwRuleName);
        pFwRule->Release();
        VariantClear(&var);
    }

    pVariant->Release();
    pEnumerator->Release();
    pFwRules->Release();
    pPolicy->Release();
    CoUninitialize();
    return true;
}
