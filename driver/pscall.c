/*
 * Process manager callback handlers
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

VOID KcfpCreateProcessNotifyRoutineEx(
    __inout PEPROCESS Process,
    __in HANDLE ProcessId,
    __in_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    );

VOID KcfpCreateThreadNotifyRoutine(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
    __in BOOLEAN Create
    );

VOID KcfpLoadImageNotifyRoutine(
    __in_opt PUNICODE_STRING FullImageName,
    __in HANDLE ProcessId,
    __in PIMAGE_INFO ImageInfo
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KcfPsInitialization)
#pragma alloc_text(PAGE, KcfPsUninitialization)
#pragma alloc_text(PAGE, KcfpCreateProcessNotifyRoutineEx)
#pragma alloc_text(PAGE, KcfpCreateThreadNotifyRoutine)
#pragma alloc_text(PAGE, KcfpLoadImageNotifyRoutine)
#endif

BOOLEAN KcfCreateProcessNotifyRoutineExSet;
BOOLEAN KcfCreateThreadNotifyRoutineSet;
BOOLEAN KcfLoadImageNotifyRoutineSet;

NTSTATUS KcfPsInitialization(
    VOID
    )
{
    NTSTATUS status;

    PAGED_CODE();

    status = PsSetCreateProcessNotifyRoutineEx(KcfpCreateProcessNotifyRoutineEx, FALSE);

    if (!NT_SUCCESS(status))
        return status;

    KcfCreateProcessNotifyRoutineExSet = TRUE;

    status = PsSetCreateThreadNotifyRoutine(KcfpCreateThreadNotifyRoutine);

    if (!NT_SUCCESS(status))
        return status;

    KcfCreateThreadNotifyRoutineSet = TRUE;

    status = PsSetLoadImageNotifyRoutine(KcfpLoadImageNotifyRoutine);

    if (!NT_SUCCESS(status))
        return status;

    KcfLoadImageNotifyRoutineSet = TRUE;

    return STATUS_SUCCESS;
}

NTSTATUS KcfPsUninitialization(
    VOID
    )
{
    PAGED_CODE();

    if (KcfCreateProcessNotifyRoutineExSet)
        PsSetCreateProcessNotifyRoutineEx(KcfpCreateProcessNotifyRoutineEx, TRUE);
    if (KcfCreateThreadNotifyRoutineSet)
        PsRemoveCreateThreadNotifyRoutine(KcfpCreateThreadNotifyRoutine);
    if (KcfLoadImageNotifyRoutineSet)
        PsRemoveLoadImageNotifyRoutine(KcfpLoadImageNotifyRoutine);

    return STATUS_SUCCESS;
}

VOID KcfpCreateProcessNotifyRoutineEx(
    __inout PEPROCESS Process,
    __in HANDLE ProcessId,
    __in_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    NTSTATUS status;
    KCF_CALLBACK_DATA data;
    PKCF_CLIENT clients[KCF_MAXIMUM_CLIENTS];
    ULONG numberOfClients;
    ULONG i;

    PAGED_CODE();

    KcfInitializeCallbackData(&data, KcfMakeEventId(KCF_CATEGORY_PROCESS, CreateInfo ? KCF_PROCESS_EVENT_PROCESS_CREATE : KCF_PROCESS_EVENT_PROCESS_EXIT));
    data.Parameters.ProcessCreate.ProcessId = ProcessId;

    if (CreateInfo)
    {
        dprintf("KcfpCreateProcessNotifyRoutineEx: Create PID %u: %.*S\n", (ULONG)ProcessId, CreateInfo->ImageFileName->Length / 2, CreateInfo->ImageFileName->Buffer);

        data.Parameters.ProcessCreate.ParentProcessId = CreateInfo->ParentProcessId;
        data.Parameters.ProcessCreate.CreatingThreadId = CreateInfo->CreatingThreadId;
        data.Parameters.ProcessCreate.ImageFileName = *CreateInfo->ImageFileName;
        if (CreateInfo->CommandLine)
            data.Parameters.ProcessCreate.CommandLine = *CreateInfo->CommandLine;
        data.Parameters.ProcessCreate.FileOpenNameAvailable = CreateInfo->FileOpenNameAvailable;
    }
    else
    {
        dprintf("KcfpCreateProcessNotifyRoutineEx: Exit PID %u\n", (ULONG)ProcessId);
    }

    if (KcfGetClientsForCallback(clients, KCF_MAXIMUM_CLIENTS, &numberOfClients, &data, NULL, 0))
    {
        dprintf("KcfpCreateProcessNotifyRoutineEx: %u clients\n", numberOfClients);

        for (i = 0; i < numberOfClients; i++)
        {
            PKCF_CLIENT client;
            PKCF_CALLBACK callback;
            PKCF_CALLBACK_RETURN_DATA returnData;

            client = clients[i];

            if (NT_SUCCESS(KcfCreateCallback(&callback, client, &data)))
            {
                status = KcfPerformCallback(callback, ExGetPreviousMode(), NULL, &returnData);

                if (NT_SUCCESS(status) && returnData)
                {
                    if (CreateInfo)
                        CreateInfo->CreationStatus = returnData->Parameters.ProcessCreate.CreationStatus;

                    KcfFreeReturnData(returnData);
                }

                KcfDereferenceCallback(callback);
            }

            KcfDereferenceClient(client);
        }
    }
    else
    {
        dprintf("KcfpCreateProcessNotifyRoutineEx: no clients\n");
    }
}

VOID KcfpCreateThreadNotifyRoutine(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
    __in BOOLEAN Create
    )
{
    NTSTATUS status;
    KCF_CALLBACK_DATA data;
    PKCF_CLIENT clients[KCF_MAXIMUM_CLIENTS];
    ULONG numberOfClients;
    ULONG i;

    PAGED_CODE();

    KcfInitializeCallbackData(&data, KcfMakeEventId(KCF_CATEGORY_PROCESS, Create ? KCF_PROCESS_EVENT_THREAD_CREATE : KCF_PROCESS_EVENT_THREAD_EXIT));
    data.Parameters.ThreadCreateExit.ThreadId.UniqueProcess = ProcessId;
    data.Parameters.ThreadCreateExit.ThreadId.UniqueThread = ThreadId;
    dprintf("KcfpCreateThreadNotifyRoutine: %s thread PID %u, TID %u\n", Create ? "Create" : "Exit", (ULONG)ProcessId, (ULONG)ThreadId);

    if (KcfGetClientsForCallback(clients, KCF_MAXIMUM_CLIENTS, &numberOfClients, &data, NULL, 0))
    {
        dprintf("KcfpCreateThreadNotifyRoutine: %u clients\n", numberOfClients);

        for (i = 0; i < numberOfClients; i++)
        {
            PKCF_CLIENT client;
            PKCF_CALLBACK callback;
            PKCF_CALLBACK_RETURN_DATA returnData;

            client = clients[i];

            if (NT_SUCCESS(KcfCreateCallback(&callback, client, &data)))
            {
                KcfPerformCallback(callback, ExGetPreviousMode(), NULL, NULL);
                KcfDereferenceCallback(callback);
            }

            KcfDereferenceClient(client);
        }
    }
    else
    {
        dprintf("KcfpCreateThreadNotifyRoutine: no clients\n");
    }
}

VOID KcfpLoadImageNotifyRoutine(
    __in_opt PUNICODE_STRING FullImageName,
    __in HANDLE ProcessId,
    __in PIMAGE_INFO ImageInfo
    )
{
    NTSTATUS status;
    KCF_CALLBACK_DATA data;
    PKCF_CLIENT clients[KCF_MAXIMUM_CLIENTS];
    ULONG numberOfClients;
    ULONG i;

    PAGED_CODE();

    // Ignore images loaded into kernel-mode.
    if (ImageInfo->SystemModeImage)
        return;

    KcfInitializeCallbackData(&data, KcfMakeEventId(KCF_CATEGORY_PROCESS, KCF_PROCESS_EVENT_IMAGE_LOAD));

    if (FullImageName)
        data.Parameters.ImageLoad.FullImageName = *FullImageName;
    else
        RtlInitEmptyUnicodeString(&data.Parameters.ImageLoad.FullImageName, NULL, 0);

    data.Parameters.ImageLoad.ProcessId = ProcessId;
    data.Parameters.ImageLoad.Properties = ImageInfo->Properties;
    data.Parameters.ImageLoad.ImageBase = ImageInfo->ImageBase;
    data.Parameters.ImageLoad.ImageSelector = ImageInfo->ImageSelector;
    data.Parameters.ImageLoad.ImageSize = ImageInfo->ImageSize;
    data.Parameters.ImageLoad.ImageSectionNumber = ImageInfo->ImageSectionNumber;

    dprintf("KcfpLoadImageNotifyRoutine: PID %u: %.*S\n", (ULONG)ProcessId, data.Parameters.ImageLoad.FullImageName.Length / 2, data.Parameters.ImageLoad.FullImageName.Buffer);

    if (KcfGetClientsForCallback(clients, KCF_MAXIMUM_CLIENTS, &numberOfClients, &data, NULL, 0))
    {
        dprintf("KcfpLoadImageNotifyRoutine: %u clients\n", numberOfClients);

        for (i = 0; i < numberOfClients; i++)
        {
            PKCF_CLIENT client;
            PKCF_CALLBACK callback;
            PKCF_CALLBACK_RETURN_DATA returnData;

            client = clients[i];

            if (NT_SUCCESS(KcfCreateCallback(&callback, client, &data)))
            {
                KcfPerformCallback(callback, ExGetPreviousMode(), NULL, NULL);
                KcfDereferenceCallback(callback);
            }

            KcfDereferenceClient(client);
        }
    }
    else
    {
        dprintf("KcfpLoadImageNotifyRoutine: no clients\n");
    }
}
