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
    KCF_CALLBACK_DATA data;

    PAGED_CODE();

    KcfInitializeCallbackData(&data, CreateInfo ? KCF_EVENT_CREATE_PROCESS : KCF_EVENT_EXIT_PROCESS);
    data.Parameters.CreateProcess.ProcessId = ProcessId;

    if (CreateInfo)
    {
        data.Parameters.CreateProcess.ParentProcessId = CreateInfo->ParentProcessId;
        data.Parameters.CreateProcess.CreatingThreadId = CreateInfo->CreatingThreadId;
        data.Parameters.CreateProcess.ImageFileName = *CreateInfo->ImageFileName;
        if (CreateInfo->CommandLine)
            data.Parameters.CreateProcess.CommandLine = *CreateInfo->CommandLine;
        data.Parameters.CreateProcess.FileOpenNameAvailable = CreateInfo->FileOpenNameAvailable;
    }
}
