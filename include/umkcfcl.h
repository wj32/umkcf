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
    __in_opt PWSTR DeviceName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfConnect2(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfConnect2Ex(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName,
    __in_opt PKCF_PARAMETERS Parameters
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
    __in_opt PWSTR DeviceName,
    __in PKCF_PARAMETERS Parameters
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfInstall(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfInstallEx(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName,
    __in_opt PKCF_PARAMETERS Parameters
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfUninstall(
    __in_opt PWSTR DeviceName
    );

UMKCFCLAPI
NTSTATUS
NTAPI
KcfQueryVersion(
    __out PULONG Version
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
