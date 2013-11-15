#ifndef _UMKCFCL_H
#define _UMKCFCL_H

#include <umkcfapi.h>

#if defined(UMKCFCL_EXPORT)
#define UMKCFCLAPI __declspec(dllexport)
#elif defined(UMKCFCL_IMPORT)
#define UMKCFCLAPI __declspec(dllimport)
#else
#define UMKCFCLAPI
#endif

typedef struct _KCF_PARAMETERS
{
    KCF_SECURITY_LEVEL SecurityLevel;
} KCF_PARAMETERS, *PKCF_PARAMETERS;

UMKCFCLAPI
NTSTATUS
NTAPI
KcfConnect(
    _In_opt_ PWSTR DeviceName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfConnect2(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfConnect2Ex(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKCF_PARAMETERS Parameters
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfDisconnect(
    VOID
    );

UMKCFCLAPI
BOOLEAN
NTAPI
KcfIsConnected(
    VOID
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfSetParameters(
    _In_opt_ PWSTR DeviceName,
    _In_ PKCF_PARAMETERS Parameters
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfInstall(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfInstallEx(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKCF_PARAMETERS Parameters
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfUninstall(
    _In_opt_ PWSTR DeviceName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfQueryVersion(
    _Out_ PULONG Version
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfRemoveCallback(
    __in_opt PLARGE_INTEGER Timeout,
    __out PKCF_CALLBACK_ID CallbackId,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfReturnCallback(
    __in KCF_CALLBACK_ID CallbackId,
    __in NTSTATUS ReturnStatus,
    __in_opt PKCF_CALLBACK_RETURN_DATA ReturnData,
    __in ULONG ReturnDataLength
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfSetFilters(
    __in PKCF_FILTER_DATA Filters,
    __in ULONG NumberOfFilters
    );

#endif
