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

#define KCF_CLIENT_CANCELLED 0x1
#define KCF_CLIENT_ENABLE_CALLBACKS 0x2

typedef struct _KCF_CLIENT
{
    LONG RefCount;
    LIST_ENTRY ListEntry;

    ULONG Flags;
    KCF_CALLBACK_ID LastCallbackId;

    FAST_MUTEX QueueLock;
    KQUEUE Queue;
    ULONG QueueCount;
    PPH_HASH_ENTRY CallbackHashSet[256];

    FAST_MUTEX FilterListLock;
    LIST_ENTRY FilterListHeads[KCF_CATEGORY_MAXIMUM];
} KCF_CLIENT, *PKCF_CLIENT;

#define KCF_MAXIMUM_QUEUED_CALLBACKS 10000

#define KCF_CALLBACK_STATE_QUEUED 0x1
#define KCF_CALLBACK_STATE_QUEUED_SHIFT 0
#define KCF_CALLBACK_STATE_COMPLETED 0x2
#define KCF_CALLBACK_STATE_COMPLETED_SHIFT 1
#define KCF_CALLBACK_STATE_CANCELLED 0x4
#define KCF_CALLBACK_STATE_CANCELLED_SHIFT 2

typedef struct _KCF_CALLBACK
{
    LONG RefCount;
    PH_HASH_ENTRY HashEntry;
    LIST_ENTRY ListEntry; // queue
    KCF_CALLBACK_ID CallbackId;
    PKCF_CLIENT Client;

    ULONG Flags;
    KEVENT Event;
    PKCF_CALLBACK_DATA Data;
    PKCF_CALLBACK_RETURN_DATA ReturnData;
} KCF_CALLBACK, *PKCF_CALLBACK;

FORCEINLINE VOID KcfInitializeCallbackData(
    __out PKCF_CALLBACK_DATA Data,
    __in KCF_EVENT_ID EventId
    )
{
    memset(Data, 0, sizeof(KCF_CALLBACK_DATA));
    Data->EventId = EventId;
    Data->ClientId.UniqueProcess = PsGetCurrentProcessId();
    Data->ClientId.UniqueThread = PsGetCurrentThreadId();
    KeQuerySystemTime(&Data->TimeStamp);
}

// main

extern KCF_PARAMETERS KcfParameters;

extern FAST_MUTEX KcfClientListLock;
extern LIST_ENTRY KcfClientListHead;

NTSTATUS KcfiQueryVersion(
    __out PULONG Version,
    __in KPROCESSOR_MODE AccessMode
    );

ULONG_PTR KcfFindUnicodeStringInUnicodeString(
    __in PUNICODE_STRING String1,
    __in PUNICODE_STRING String2,
    __in BOOLEAN IgnoreCase
    );

FORCEINLINE BOOLEAN KcfSuffixUnicodeString(
    __in PUNICODE_STRING String1,
    __in PUNICODE_STRING String2,
    __in BOOLEAN IgnoreCase
    )
{
    UNICODE_STRING us1;

    if (String2->Length > String1->Length)
        return FALSE;

    us1.Buffer = (PWSTR)((PCHAR)String1->Buffer + String1->Length - String2->Length);
    us1.Length = String2->Length;
    us1.MaximumLength = String2->Length;

    return RtlEqualUnicodeString(&us1, String2, IgnoreCase);
}

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

VOID KcfCancelClient(
    __in PKCF_CLIENT Client
    );

VOID KcfReferenceClient(
    __in PKCF_CLIENT Client
    );

VOID KcfDereferenceClient(
    __in PKCF_CLIENT Client
    );

NTSTATUS KcfCreateCallback(
    __out PKCF_CALLBACK *Callback,
    __in PKCF_CLIENT Client,
    __in PKCF_CALLBACK_DATA Data
    );

VOID KcfReferenceCallback(
    __in PKCF_CALLBACK Callback
    );

VOID KcfDereferenceCallback(
    __in PKCF_CALLBACK Callback
    );

PKCF_CALLBACK KcfFindCallback(
    __in PKCF_CLIENT Client,
    __in KCF_CALLBACK_ID CallbackId
    );

NTSTATUS KcfPerformCallback(
    __in PKCF_CALLBACK Callback,
    __in KPROCESSOR_MODE WaitMode,
    __in_opt PLARGE_INTEGER Timeout,
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

VOID KcfFreeReturnData(
    __in PKCF_CALLBACK_RETURN_DATA ReturnData
    );

NTSTATUS KcfiReturnCallback(
    __in KCF_CALLBACK_ID CallbackId,
    __in NTSTATUS ReturnStatus,
    __in_opt PKCF_CALLBACK_RETURN_DATA ReturnData,
    __in ULONG ReturnDataLength,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    );

// filter

#define KCF_MAXIMUM_CLIENTS 32

typedef struct _KCF_FILTER
{
    LIST_ENTRY ListEntry;
    PKCF_CLIENT Client;
    KCF_FILTER_DATA Data;
} KCF_FILTER, *PKCF_FILTER;

VOID KcfFilterInitialization(
    VOID
    );

VOID KcfDeleteDataItem(
    __in PKCF_DATA_ITEM DataItem
    );

VOID KcfDeleteFilterData(
    __in PKCF_FILTER_DATA FilterData
    );

NTSTATUS KcfiSetFilters(
    __in PKCF_FILTER_DATA Filters,
    __in ULONG NumberOfFilters,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    );

BOOLEAN KcfGetClientsForCallback(
    __out PKCF_CLIENT *Clients,
    __in ULONG MaximumClients,
    __out PULONG NumberOfClients,
    __in PKCF_CALLBACK_DATA Data,
    __in_opt PKCF_DATA_ITEM CustomValues,
    __in ULONG NumberOfCustomValues
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
