/*
 * Client
 *
 * Copyright (C) 2013 Wen Jia Liu
 *
 * This file is part of UMKCF.
 *
 * UMKCF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UMKCF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UMKCF.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <umkcf.h>

#define HASH_CALLBACK_ID(CallbackId) ((ULONG)(CallbackId))

NTSTATUS KcfpCopyCallbackData(
    __in PKCF_CALLBACK_DATA SourceData,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength,
    __in KPROCESSOR_MODE AccessMode
    );

NTSTATUS KcfpCaptureReturnData(
    __in PKCF_CALLBACK_RETURN_DATA ReturnData,
    __out PKCF_CALLBACK_RETURN_DATA *CapturedReturnData,
    __in KPROCESSOR_MODE AccessMode
    );

VOID KcfpFreeReturnData(
    __in PKCF_CALLBACK_RETURN_DATA CapturedReturnData
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KcfClientInitialization)
#pragma alloc_text(PAGE, KcfClientUninitialization)
#pragma alloc_text(PAGE, KcfCreateClient)
#pragma alloc_text(PAGE, KcfDestroyClient)
#pragma alloc_text(PAGE, KcfCreateCallback)
#pragma alloc_text(PAGE, KcfDestroyCallback)
#pragma alloc_text(PAGE, KcfFindCallback)
#pragma alloc_text(PAGE, KcfPerformCallback)
#pragma alloc_text(PAGE, KcfpCopyCallbackData)
#pragma alloc_text(PAGE, KcfiRemoveCallback)
#pragma alloc_text(PAGE, KcfpCaptureReturnData)
#pragma alloc_text(PAGE, KcfpFreeReturnData)
#pragma alloc_text(PAGE, KcfiReturnCallback)
#endif

NPAGED_LOOKASIDE_LIST KcfCallbackLookasideList;

VOID KcfClientInitialization(
    VOID
    )
{
    PAGED_CODE();

    ExInitializeNPagedLookasideList(&KcfCallbackLookasideList, NULL, NULL, 0, sizeof(KCF_CALLBACK), 'KfcK', 0);
}

VOID KcfClientUninitialization(
    VOID
    )
{
    PAGED_CODE();

    ExDeleteNPagedLookasideList(&KcfCallbackLookasideList);
}

NTSTATUS KcfCreateClient(
    __out PKCF_CLIENT *Client
    )
{
    PKCF_CLIENT client;

    PAGED_CODE();

    client = ExAllocatePoolWithTag(NonPagedPool, sizeof(KCF_CLIENT), 'CfcK');

    if (!client)
        return STATUS_INSUFFICIENT_RESOURCES;

    client->Flags = 0;
    client->LastCallbackId = 0;
    ExInitializeFastMutex(&client->CallbackHashSetLock);
    PhInitializeHashSet(client->CallbackHashSet, PH_HASH_SET_SIZE(client->CallbackHashSet));
    KeInitializeQueue(&client->Queue, 0);

    *Client = client;

    return STATUS_SUCCESS;
}

NTSTATUS KcfDestroyClient(
    __post_invalid PKCF_CLIENT Client
    )
{
    PAGED_CODE();

    ExFreePoolWithTag(Client, 'CfcK');

    return STATUS_SUCCESS;
}

NTSTATUS KcfCreateCallback(
    __out PKCF_CALLBACK *Callback,
    __in PKCF_CLIENT Client,
    __in PKCF_CALLBACK_DATA Data
    )
{
    PKCF_CALLBACK callback;

    PAGED_CODE();

    callback = ExAllocateFromNPagedLookasideList(&KcfCallbackLookasideList);

    if (!callback)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(callback, 0, sizeof(KCF_CALLBACK));
    callback->CallbackId = InterlockedIncrement(&Client->LastCallbackId);
    callback->Client = Client;
    ExInitializeFastMutex(&callback->Lock);
    KeInitializeEvent(&callback->Event, NotificationEvent, FALSE);
    callback->Data = Data;

    ExAcquireFastMutex(&Client->CallbackHashSetLock);
    PhAddEntryHashSet(Client->CallbackHashSet, PH_HASH_SET_SIZE(Client->CallbackHashSet), &callback->HashEntry,
        HASH_CALLBACK_ID(callback->CallbackId));
    ExReleaseFastMutex(&Client->CallbackHashSetLock);

    *Callback = callback;

    return STATUS_SUCCESS;
}

NTSTATUS KcfDestroyCallback(
    __post_invalid PKCF_CALLBACK Callback
    )
{
    PKCF_CLIENT client;

    PAGED_CODE();

    client = Callback->Client;
    ExAcquireFastMutex(&client->CallbackHashSetLock);
    PhRemoveEntryHashSet(client->CallbackHashSet, PH_HASH_SET_SIZE(client->CallbackHashSet), &Callback->HashEntry);
    ExReleaseFastMutex(&client->CallbackHashSetLock);

    ExFreeToNPagedLookasideList(&KcfCallbackLookasideList, Callback);

    return STATUS_SUCCESS;
}

PKCF_CALLBACK KcfFindCallback(
    __in PKCF_CLIENT Client,
    __in KCF_CALLBACK_ID CallbackId
    )
{
    PPH_HASH_ENTRY entry;
    PKCF_CALLBACK callback;

    PAGED_CODE();

    ExAcquireFastMutex(&Client->CallbackHashSetLock);

    entry = PhFindEntryHashSet(
        Client->CallbackHashSet,
        PH_HASH_SET_SIZE(Client->CallbackHashSet),
        HASH_CALLBACK_ID(CallbackId)
        );

    for (; entry; entry = entry->Next)
    {
        callback = CONTAINING_RECORD(entry, KCF_CALLBACK, HashEntry);

        if (callback->CallbackId == CallbackId)
        {
            ExReleaseFastMutex(&Client->CallbackHashSetLock);
            return callback;
        }
    }

    ExReleaseFastMutex(&Client->CallbackHashSetLock);

    return NULL;
}

NTSTATUS KcfPerformCallback(
    __in PKCF_CALLBACK Callback,
    __in KPROCESSOR_MODE WaitMode,
    __in_opt PLARGE_INTEGER Timeout,
    __out PKCF_CALLBACK_RETURN_DATA *ReturnData
    )
{
    NTSTATUS status;
    PKCF_CLIENT client;

    PAGED_CODE();

    client = Callback->Client;
    KeInsertQueue(&client->Queue, &Callback->ListEntry);

    // Wait for a callback return signal.
    status = KeWaitForSingleObject(&Callback->Event, Executive, WaitMode, FALSE, Timeout);

    if (status != STATUS_SUCCESS)
        return status;

    *ReturnData = Callback->ReturnData;

    return STATUS_SUCCESS;
}

FORCEINLINE VOID KcfpCopyUnicodeString(
    __in PUNICODE_STRING SourceString,
    __out PUNICODE_STRING DestinationString,
    __inout PUCHAR *BufferPointer
    )
{
    if (SourceString->Length != 0 && SourceString->Buffer)
    {
        DestinationString->Length = SourceString->Length;
        DestinationString->MaximumLength = SourceString->Length;
        memcpy(*BufferPointer, SourceString->Buffer, SourceString->Length);
        DestinationString->Buffer = (PWSTR)*BufferPointer;
        *BufferPointer += SourceString->Length;
    }
    else
    {
        DestinationString->Length = 0;
        DestinationString->MaximumLength = 0;
        DestinationString->Buffer = NULL;
    }
}

NTSTATUS KcfpCopyCallbackData(
    __in PKCF_CALLBACK_DATA SourceData,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength,
    __in KPROCESSOR_MODE AccessMode
    )
{
    static UNICODE_STRING zeroUnicodeString;

    KCF_CALLBACK_DATA data;
    ULONG returnLength;
    PUCHAR bufferPointer;

    PAGED_CODE();

    data = *SourceData;
    returnLength = sizeof(KCF_CALLBACK_DATA);

    switch (data.EventId.Value)
    {
    case KCF_MAKE_EVENT_ID_VALUE(KCF_CATEGORY_PROCESS, KCF_PROCESS_EVENT_CREATE_PROCESS):
        data.Parameters.CreateProcess.ImageFileName = zeroUnicodeString;
        data.Parameters.CreateProcess.CommandLine = zeroUnicodeString;
        returnLength += SourceData->Parameters.CreateProcess.ImageFileName.Length;
        returnLength += SourceData->Parameters.CreateProcess.CommandLine.Length;
        break;
    }

    if (DataLength < returnLength)
        return STATUS_BUFFER_TOO_SMALL;

    __try
    {
        bufferPointer = (PUCHAR)Data;
        memcpy(bufferPointer, &data, sizeof(KCF_CALLBACK_DATA));
        bufferPointer += sizeof(KCF_CALLBACK_DATA);

        switch (data.EventId.Value)
        {
        case KCF_MAKE_EVENT_ID_VALUE(KCF_CATEGORY_PROCESS, KCF_PROCESS_EVENT_CREATE_PROCESS):
            KcfpCopyUnicodeString(&SourceData->Parameters.CreateProcess.ImageFileName, &Data->Parameters.CreateProcess.ImageFileName, &bufferPointer);
            KcfpCopyUnicodeString(&SourceData->Parameters.CreateProcess.CommandLine, &Data->Parameters.CreateProcess.CommandLine, &bufferPointer);
            break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        NOTHING;
    }

    if (ReturnLength)
    {
        __try
        {
            *ReturnLength = returnLength;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            NOTHING;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS KcfiRemoveCallback(
    __in_opt PLARGE_INTEGER Timeout,
    __out PKCF_CALLBACK_ID CallbackId,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status;
    PLARGE_INTEGER timeout;
    LARGE_INTEGER capturedTimeout;
    PLIST_ENTRY listEntry;
    PKCF_CALLBACK callback;

    PAGED_CODE();

    if (AccessMode != KernelMode)
    {
        __try
        {
            if (Timeout)
            {
                ProbeForRead(Timeout, sizeof(LARGE_INTEGER), sizeof(ULONG));
                capturedTimeout = *Timeout;
                timeout = &capturedTimeout;
            }
            else
            {
                timeout = NULL;
            }

            ProbeForWrite(CallbackId, sizeof(KCF_CALLBACK_ID), sizeof(ULONG));
            ProbeForWrite(Data, DataLength, sizeof(ULONG));

            if (ReturnLength)
                ProbeForWrite(ReturnLength, sizeof(ULONG), sizeof(ULONG));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else
    {
        timeout = Timeout;
    }

    listEntry = KeRemoveQueue(&Client->Queue, AccessMode, timeout);

    if ((LONG_PTR)listEntry == STATUS_TIMEOUT || (LONG_PTR)listEntry == STATUS_USER_APC ||
        (LONG_PTR)listEntry == STATUS_ABANDONED)
    {
        return (NTSTATUS)listEntry;
    }

    callback = CONTAINING_RECORD(listEntry, KCF_CALLBACK, ListEntry);

    status = KcfpCopyCallbackData(callback->Data, Data, DataLength, ReturnLength, AccessMode);

    if (NT_SUCCESS(status))
    {
        __try
        {
            *CallbackId = callback->CallbackId;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            NOTHING;
        }
    }
    else
    {
        KeInsertHeadQueue(&Client->Queue, &callback->ListEntry);
    }

    return status;
}

NTSTATUS KcfpCaptureReturnData(
    __in PKCF_CALLBACK_RETURN_DATA ReturnData,
    __out PKCF_CALLBACK_RETURN_DATA *CapturedReturnData,
    __in KPROCESSOR_MODE AccessMode
    )
{
    PKCF_CALLBACK_RETURN_DATA capturedReturnData;

    PAGED_CODE();

    capturedReturnData = ExAllocatePoolWithTag(PagedPool, sizeof(KCF_CALLBACK_RETURN_DATA), 'RfcK');

    if (!capturedReturnData)
        return STATUS_INSUFFICIENT_RESOURCES;

    memcpy(capturedReturnData, ReturnData, sizeof(KCF_CALLBACK_RETURN_DATA));
    *CapturedReturnData = capturedReturnData;

    return STATUS_SUCCESS;
}

VOID KcfpFreeReturnData(
    __in PKCF_CALLBACK_RETURN_DATA CapturedReturnData
    )
{
    PAGED_CODE();

    ExFreePoolWithTag(CapturedReturnData, 'RfcK');
}

NTSTATUS KcfiReturnCallback(
    __in KCF_CALLBACK_ID CallbackId,
    __in NTSTATUS ReturnStatus,
    __in_opt PKCF_CALLBACK_RETURN_DATA ReturnData,
    __in ULONG ReturnDataLength,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status;
    PKCF_CALLBACK_RETURN_DATA returnData;
    KCF_CALLBACK_RETURN_DATA tempReturnData;
    PKCF_CALLBACK callback;
    PKCF_CALLBACK_RETURN_DATA capturedReturnData;

    PAGED_CODE();

    if (ReturnDataLength < sizeof(KCF_CALLBACK_RETURN_DATA))
        return STATUS_INVALID_PARAMETER_4;

    if (AccessMode != KernelMode)
    {
        __try
        {
            if (ReturnData)
            {
                ProbeForRead(ReturnData, ReturnDataLength, sizeof(ULONG));
                tempReturnData = *ReturnData;
                returnData = &tempReturnData;
            }
            else
            {
                returnData = NULL;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else
    {
        returnData = ReturnData;
    }

    callback = KcfFindCallback(Client, CallbackId);

    if (!callback)
        return STATUS_INVALID_PARAMETER_1;

    if (returnData)
    {
        if (!KcfEqualEventId(returnData->EventId, callback->Data->EventId))
            return STATUS_INVALID_PARAMETER_3;

        status = KcfpCaptureReturnData(returnData, &capturedReturnData, AccessMode);

        if (!NT_SUCCESS(status))
            return status;

        callback->ReturnData = capturedReturnData;
    }

    KeSetEvent(&callback->Event, 0, FALSE);

    return STATUS_SUCCESS;
}
