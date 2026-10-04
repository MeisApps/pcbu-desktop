//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// CSampleCredential is our implementation of ICredentialProviderCredential.
// ICredentialProviderCredential is what LogonUI uses to let a credential
// provider specify what a user tile looks like and then tell it what the
// user has entered into the tile.  ICredentialProviderCredential is also
// responsible for packaging up the users credentials into a buffer that
// LogonUI then sends on to LSA.

#pragma once

// clang-format off
#include <atomic>
#include <mutex>
#include <string>
#include <windows.h>
#include <strsafe.h>
#include <shlguid.h>
#include <propkey.h>

#include "common.h"
#include "CUnlockListener.h"
#include "Dll.h"
#include "resource.h"
#include "handler/UnlockHandler.h"
// clang-format on

class CSampleProvider;
class CUnlockCredential : public ICredentialProviderCredential2, ICredentialProviderCredentialWithFieldOptions {
public:
  // IUnknown
  IFACEMETHODIMP_(ULONG) AddRef() {
    return InterlockedIncrement(&_cRef);
  }

  IFACEMETHODIMP_(ULONG) Release() {
    long cRef = InterlockedDecrement(&_cRef);
    if(!cRef) {
      delete this;
    }
    return cRef;
  }

  IFACEMETHODIMP QueryInterface(_In_ REFIID riid, _COM_Outptr_ void **ppv) {
    static const QITAB qit[] = {
        QITABENT(CUnlockCredential, ICredentialProviderCredential),                 // IID_ICredentialProviderCredential
        QITABENT(CUnlockCredential, ICredentialProviderCredential2),                // IID_ICredentialProviderCredential2
        QITABENT(CUnlockCredential, ICredentialProviderCredentialWithFieldOptions), // IID_ICredentialProviderCredentialWithFieldOptions
        {0},
    };
    return QISearch(this, qit, riid, ppv);
  }

public:
  // ICredentialProviderCredential
  IFACEMETHODIMP Advise(_In_ ICredentialProviderCredentialEvents *pcpce);
  IFACEMETHODIMP UnAdvise();

  IFACEMETHODIMP SetSelected(_Out_ BOOL *pbAutoLogon);
  IFACEMETHODIMP SetDeselected();

  IFACEMETHODIMP GetFieldState(DWORD dwFieldID, _Out_ CREDENTIAL_PROVIDER_FIELD_STATE *pcpfs,
                               _Out_ CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE *pcpfis);

  IFACEMETHODIMP GetStringValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ PWSTR *ppwsz);
  IFACEMETHODIMP GetBitmapValue(DWORD dwFieldID, _Outptr_result_nullonfailure_ HBITMAP *phbmp);
  IFACEMETHODIMP GetCheckboxValue(DWORD dwFieldID, _Out_ BOOL *pbChecked, _Outptr_result_nullonfailure_ PWSTR *ppwszLabel);
  IFACEMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, _Out_ DWORD *pcItems, _Deref_out_range_(<, *pcItems) _Out_ DWORD *pdwSelectedItem);
  IFACEMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, _Outptr_result_nullonfailure_ PWSTR *ppwszItem);
  IFACEMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, _Out_ DWORD *pdwAdjacentTo);

  IFACEMETHODIMP SetStringValue(DWORD dwFieldID, _In_ PCWSTR pwz);
  IFACEMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked);
  IFACEMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem);
  IFACEMETHODIMP CommandLinkClicked(DWORD dwFieldID);

  IFACEMETHODIMP GetSerialization(_Out_ CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE *pcpgsr,
                                  _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION *pcpcs, _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                                  _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon);
  IFACEMETHODIMP ReportResult(NTSTATUS ntsStatus, NTSTATUS ntsSubstatus, _Outptr_result_maybenull_ PWSTR *ppwszOptionalStatusText,
                              _Out_ CREDENTIAL_PROVIDER_STATUS_ICON *pcpsiOptionalStatusIcon);

  // ICredentialProviderCredential2
  IFACEMETHODIMP GetUserSid(_Outptr_result_nullonfailure_ PWSTR *ppszSid);

  // ICredentialProviderCredentialWithFieldOptions
  IFACEMETHODIMP GetFieldOptions(DWORD dwFieldID, _Out_ CREDENTIAL_PROVIDER_CREDENTIAL_FIELD_OPTIONS *pcpcfo);

public:
  CUnlockCredential();
  virtual ~CUnlockCredential();
  HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, _In_ CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR const *rgcpfd,
                     _In_ FIELD_STATE_PAIR const *rgfsp, _In_ ICredentialProviderUser *pcpUser, _In_ CSampleProvider *pProvider,
                     _In_ const std::wstring &userDomain);
  void Shutdown();

  uint64_t SetUnlockData(const UnlockResult &result, const std::atomic<bool> *isRunning = nullptr);
  bool IsUnlockSuccess() const;
  bool IsUnlockPending(uint64_t sequence) const;
  void ExpireUnlockSuccess(uint64_t sequence);

  void UpdateProvider();
  void UpdateMessage(const std::string &message);
  void UpdateRetryButton();

  template <typename T> static void SecureErase(std::basic_string<T> &str) {
    SecureZeroMemory(str.data(), str.size() * sizeof(T));
    str.clear();
  }

private:
  void ResetUnlockResult(const UnlockResult &result = {});
  void FinishUnlockSubmission(uint64_t sequence, bool isSubmitted);
  void UpdateStateMessage(UnlockState state);
  static bool IsRetryVisible(UnlockState state);

public:
  long _cRef;
  CREDENTIAL_PROVIDER_USAGE_SCENARIO _cpus;                                           // The usage scenario for which we were enumerated.
  CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR _rgCredProvFieldDescriptors[SFI_NUM_FIELDS]{}; // An array holding the type and name of each field in the tile.
  FIELD_STATE_PAIR _rgFieldStatePairs[SFI_NUM_FIELDS]{};                              // An array holding the state of each field in the tile.
  PWSTR _rgFieldStrings[SFI_NUM_FIELDS]{}; // An array holding the string value of each field. This is different from the name of the field held in
                                           // _rgCredProvFieldDescriptors.
  PWSTR _pszUserSid;
  PWSTR _pszQualifiedUserName;                                      // The user name that's used to pack the authentication buffer
  ICredentialProviderCredentialEvents2 *_pCredProvCredentialEvents; // Used to update fields.
                                                                    // CredentialEvents2 for Begin and EndFieldUpdates.
  BOOL _fChecked;                                                   // Tracks the state of our checkbox.
  DWORD _dwComboIndex;                                              // Tracks the current index of our combobox.
  bool _fShowControls;                                              // Tracks the state of our show/hide controls link.
  bool _fIsLocalUser;                                               // If the cred prov is assosiating with a local user tile
  CSampleProvider *_pCredentialProvider{};
  CUnlockListener *_pUnlockListener{};
  UnlockResult _unlockResult{};
  uint64_t _unlockSequence{};
  bool _isAutoStartBlocked{};
  mutable std::mutex _mutex{};
  std::mutex _providerMutex{};
  std::mutex _listenerMutex{};
};
