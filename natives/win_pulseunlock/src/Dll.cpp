//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Standard dll required functions and class factory implementation.

// clang-format off
#include <windows.h>
#include <unknwn.h>
#include "Dll.h"
#include "helpers.h"
// clang-format on

#include <mutex>
#include <new>

#include "storage/LoggingSystem.h"

static long g_cRef = 0;   // global dll reference count
static std::mutex g_RefMutex{};
HINSTANCE g_hinst = NULL; // global dll hinstance

extern HRESULT CSample_CreateInstance(__in REFIID riid, __deref_out void **ppv);
EXTERN_C GUID CLSID_CSample;

class CClassFactory : public IClassFactory {
public:
  CClassFactory() : _cRef(1) {
    DllAddRef();
  }

  // IUnknown
  IFACEMETHODIMP QueryInterface(__in REFIID riid, __deref_out void **ppv) {
    static const QITAB qit[] = {
        QITABENT(CClassFactory, IClassFactory),
        {0},
    };
    return QISearch(this, qit, riid, ppv);
  }

  IFACEMETHODIMP_(ULONG) AddRef() {
    return InterlockedIncrement(&_cRef);
  }

  IFACEMETHODIMP_(ULONG) Release() {
    long cRef = InterlockedDecrement(&_cRef);
    if(!cRef)
      delete this;
    return cRef;
  }

  // IClassFactory
  IFACEMETHODIMP CreateInstance(__in IUnknown *pUnkOuter, __in REFIID riid, __deref_out void **ppv) {
    HRESULT hr;
    if(!pUnkOuter) {
      hr = CSample_CreateInstance(riid, ppv);
    } else {
      *ppv = NULL;
      hr = CLASS_E_NOAGGREGATION;
    }
    return hr;
  }

  IFACEMETHODIMP LockServer(__in BOOL bLock) {
    if(bLock) {
      DllAddRef();
    } else {
      DllRelease();
    }
    return S_OK;
  }

private:
  ~CClassFactory() {
    DllRelease();
  }
  long _cRef;
};

HRESULT CClassFactory_CreateInstance(__in REFCLSID rclsid, __in REFIID riid, __deref_out void **ppv) {
  *ppv = NULL;

  HRESULT hr;

  if(CLSID_CSample == rclsid) {
    CClassFactory *pcf = new(std::nothrow) CClassFactory();
    if(pcf) {
      hr = pcf->QueryInterface(riid, ppv);
      pcf->Release();
    } else {
      hr = E_OUTOFMEMORY;
    }
  } else {
    hr = CLASS_E_CLASSNOTAVAILABLE;
  }
  return hr;
}

void DllAddRef() {
  std::lock_guard lock(g_RefMutex);
  if(InterlockedIncrement(&g_cRef) == 1) {
    try {
      LoggingSystem::Init("module");
    } catch(...) {
    }
  }
}

void DllRelease() {
  std::lock_guard lock(g_RefMutex);
  if(InterlockedDecrement(&g_cRef) == 0) {
    try {
      LoggingSystem::Destroy();
    } catch(...) {
    }
  }
}

STDAPI DllCanUnloadNow() {
  return (g_cRef > 0) ? S_FALSE : S_OK;
}

STDAPI DllGetClassObject(__in REFCLSID rclsid, __in REFIID riid, __deref_out void **ppv) {
  return CClassFactory_CreateInstance(rclsid, riid, ppv);
}

STDAPI_(BOOL) DllMain(__in HINSTANCE hinstDll, __in DWORD dwReason, __in void *) {
  switch(dwReason) {
    case DLL_PROCESS_ATTACH:
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
  }

  g_hinst = hinstDll;
  return TRUE;
}
