#ifndef UMKCF_H
#define UMKCF_H

#include <ntifs.h>
#define PHNT_MODE PHNT_MODE_KERNEL
#include <phnt.h>
#include <ntfill.h>
#include <umkcfapi.h>
#include <hashset.h>

// Debugging

#ifdef DBG
#define dprintf(Format, ...) DbgPrint("UMKCF: " Format, __VA_ARGS__)
#else
#define dprintf
#endif

typedef struct _KCF_PARAMETERS
{
    KCF_SECURITY_LEVEL SecurityLevel;
} KCF_PARAMETERS, *PKCF_PARAMETERS;

#define KCF_CLIENT_ENABLE_CALLBACKS 0x1

typedef struct _KCF_CLIENT
{
    ULONG Flags;
    KCF_CALLBACK_ID LastCallbackId;
    FAST_MUTEX CallbackHashSetLock;
    PPH_HASH_ENTRY CallbackHashSet[256];
    KQUEUE Queue;
} KCF_CLIENT, *PKCF_CLIENT;

#define KCF_CALLBACK_STATE_QUEUED 0x1
#define KCF_CALLBACK_STATE_COMPLETED 0x2

typedef struct _KCF_CALLBACK
{
    PH_HASH_ENTRY HashEntry;
    LIST_ENTRY ListEntry; // queue
    KCF_CALLBACK_ID CallbackId;
    PKCF_CLIENT Client;
    ULONG Flags;
    KEVENT Event;
    PKCF_CALLBACK_DATA Data;
    PKCF_CALLBACK_RETURN_DATA *ReturnData;
} KCF_CALLBACK, *PKCF_CALLBACK;

// main

extern KCF_PARAMETERS KcfParameters;

NTSTATUS KcfiQueryVersion(
    __out PULONG Version,
    __in KPROCESSOR_MODE AccessMode
    );

// client

VOID KcfClientInitialization(
    VOID
    );

VOID KcfClientUninitialization(
    VOID
    );

NTSTATUS KcfCreateClient(
    __out PKCF_CLIENT *Client
    );

NTSTATUS KcfDestroyClient(
    __post_invalid PKCF_CLIENT Client
    );

FORCEINLINE VOID KcfInitializeCallbackData(
    __out PKCF_CALLBACK_DATA Data,
    __in ULONGLONG EventId
    )
{
    memset(Data, 0, sizeof(KCF_CALLBACK_DATA));
    Data->EventId = EventId;
    Data->ClientId.UniqueProcess = PsGetCurrentProcessId();
    Data->ClientId.UniqueThread = PsGetCurrentThreadId();
}

NTSTATUS KcfCreateCallback(
    __out PKCF_CALLBACK *Callback,
    __in PKCF_CLIENT Client,
    __in PKCF_CALLBACK_DATA Data
    );

NTSTATUS KcfDestroyCallback(
    __post_invalid PKCF_CALLBACK Callback
    );

PKCF_CALLBACK KcfFindCallback(
    __in PKCF_CLIENT Client,
    __in KCF_CALLBACK_ID CallbackId
    );

NTSTATUS KcfPerformCallback(
    __in PKCF_CALLBACK Callback,
    __out PKCF_CALLBACK_RETURN_DATA *ReturnData
    );

NTSTATUS KcfiRemoveCallback(
    __in_opt PLARGE_INTEGER Timeout,
    __out PKCF_CALLBACK_ID CallbackId,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    );

NTSTATUS KcfiReturnCallback(
    __in KCF_CALLBACK_ID CallbackId,
    __in NTSTATUS ReturnStatus,
    __in_opt PKCF_CALLBACK_RETURN_DATA ReturnData,
    __in ULONG ReturnDataLength,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    );

// devctrl

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL) DRIVER_DISPATCH KcfDispatchDeviceControl;

NTSTATUS KcfDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

// pscall

NTSTATUS KcfPsInitialization(
    VOID
    );

NTSTATUS KcfPsUninitialization(
    VOID
    );

#endif
