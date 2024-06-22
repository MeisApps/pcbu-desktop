#pragma once

#include <atomic>
#include <thread>
#include <string>

#include <WinSock2.h>
#include <credentialprovider.h>

class CSampleProvider;
class CUnlockCredential;
class CUnlockListener
{
public:
	CUnlockListener() = default;
	void Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, CSampleProvider *pCredentialProvider, CUnlockCredential *pCredential, const std::wstring& userDomain);
	void Release();

	void Start();
	void Stop();

	bool HasResponse() const;

private:
	void ListenThread();

	std::thread m_ListenThread{};
	std::atomic<bool> m_IsRunning{};
	bool m_HasResponse{};

	CREDENTIAL_PROVIDER_USAGE_SCENARIO m_ProviderUsage{};
	CSampleProvider* m_CredentialProvider{};
	CUnlockCredential* m_Credential{};
	std::wstring m_UserDomain{};
};
