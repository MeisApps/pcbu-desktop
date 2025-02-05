//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// CSampleProvider implements ICredentialProvider, which is the main
// interface that logonUI uses to decide which tiles to display.
// In this sample, we will display one tile that uses each of the nine
// available UI controls.

#include <initguid.h>
#include "CSampleProvider.h"
#include "CUnlockCredential.h"
#include "guid.h"

#include "storage/LoggingSystem.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/StringUtils.h"

CSampleProvider::CSampleProvider() :
    _cRef(1),
    _rgCredProvFieldDescriptors(),
    _pCredProviderUserArray(nullptr),
    _pCredProvEvents(nullptr),
    _upAdviseContext(0),
    _fRecreateEnumeratedCredentials(true),
    _cpus()
{
    DllAddRef();
    LoggingSystem::Init("module");

    AddFieldDescriptor(SFI_TILEIMAGE, CPFT_TILE_IMAGE, "Image", CPFG_CREDENTIAL_PROVIDER_LOGO);
    AddFieldDescriptor(SFI_USERNAME, CPFT_SMALL_TEXT, "Username");
    AddFieldDescriptor(SFI_MESSAGE, CPFT_SMALL_TEXT, "Message");
    AddFieldDescriptor(SFI_PASSWORD, CPFT_PASSWORD_TEXT, I18n::Get("password"));
    AddFieldDescriptor(SFI_SUBMIT_BUTTON, CPFT_SUBMIT_BUTTON, "Submit");
}

CSampleProvider::~CSampleProvider()
{
    for(const auto cred : _pCredentials)
        cred->Release();
    _pCredentials.clear();
    if (_pCredProviderUserArray != nullptr)
    {
        _pCredProviderUserArray->Release();
        _pCredProviderUserArray = nullptr;
    }
    for(auto& desc : _rgCredProvFieldDescriptors)
        CoTaskMemFree(desc.pszLabel);
    DllRelease();
    LoggingSystem::Destroy();
}

void CSampleProvider::AddFieldDescriptor(DWORD id, CREDENTIAL_PROVIDER_FIELD_TYPE type, const std::string& label, GUID guid) {
    LPWSTR labelCopy{};
    SHStrDupW(StringUtils::ToWideString(label).c_str(), &labelCopy);
    _rgCredProvFieldDescriptors.emplace_back(id, type, labelCopy, guid);
}

// SetUsageScenario is the provider's cue that it's going to be asked for tiles
// in a subsequent call.
HRESULT CSampleProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    DWORD /*dwFlags*/)
{
    HRESULT hr;

    // Decide which scenarios to support here. Returning E_NOTIMPL simply tells the caller
    // that we're not designed for that scenario.
    switch (cpus)
    {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
    case CPUS_CREDUI:
        // The reason why we need _fRecreateEnumeratedCredentials is because ICredentialProviderSetUserArray::SetUserArray() is called after ICredentialProvider::SetUsageScenario(),
        // while we need the ICredentialProviderUserArray during enumeration in ICredentialProvider::GetCredentialCount()
        _cpus = cpus;
        _fRecreateEnumeratedCredentials = true;
        hr = S_OK;
        break;

    case CPUS_CHANGE_PASSWORD:
        hr = E_NOTIMPL;
        break;

    default:
        hr = E_INVALIDARG;
        break;
    }
    return hr;
}

// SetSerialization takes the kind of buffer that you would normally return to LogonUI for
// an authentication attempt.  It's the opposite of ICredentialProviderCredential::GetSerialization.
// GetSerialization is implement by a credential and serializes that credential.  Instead,
// SetSerialization takes the serialization and uses it to create a tile.
//
// SetSerialization is called for two main scenarios.  The first scenario is in the credui case
// where it is prepopulating a tile with credentials that the user chose to store in the OS.
// The second situation is in a remote logon case where the remote client may wish to
// prepopulate a tile with a username, or in some cases, completely populate the tile and
// use it to logon without showing any UI.
//
// If you wish to see an example of SetSerialization, please see either the SampleCredentialProvider
// sample or the SampleCredUICredentialProvider sample.  [The logonUI team says, "The original sample that
// this was built on top of didn't have SetSerialization.  And when we decided SetSerialization was
// important enough to have in the sample, it ended up being a non-trivial amount of work to integrate
// it into the main sample.  We felt it was more important to get these samples out to you quickly than to
// hold them in order to do the work to integrate the SetSerialization changes from SampleCredentialProvider
// into this sample.]
HRESULT CSampleProvider::SetSerialization(
    _In_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION const * /*pcpcs*/)
{
    return E_NOTIMPL;
}

// Called by LogonUI to give you a callback.  Providers often use the callback if they
// some event would cause them to need to change the set of tiles that they enumerated.
HRESULT CSampleProvider::Advise(
    _In_ ICredentialProviderEvents * pcpe,
    _In_ UINT_PTR upAdviseContext)
{
    if (_pCredProvEvents != NULL)
    {
        _pCredProvEvents->Release();
    }
    _pCredProvEvents = pcpe;
    _pCredProvEvents->AddRef();
    _upAdviseContext = upAdviseContext;
    return S_OK;
}

// Called by LogonUI when the ICredentialProviderEvents callback is no longer valid.
HRESULT CSampleProvider::UnAdvise()
{
    if (_pCredProvEvents != NULL)
    {
        _pCredProvEvents->Release();
        _pCredProvEvents = NULL;
    }
    return S_OK;
}

// Called by LogonUI to determine the number of fields in your tiles.  This
// does mean that all your tiles must have the same number of fields.
// This number must include both visible and invisible fields. If you want a tile
// to have different fields from the other tiles you enumerate for a given usage
// scenario you must include them all in this count and then hide/show them as desired
// using the field descriptors.
HRESULT CSampleProvider::GetFieldDescriptorCount(
    _Out_ DWORD *pdwCount)
{
    *pdwCount = SFI_NUM_FIELDS;
    return S_OK;
}

// Gets the field descriptor for a particular field.
HRESULT CSampleProvider::GetFieldDescriptorAt(
    DWORD dwIndex,
    _Outptr_result_nullonfailure_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR **ppcpfd)
{
    HRESULT hr;
    // Verify dwIndex is a valid field.
    if ((dwIndex < _rgCredProvFieldDescriptors.size()) && ppcpfd)
    {
        *ppcpfd = nullptr;
        hr = FieldDescriptorCoAllocCopy(_rgCredProvFieldDescriptors[dwIndex], ppcpfd);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Sets pdwCount to the number of tiles that we wish to show at this time.
// Sets pdwDefault to the index of the tile which should be used as the default.
// The default tile is the tile which will be shown in the zoomed view by default. If
// more than one provider specifies a default the last used cred prov gets to pick
// the default. If *pbAutoLogonWithDefault is TRUE, LogonUI will immediately call
// GetSerialization on the credential you've specified as the default and will submit
// that credential for authentication without showing any further UI.
HRESULT CSampleProvider::GetCredentialCount(
    _Out_ DWORD *pdwCount,
    _Out_ DWORD *pdwDefault,
    _Out_ BOOL *pbAutoLogonWithDefault)
{
    *pbAutoLogonWithDefault = FALSE;
    if (_fRecreateEnumeratedCredentials)
    {
        _fRecreateEnumeratedCredentials = false;
        _ReleaseEnumeratedCredentials();
        _CreateEnumeratedCredentials();
    }

    int idx{};
    for(const auto cred : _pCredentials)
    {
        if(cred->_unlockResult.state == UnlockState::SUCCESS)
        {
            *pdwDefault = idx;
            *pbAutoLogonWithDefault = TRUE;
        }
        idx++;
    }

    *pdwCount = static_cast<DWORD>(_pCredentials.size());
    return S_OK;
}

// Returns the credential at the index specified by dwIndex. This function is called by logonUI to enumerate
// the tiles.
HRESULT CSampleProvider::GetCredentialAt(
    DWORD dwIndex,
    _Outptr_result_nullonfailure_ ICredentialProviderCredential **ppcpc)
{
    HRESULT hr = E_INVALIDARG;
    if(ppcpc == nullptr)
    {
        return hr;
    }
    *ppcpc = nullptr;

    if (dwIndex < _pCredentials.size())
    {
        const auto cred = _pCredentials[dwIndex];
        hr = cred->QueryInterface(IID_ICredentialProviderCredential, reinterpret_cast<void**>(ppcpc));
    }
    return hr;
}

// This function will be called by LogonUI after SetUsageScenario succeeds.
// Sets the User Array with the list of users to be enumerated on the logon screen.
HRESULT CSampleProvider::SetUserArray(_In_ ICredentialProviderUserArray *users)
{
    if (_pCredProviderUserArray)
    {
        _pCredProviderUserArray->Release();
    }
    _pCredProviderUserArray = users;
    _pCredProviderUserArray->AddRef();
    return S_OK;
}

void CSampleProvider::_CreateEnumeratedCredentials()
{
    switch (_cpus)
    {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
    case CPUS_CREDUI:
        {
            _EnumerateCredentials();
            break;
        }
    default:
        break;
    }
}

void CSampleProvider::_ReleaseEnumeratedCredentials()
{
    for(const auto cred : _pCredentials)
        cred->Release();
    _pCredentials.clear();
}

HRESULT CSampleProvider::_EnumerateCredentials()
{
    HRESULT hr = S_OK;
    if (_pCredProviderUserArray != nullptr)
    {
        DWORD dwUserCount;
        _pCredProviderUserArray->GetCount(&dwUserCount);
        if (dwUserCount > 0)
        {
            for (DWORD i = 0; i < dwUserCount; i++)
            {
                ICredentialProviderUser* pCredUser;
                hr = _pCredProviderUserArray->GetAt(i, &pCredUser);
                if (SUCCEEDED(hr))
                {
                    PWSTR userDomain{};
                    hr = pCredUser->GetStringValue(PKEY_Identity_QualifiedUserName, &userDomain);
                    if (SUCCEEDED(hr))
                    {
                        auto userDomainStr = StringUtils::FromWideString(std::wstring(userDomain));
                        auto userDevices = PairedDevicesStorage::GetDevicesForUser(userDomainStr);
                        for(auto userDevice : userDevices)
                        {
                            auto cred = new(std::nothrow) CUnlockCredential();
                            if(cred != nullptr)
                            {
                                hr = cred->Initialize(_cpus, _rgCredProvFieldDescriptors.data(), s_rgFieldStatePairs, pCredUser, this, userDomain);
                                if (FAILED(hr))
                                {
                                    spdlog::error("Failed to initialize credential.");
                                    cred->Release();
                                    cred = nullptr;
                                }
                                else if (SUCCEEDED(hr))
                                {
                                    _pCredentials.emplace_back(cred);
                                }
                            }
                            else
                            {
                                spdlog::error("Failed to allocate credential.");
                                hr = E_OUTOFMEMORY;
                            }
                        }
                    }
                    else
                    {
                        spdlog::error("Failed to get QualifiedUserName.");
                    }
                }
                else
                {
                    spdlog::error("Failed to get user in credProviderUserArray.");
                    hr = E_OUTOFMEMORY;
                }
            }

            if(_pCredentials.empty())
            {
                hr = E_ABORT;
                spdlog::error("Could not find any paired user.");
            }
        }
        else
        {
            hr = E_ABORT;
            spdlog::error("No users found (count=0).");
        }
    }
    else
    {
        hr = E_ABORT;
        spdlog::error("No users found (array=nullptr).");
    }
    return hr;
}

void CSampleProvider::UpdateCredsStatus() const
{
    if (_pCredProvEvents != nullptr)
    {
        _pCredProvEvents->CredentialsChanged(_upAdviseContext);
    }
    else
    {
        spdlog::error("Failed to update credential provider.");
    }
}

// Boilerplate code to create our provider.
HRESULT CSample_CreateInstance(_In_ REFIID riid, _Outptr_ void **ppv)
{
    HRESULT hr;
    if (auto pProvider = new(std::nothrow) CSampleProvider())
    {
        hr = pProvider->QueryInterface(riid, ppv);
        pProvider->Release();
    }
    else
    {
        hr = E_OUTOFMEMORY;
    }
    return hr;
}
