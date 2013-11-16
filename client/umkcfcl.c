/*
 * User-mode client
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

// 'function': was declared deprecated
#pragma warning(disable: 4996)

#include <ntwin.h>
#include <ntbasic.h>
#include <phnt.h>
#include <phsup.h>
#include <umkcfcl.h>

NTSTATUS KcfpDeviceIoControl(
    __in ULONG KcfControlCode,
    __in PVOID InBuffer,
    __in ULONG InBufferLength
    );

HANDLE KcfHandle = NULL;

NTSTATUS KcfConnect(
    __in_opt PWSTR DeviceName
    )
{
    NTSTATUS status;
    HANDLE kphHandle;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK isb;

    if (KcfHandle)
        return STATUS_ADDRESS_ALREADY_EXISTS;

    if (DeviceName)
        RtlInitUnicodeString(&objectName, DeviceName);
    else
        RtlInitUnicodeString(&objectName, KCF_DEVICE_NAME);

    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtOpenFile(
        &kphHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objectAttributes,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_NON_DIRECTORY_FILE
        );

    if (NT_SUCCESS(status))
    {
        KcfHandle = kphHandle;
    }

    return status;
}

NTSTATUS KcfConnect2(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName
    )
{
    return KcfConnect2Ex(DeviceName, FileName, NULL);
}

NTSTATUS KcfConnect2Ex(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName,
    __in_opt PKCF_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    WCHAR fullDeviceName[256];
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle = NULL;
    BOOLEAN started = FALSE;
    BOOLEAN created = FALSE;

    if (!DeviceName)
        DeviceName = KCF_DEVICE_SHORT_NAME;

    if (_snwprintf(fullDeviceName, sizeof(fullDeviceName) / 2 - 1, L"\\Device\\%s", DeviceName) == -1)
        return STATUS_NAME_TOO_LONG;

    // Try to open the device.
    status = KcfConnect(fullDeviceName);

    if (NT_SUCCESS(status) || status == STATUS_ADDRESS_ALREADY_EXISTS)
        return status;

    if (
        status != STATUS_NO_SUCH_DEVICE &&
        status != STATUS_NO_SUCH_FILE &&
        status != STATUS_OBJECT_NAME_NOT_FOUND &&
        status != STATUS_OBJECT_PATH_NOT_FOUND
        )
        return status;

    // Load the driver, and try again.

    // Try to start the service, if it exists.

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (scmHandle)
    {
        serviceHandle = OpenService(scmHandle, DeviceName, SERVICE_START);

        if (serviceHandle)
        {
            if (StartService(serviceHandle, 0, NULL))
                started = TRUE;

            CloseServiceHandle(serviceHandle);
        }

        CloseServiceHandle(scmHandle);
    }

    if (!started && RtlDoesFileExists_U(FileName))
    {
        // Try to create the service.

        scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

        if (scmHandle)
        {
            serviceHandle = CreateService(
                scmHandle,
                DeviceName,
                DeviceName,
                SERVICE_ALL_ACCESS,
                SERVICE_KERNEL_DRIVER,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_IGNORE,
                FileName,
                NULL,
                NULL,
                NULL,
                NULL,
                L""
                );

            if (serviceHandle)
            {
                created = TRUE;

                // Set parameters if the caller supplied them.
                // Note that we fail the entire function if this fails,
                // because failing to set parameters like SecurityLevel may
                // result in security vulnerabilities.
                if (Parameters)
                {
                    status = KcfSetParameters(DeviceName, Parameters);

                    if (!NT_SUCCESS(status))
                    {
                        // Delete the service and fail.
                        goto CreateAndConnectEnd;
                    }
                }

                if (StartService(serviceHandle, 0, NULL))
                    started = TRUE;
            }

            CloseServiceHandle(scmHandle);
        }
    }

    if (started)
    {
        // Try to open the device again.
        status = KcfConnect(fullDeviceName);
    }

CreateAndConnectEnd:
    if (created)
    {
        // "Delete" the service. Since we (may) have a handle to
        // the device, the SCM will delete the service automatically
        // when it is stopped (upon reboot). If we don't have a
        // handle to the device, the service will get deleted immediately,
        // which is a good thing anyway.
        DeleteService(serviceHandle);
        CloseServiceHandle(serviceHandle);
    }

    return status;
}

NTSTATUS KcfDisconnect(
    VOID
    )
{
    NTSTATUS status;
    OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

    if (!KcfHandle)
        return STATUS_ALREADY_DISCONNECTED;

    // Unprotect the handle.

    handleFlagInfo.Inherit = FALSE;
    handleFlagInfo.ProtectFromClose = FALSE;

    NtSetInformationObject(
        KcfHandle,
        ObjectHandleFlagInformation,
        &handleFlagInfo,
        sizeof(OBJECT_HANDLE_FLAG_INFORMATION)
        );

    status = NtClose(KcfHandle);
    KcfHandle = NULL;

    return status;
}

BOOLEAN KcfIsConnected(
    VOID
    )
{
    return KcfHandle != NULL;
}

NTSTATUS KcfSetParameters(
    __in_opt PWSTR DeviceName,
    __in PKCF_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    HANDLE parametersKeyHandle = NULL;
    WCHAR parametersKeyName[360];
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition;
    UNICODE_STRING valueName;

    if (!DeviceName)
        DeviceName = KCF_DEVICE_SHORT_NAME;

    if (_snwprintf(parametersKeyName, sizeof(parametersKeyName) / 2 - 1, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s\\Parameters", DeviceName) == -1)
        return STATUS_NAME_TOO_LONG;

    RtlInitUnicodeString(&objectName, parametersKeyName);
    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtCreateKey(
        &parametersKeyHandle,
        KEY_WRITE | DELETE,
        &objectAttributes,
        0,
        NULL,
        0,
        &disposition
        );

    if (!NT_SUCCESS(status))
        return status;

    RtlInitUnicodeString(&valueName, L"SecurityLevel");
    status = NtSetValueKey(parametersKeyHandle, &valueName, 0, REG_DWORD, &Parameters->SecurityLevel, sizeof(ULONG));

    if (!NT_SUCCESS(status))
        goto SetValuesEnd;

    // Put more parameters here...

SetValuesEnd:
    if (!NT_SUCCESS(status))
    {
        // Delete the key if we created it.
        if (disposition == REG_CREATED_NEW_KEY)
            NtDeleteKey(parametersKeyHandle);
    }

    NtClose(parametersKeyHandle);

    return status;
}

NTSTATUS KcfInstall(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName
    )
{
    return KcfInstallEx(DeviceName, FileName, NULL);
}

NTSTATUS KcfInstallEx(
    __in_opt PWSTR DeviceName,
    __in PWSTR FileName,
    __in_opt PKCF_PARAMETERS Parameters
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle;

    if (!DeviceName)
        DeviceName = KCF_DEVICE_SHORT_NAME;

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    if (!scmHandle)
        return PhGetLastWin32ErrorAsNtStatus();

    serviceHandle = CreateService(
        scmHandle,
        DeviceName,
        DeviceName,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_SYSTEM_START,
        SERVICE_ERROR_IGNORE,
        FileName,
        NULL,
        NULL,
        NULL,
        NULL,
        L""
        );

    if (serviceHandle)
    {
        // See KcfConnect2Ex for more details.
        if (Parameters)
        {
            status = KcfSetParameters(DeviceName, Parameters);

            if (!NT_SUCCESS(status))
            {
                DeleteService(serviceHandle);
                goto CreateEnd;
            }
        }

        if (!StartService(serviceHandle, 0, NULL))
            status = PhGetLastWin32ErrorAsNtStatus();

CreateEnd:
        CloseServiceHandle(serviceHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    CloseServiceHandle(scmHandle);

    return status;
}

NTSTATUS KcfUninstall(
    __in_opt PWSTR DeviceName
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle;

    if (!DeviceName)
        DeviceName = KCF_DEVICE_SHORT_NAME;

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!scmHandle)
        return PhGetLastWin32ErrorAsNtStatus();

    serviceHandle = OpenService(scmHandle, DeviceName, SERVICE_STOP | DELETE);

    if (serviceHandle)
    {
        SERVICE_STATUS serviceStatus;

        ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus);

        if (!DeleteService(serviceHandle))
            status = PhGetLastWin32ErrorAsNtStatus();

        CloseServiceHandle(serviceHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    CloseServiceHandle(scmHandle);

    return status;
}

NTSTATUS KcfpDeviceIoControl(
    __in ULONG KcfControlCode,
    __in PVOID InBuffer,
    __in ULONG InBufferLength
    )
{
    IO_STATUS_BLOCK isb;

    return NtDeviceIoControlFile(
        KcfHandle,
        NULL,
        NULL,
        NULL,
        &isb,
        KcfControlCode,
        InBuffer,
        InBufferLength,
        NULL,
        0
        );
}

NTSTATUS KcfQueryVersion(
    __out PULONG Version
    )
{
    struct
    {
        PULONG Version;
    } input = { Version };

    return KcfpDeviceIoControl(
        KCF_QUERYVERSION,
        &input,
        sizeof(input)
        );
}

NTSTATUS KcfRemoveCallback(
    __in_opt PLARGE_INTEGER Timeout,
    __out PKCF_CALLBACK_ID CallbackId,
    __out PKCF_CALLBACK_DATA Data,
    __in ULONG DataLength,
    __out_opt PULONG ReturnLength
    )
{
    struct
    {
        PLARGE_INTEGER Timeout;
        PKCF_CALLBACK_ID CallbackId;
        PKCF_CALLBACK_DATA Data;
        ULONG DataLength;
        PULONG ReturnLength;
    } input = { Timeout, CallbackId, Data, DataLength, ReturnLength };

    return KcfpDeviceIoControl(
        KCF_REMOVECALLBACK,
        &input,
        sizeof(input)
        );
}

NTSTATUS KcfReturnCallback(
    __in KCF_CALLBACK_ID CallbackId,
    __in NTSTATUS ReturnStatus,
    __in_opt PKCF_CALLBACK_RETURN_DATA ReturnData,
    __in ULONG ReturnDataLength
    )
{
    struct
    {
        KCF_CALLBACK_ID CallbackId;
        NTSTATUS ReturnStatus;
        PKCF_CALLBACK_RETURN_DATA ReturnData;
        ULONG ReturnDataLength;
    } input = { CallbackId, ReturnStatus, ReturnData, ReturnDataLength };

    return KcfpDeviceIoControl(
        KCF_RETURNCALLBACK,
        &input,
        sizeof(input)
        );
}

NTSTATUS KcfSetFilters(
    __in PKCF_FILTER_DATA Filters,
    __in ULONG NumberOfFilters
    )
{
    struct
    {
        PKCF_FILTER_DATA Filters;
        ULONG NumberOfFilters;
    } input = { Filters, NumberOfFilters };

    return KcfpDeviceIoControl(
        KCF_SETFILTERS,
        &input,
        sizeof(input)
        );
}
