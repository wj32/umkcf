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

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KcfPsInitialization)
#pragma alloc_text(PAGE, KcfPsUninitialization)
#pragma alloc_text(PAGE, KcfpCreateProcessNotifyRoutineEx)
#endif

BOOLEAN KcfCreateProcessNotifyRoutineExSet;

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

    return STATUS_SUCCESS;
}

NTSTATUS KcfPsUninitialization(
    VOID
    )
{
    PAGED_CODE();

    if (KcfCreateProcessNotifyRoutineExSet)
        PsSetCreateProcessNotifyRoutineEx(KcfpCreateProcessNotifyRoutineEx, TRUE);

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
