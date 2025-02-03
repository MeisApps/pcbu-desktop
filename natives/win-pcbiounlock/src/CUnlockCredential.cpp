//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//

#include "CUnlockCredential.h"

#include <unknwn.h>
#include <ShlObj_core.h>
#include "guid.h"
#include "utils/StringUtils.h"

CUnlockCredential::CUnlockCredential():
    _cRef(1),
    _pCredProvCredentialEvents(nullptr),
    _pszUserSid(nullptr),
    _pszQualifiedUserName(nullptr),
    _fIsLocalUser(false),
    _fChecked(false),
    _fShowControls(false),
    _dwComboIndex(0),
    _cpus(CPUS_INVALID),
    _unlockResult(UnlockState::UNKNOWN)
{
    DllAddRef();

    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs, sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgFieldStrings, sizeof(_rgFieldStrings));
}

CUnlockCredential::~CUnlockCredential()
{
    if(_pUnlockListener != nullptr)
    {
        _pUnlockListener->Release();
        _pUnlockListener = nullptr;
    }
    if (_rgFieldStrings[SFI_PASSWORD])
    {
        size_t lenPassword = wcslen(_rgFieldStrings[SFI_PASSWORD]);
        SecureZeroMemory(_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*_rgFieldStrings[SFI_PASSWORD]));
    }
    for (int i = 0; i < ARRAYSIZE(_rgFieldStrings); i++)
    {
        CoTaskMemFree(_rgFieldStrings[i]);
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
    }
    CoTaskMemFree(_pszUserSid);
    CoTaskMemFree(_pszQualifiedUserName);
    DllRelease();
}

// Initializes one credential with the field information passed in.
// Set the value of the SFI_LARGE_TEXT field to pwzUsername.
HRESULT CUnlockCredential::Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                                      _In_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const *rgcpfd,
                                      _In_ FIELD_STATE_PAIR const *rgfsp,
                                      _In_ ICredentialProviderUser *pcpUser,
                                      _In_ CSampleProvider *pProvider,
                                      _In_ const std::wstring& userDomain)
{
    HRESULT hr = S_OK;
    _cpus = cpus;
    _pCredentialProvider = pProvider;

    GUID guidProvider;
    pcpUser->GetProviderID(&guidProvider);
    _fIsLocalUser = (guidProvider == Identity_LocalUserProvider);

    // Copy the field descriptors for each field. This is useful if you want to vary the field
    // descriptors based on what Usage scenario the credential was created for.
    for (DWORD i = 0; SUCCEEDED(hr) && i < ARRAYSIZE(_rgCredProvFieldDescriptors); i++)
    {
        _rgFieldStatePairs[i] = rgfsp[i];
        hr = FieldDescriptorCopy(rgcpfd[i], &_rgCredProvFieldDescriptors[i]);
    }

    // Initialize the String value of all the fields.
    if (SUCCEEDED(hr))
    {
        hr = pcpUser->GetStringValue(PKEY_Identity_QualifiedUserName, &_pszQualifiedUserName);
    }
    if (SUCCEEDED(hr))
    {
        hr = pcpUser->GetSid(&_pszUserSid);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(_pszQualifiedUserName, &_rgFieldStrings[SFI_USERNAME]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(StringUtils::ToWideString(I18n::Get("initializing")).c_str(), &_rgFieldStrings[SFI_MESSAGE]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);
    }
    if (SUCCEEDED(hr))
    {
        hr = SHStrDupW(L"Submit", &_rgFieldStrings[SFI_SUBMIT_BUTTON]);
    }
    if(SUCCEEDED(hr))
    {
        _pUnlockListener = new(std::nothrow) CUnlockListener();
        if(_pUnlockListener != nullptr)
        {
            _pUnlockListener->Initialize(_cpus, _pCredentialProvider, this, userDomain);
        }
        else
        {
            hr = E_OUTOFMEMORY;
        }
    }

    return hr;
}

bool CUnlockCredential::IsSelected() const
{
    return _isSelected;
}

void CUnlockCredential::SetUnlockData(const UnlockResult& result)
{
    _unlockResult = result;
    UpdateMessage(UnlockStateUtils::ToString(result.state));
}

void CUnlockCredential::UpdateMessage(const std::string& message)
{
    if(_rgFieldStrings[SFI_MESSAGE])
        CoTaskMemFree(_rgFieldStrings[SFI_MESSAGE]);
    SHStrDupW(StringUtils::ToWideString(message).c_str(), &_rgFieldStrings[SFI_MESSAGE]);
    if (_pCredProvCredentialEvents)
        _pCredProvCredentialEvents->SetFieldString(this, SFI_MESSAGE, _rgFieldStrings[SFI_MESSAGE]);
}

// LogonUI calls this in order to give us a callback in case we need to notify it of anything.
HRESULT CUnlockCredential::Advise(_In_ ICredentialProviderCredentialEvents *pcpce)
{
    if (_pCredProvCredentialEvents != nullptr)
    {
        _pCredProvCredentialEvents->Release();
    }

    auto result = pcpce->QueryInterface(IID_PPV_ARGS(&_pCredProvCredentialEvents));
    if (_pCredProvCredentialEvents)
        _pCredProvCredentialEvents->SetFieldString(this, SFI_MESSAGE, _rgFieldStrings[SFI_MESSAGE]);
    return result;
}

// LogonUI calls this to tell us to release the callback.
HRESULT CUnlockCredential::UnAdvise()
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
    }
    _pCredProvCredentialEvents = nullptr;
    return S_OK;
}

// LogonUI calls this function when our tile is selected (zoomed)
// If you simply want fields to show/hide based on the selected state,
// there's no need to do anything here - you can set that up in the
// field definitions. But if you want to do something
// more complicated, like change the contents of a field when the tile is
// selected, you would do it here.
HRESULT CUnlockCredential::SetSelected(_Out_ BOOL *pbAutoLogon)
{
    _isSelected = true;
    if(_pUnlockListener != nullptr)
    {
        _pUnlockListener->Start();
    }
    *pbAutoLogon = _unlockResult.state == UnlockState::SUCCESS;
    return S_OK;
}

// Similarly to SetSelected, LogonUI calls this when your tile was selected
// and now no longer is. The most common thing to do here (which we do below)
// is to clear out the password field.
HRESULT CUnlockCredential::SetDeselected()
{
    _isSelected = false;
    if(_pUnlockListener != nullptr)
    {
        _pUnlockListener->Stop();
    }
    HRESULT hr = S_OK;
    if (_rgFieldStrings[SFI_PASSWORD])
    {
        size_t lenPassword = wcslen(_rgFieldStrings[SFI_PASSWORD]);
        SecureZeroMemory(_rgFieldStrings[SFI_PASSWORD], lenPassword * sizeof(*_rgFieldStrings[SFI_PASSWORD]));

        CoTaskMemFree(_rgFieldStrings[SFI_PASSWORD]);
        hr = SHStrDupW(L"", &_rgFieldStrings[SFI_PASSWORD]);

        if (SUCCEEDED(hr) && _pCredProvCredentialEvents)
        {
            _pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, _rgFieldStrings[SFI_PASSWORD]);
        }
    }
    return hr;
}

// Get info for a particular field of a tile. Called by logonUI to get information
// to display the tile.
HRESULT CUnlockCredential::GetFieldState(DWORD dwFieldID,
                                         _Out_ CREDENTIAL_PROVIDER_FIELD_STATE *pcpfs,
                                         _Out_ CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE *pcpfis)
{
    HRESULT hr;
    // Validate our parameters.
    if ((dwFieldID < ARRAYSIZE(_rgFieldStatePairs)))
    {
        if(dwFieldID == SFI_USERNAME && _cpus != CPUS_CREDUI) // Show username only in CredUI
        {
            *pcpfs = CPFS_HIDDEN;
            *pcpfis = CPFIS_NONE;
        }
        else
        {
            *pcpfs = _rgFieldStatePairs[dwFieldID].cpfs;
            *pcpfis = _rgFieldStatePairs[dwFieldID].cpfis;
        }
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

// Sets ppwsz to the string value of the field at the index dwFieldID
HRESULT CUnlockCredential::GetStringValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ PWSTR *ppwsz)
{
    HRESULT hr;
    *ppwsz = nullptr;

    // Check to make sure dwFieldID is a legitimate index
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors))
    {
        // Make a copy of the string and return that. The caller
        // is responsible for freeing it.
        hr = SHStrDupW(_rgFieldStrings[dwFieldID], ppwsz);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Get the image to show in the user tile
HRESULT CUnlockCredential::GetBitmapValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ HBITMAP *phbmp)
{
    HRESULT hr;
    *phbmp = nullptr;

    if ((SFI_TILEIMAGE == dwFieldID))
    {
        HBITMAP hbmp = LoadBitmapW(g_hinst, MAKEINTRESOURCE(IDB_TILE_IMAGE));
        if (hbmp != nullptr)
        {
            hr = S_OK;
            *phbmp = hbmp;
        }
        else
        {
            wchar_t szPath[MAX_PATH]{};
            hr = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, szPath);
            if(hr == S_OK)
            {
                if(PathAppendW(szPath, L"Microsoft\\User Account Pictures\\user.bmp"))
                {
                    hbmp = (HBITMAP)LoadImageW(nullptr, szPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
                    if(hbmp != nullptr)
                    {
                        hr = S_OK;
                        *phbmp = hbmp;
                    }
                    else
                    {
                        hr = HRESULT_FROM_WIN32(GetLastError());
                    }
                }
                else
                {
                    hr = E_INVALIDARG;
                }
            }
        }
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Sets pdwAdjacentTo to the index of the field the submit button should be
// adjacent to. We recommend that the submit button is placed next to the last
// field which the user is required to enter information in. Optional fields
// should be below the submit button.
HRESULT CUnlockCredential::GetSubmitButtonValue(DWORD dwFieldID, _Out_ DWORD *pdwAdjacentTo)
{
    HRESULT hr;

    if (SFI_SUBMIT_BUTTON == dwFieldID)
    {
        // pdwAdjacentTo is a pointer to the fieldID you want the submit button to
        // appear next to.
        *pdwAdjacentTo = SFI_PASSWORD;
        hr = S_OK;
    }
    else
    {
        hr = E_INVALIDARG;
    }
    return hr;
}

// Sets the value of a field which can accept a string as a value.
// This is called on each keystroke when a user types into an edit field
HRESULT CUnlockCredential::SetStringValue(DWORD dwFieldID, _In_ PCWSTR pwz)
{
    HRESULT hr;

    // Validate parameters.
    if (dwFieldID < ARRAYSIZE(_rgCredProvFieldDescriptors) &&
        (CPFT_EDIT_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft ||
        CPFT_PASSWORD_TEXT == _rgCredProvFieldDescriptors[dwFieldID].cpft))
    {
        PWSTR *ppwszStored = &_rgFieldStrings[dwFieldID];
        CoTaskMemFree(*ppwszStored);
        hr = SHStrDupW(pwz, ppwszStored);
    }
    else
    {
        hr = E_INVALIDARG;
    }

    return hr;
}

// Our credential doesn't have any checkable boxes.
HRESULT CUnlockCredential::GetCheckboxValue(
    __in DWORD dwFieldID,
    __out BOOL* pbChecked,
    __deref_out PWSTR* ppwszLabel
)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pbChecked);
    UNREFERENCED_PARAMETER(ppwszLabel);
    return E_NOTIMPL;
}

// Our credential doesn't have a checkbox.
HRESULT CUnlockCredential::SetCheckboxValue(
    __in DWORD dwFieldID,
    __in BOOL bChecked
)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(bChecked);
    return E_NOTIMPL;
}

// Our credential doesn't have a combobox.
HRESULT CUnlockCredential::GetComboBoxValueCount(
    DWORD dwFieldID, _Out_ DWORD* pcItems, _Deref_out_range_(< , *pcItems) _Out_ DWORD* pdwSelectedItem
)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(pcItems);
    UNREFERENCED_PARAMETER(pdwSelectedItem);
    return E_NOTIMPL;
}

// Our credential doesn't have a combobox.
HRESULT CUnlockCredential::GetComboBoxValueAt(
    __in DWORD dwFieldID,
    __out DWORD dwItem,
    __deref_out PWSTR* ppwszItem
)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    UNREFERENCED_PARAMETER(dwItem);
    UNREFERENCED_PARAMETER(ppwszItem);
    return E_NOTIMPL;
}

// Our credential doesn't have a combobox.
HRESULT CUnlockCredential::SetComboBoxSelectedValue(
    __in DWORD dwFieldId,
    __in DWORD dwSelectedItem
)
{
    UNREFERENCED_PARAMETER(dwFieldId);
    UNREFERENCED_PARAMETER(dwSelectedItem);
    return E_NOTIMPL;
}


// Our credential doesn't have a command link.
HRESULT CUnlockCredential::CommandLinkClicked(__in DWORD dwFieldID)
{
    UNREFERENCED_PARAMETER(dwFieldID);
    return E_NOTIMPL;
}

// Collect the username and password into a serialized credential for the correct usage scenario
// (logon/unlock is what's demonstrated in this sample).  LogonUI then passes these credentials
// back to the system to log on.
HRESULT CUnlockCredential::GetSerialization(_Out_ CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpgsr,
                                            _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpcs,
                                            _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                                            _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
    HRESULT hr = E_UNEXPECTED;
    *pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;
    ZeroMemory(pcpcs, sizeof(*pcpcs));

    // Check for password or unlock success
    std::wstring pwd;
    if (_unlockResult.state == UnlockState::SUCCESS)
    {
        pwd = StringUtils::ToWideString(_unlockResult.password);
    }
    else if (_rgFieldStrings[SFI_PASSWORD] && lstrlenW(_rgFieldStrings[SFI_PASSWORD]) >= 1)
    {
        pwd = std::wstring(_rgFieldStrings[SFI_PASSWORD]);
    }
    else
    {
        return E_ABORT;
    }

    // For local user, the domain and user name can be split from _pszQualifiedUserName (domain\username).
    // CredPackAuthenticationBuffer() cannot be used because it won't work with unlock scenario.
    if (_fIsLocalUser)
    {
        PWSTR pwzProtectedPassword;
        hr = ProtectIfNecessaryAndCopyPassword(pwd.c_str(), _cpus, &pwzProtectedPassword);
        if (SUCCEEDED(hr))
        {
            PWSTR pszDomain;
            PWSTR pszUsername;
            hr = SplitDomainAndUsername(_pszQualifiedUserName, &pszDomain, &pszUsername);
            if (SUCCEEDED(hr))
            {
                KERB_INTERACTIVE_UNLOCK_LOGON kiul;
                hr = KerbInteractiveUnlockLogonInit(pszDomain, pszUsername, pwzProtectedPassword, _cpus, &kiul);
                if (SUCCEEDED(hr))
                {
                    // We use KERB_INTERACTIVE_UNLOCK_LOGON in both unlock and logon scenarios.  It contains a
                    // KERB_INTERACTIVE_LOGON to hold the creds plus a LUID that is filled in for us by Winlogon
                    // as necessary.
                    hr = KerbInteractiveUnlockLogonPack(kiul, &pcpcs->rgbSerialization, &pcpcs->cbSerialization);
                    if (SUCCEEDED(hr))
                    {
                        ULONG ulAuthPackage;
                        hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                        if (SUCCEEDED(hr))
                        {
                            pcpcs->ulAuthenticationPackage = ulAuthPackage;
                            pcpcs->clsidCredentialProvider = CLSID_CSample;
                            // At this point the credential has created the serialized credential used for logon
                            // By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
                            // that we have all the information we need and it should attempt to submit the
                            // serialized credential.
                            *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
                        }
                    }
                }
                CoTaskMemFree(pszDomain);
                CoTaskMemFree(pszUsername);
            }
            CoTaskMemFree(pwzProtectedPassword);
        }
    }
    else
    {
        DWORD dwAuthFlags = CRED_PACK_PROTECTED_CREDENTIALS | CRED_PACK_ID_PROVIDER_CREDENTIALS;

        // First get the size of the authentication buffer to allocate
        if (!CredPackAuthenticationBufferW(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(pwd.c_str()), nullptr, &pcpcs->cbSerialization) &&
            (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            pcpcs->rgbSerialization = static_cast<byte *>(CoTaskMemAlloc(pcpcs->cbSerialization));
            if (pcpcs->rgbSerialization != nullptr)
            {
                hr = S_OK;

                // Retrieve the authentication buffer
                if (CredPackAuthenticationBufferW(dwAuthFlags, _pszQualifiedUserName, const_cast<PWSTR>(pwd.c_str()), pcpcs->rgbSerialization, &pcpcs->cbSerialization))
                {
                    ULONG ulAuthPackage;
                    hr = RetrieveNegotiateAuthPackage(&ulAuthPackage);
                    if (SUCCEEDED(hr))
                    {
                        pcpcs->ulAuthenticationPackage = ulAuthPackage;
                        pcpcs->clsidCredentialProvider = CLSID_CSample;

                        // At this point the credential has created the serialized credential used for logon
                        // By setting this to CPGSR_RETURN_CREDENTIAL_FINISHED we are letting logonUI know
                        // that we have all the information we need and it should attempt to submit the
                        // serialized credential.
                        *pcpgsr = CPGSR_RETURN_CREDENTIAL_FINISHED;
                    }
                }
                else
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    if (SUCCEEDED(hr))
                    {
                        hr = E_FAIL;
                    }
                }

                if (FAILED(hr))
                {
                    CoTaskMemFree(pcpcs->rgbSerialization);
                }
            }
            else
            {
                hr = E_OUTOFMEMORY;
            }
        }
    }
    return hr;
}

struct REPORT_RESULT_STATUS_INFO
{
    NTSTATUS ntsStatus;
    NTSTATUS ntsSubstatus;
    std::wstring    pwzMessage;
    CREDENTIAL_PROVIDER_STATUS_ICON cpsi;
};

// ReportResult is completely optional.  Its purpose is to allow a credential to customize the string
// and the icon displayed in the case of a logon failure.  For example, we have chosen to
// customize the error shown in the case of bad username/password and in the case of the account
// being disabled.
HRESULT CUnlockCredential::ReportResult(NTSTATUS ntsStatus,
                                        NTSTATUS ntsSubstatus,
                                        _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                                        _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    REPORT_RESULT_STATUS_INFO rgLogonStatusInfo[] =
    {
        { STATUS_LOGON_FAILURE, 0, StringUtils::ToWideString(I18n::Get("error_password")), CPSI_ERROR, },
        { STATUS_ACCOUNT_RESTRICTION, STATUS_ACCOUNT_DISABLED, StringUtils::ToWideString(I18n::Get("error_account_disabled")), CPSI_WARNING },
    };
    _unlockResult = UnlockResult(UnlockState::UNKNOWN);
    DWORD dwStatusInfo = (DWORD)-1;

    // Look for a match on status and substatus.
    for (DWORD i = 0; i < ARRAYSIZE(rgLogonStatusInfo); i++)
    {
        if (rgLogonStatusInfo[i].ntsStatus == ntsStatus && rgLogonStatusInfo[i].ntsSubstatus == ntsSubstatus)
        {
            dwStatusInfo = i;
            break;
        }
    }

    if ((DWORD)-1 != dwStatusInfo)
    {
        if (SUCCEEDED(SHStrDupW(rgLogonStatusInfo[dwStatusInfo].pwzMessage.c_str(), ppwszOptionalStatusText)))
        {
            *pcpsiOptionalStatusIcon = rgLogonStatusInfo[dwStatusInfo].cpsi;
        }
    }

    // If we failed the logon, try to erase the password field.
    if (FAILED(HRESULT_FROM_NT(ntsStatus)))
    {
        if (_pCredProvCredentialEvents)
        {
            _pCredProvCredentialEvents->SetFieldString(this, SFI_PASSWORD, L"");
        }
    }

    // Since nullptr is a valid value for *ppwszOptionalStatusText and *pcpsiOptionalStatusIcon
    // this function can't fail.
    return S_OK;
}

// Gets the SID of the user corresponding to the credential.
HRESULT CUnlockCredential::GetUserSid(_Outptr_result_nullonfailure_ PWSTR *ppszSid)
{
    *ppszSid = nullptr;
    HRESULT hr = E_UNEXPECTED;
    if (_pszUserSid != nullptr)
    {
        hr = SHStrDupW(_pszUserSid, ppszSid);
    }
    // Return S_FALSE with a null SID in ppszSid for the
    // credential to be associated with an empty user tile.

    return hr;
}

// GetFieldOptions to enable the password reveal button and touch keyboard auto-invoke in the password field.
HRESULT CUnlockCredential::GetFieldOptions(DWORD dwFieldID,
                                           _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_FIELD_OPTIONS *pcpcfo)
{
    *pcpcfo = CPCFO_NONE;

    if (dwFieldID == SFI_PASSWORD)
    {
        *pcpcfo = CPCFO_ENABLE_PASSWORD_REVEAL;
    }
    else if (dwFieldID == SFI_TILEIMAGE)
    {
        *pcpcfo = CPCFO_ENABLE_TOUCH_KEYBOARD_AUTO_INVOKE;
    }

    return S_OK;
}
