/*
 * Main driver component
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

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
__drv_dispatchType(IRP_MJ_CREATE) DRIVER_DISPATCH KcfDispatchCreate;
__drv_dispatchType(IRP_MJ_CLOSE) DRIVER_DISPATCH KcfDispatchClose;

ULONG KcfpReadIntegerParameter(
    __in_opt HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName,
    __in ULONG DefaultValue
    );

NTSTATUS KcfpReadDriverParameters(
    __in PUNICODE_STRING RegistryPath
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(PAGE, KcfpReadIntegerParameter)
#pragma alloc_text(PAGE, KcfpReadDriverParameters)
#pragma alloc_text(PAGE, KcfiQueryVersion)
#endif

PDRIVER_OBJECT KcfDriverObject;
PDEVICE_OBJECT KcfDeviceObject;
KCF_PARAMETERS KcfParameters;

FAST_MUTEX KcfClientListLock;
LIST_ENTRY KcfClientListHead;

NTSTATUS DriverEntry(
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE();

    KcfDriverObject = DriverObject;

    if (!NT_SUCCESS(status = KcfpReadDriverParameters(RegistryPath)))
        goto ErrorExit;

    if (!NT_SUCCESS(status = KcfPsInitialization()))
        goto ErrorExit;

    // Create the device.

    RtlInitUnicodeString(&deviceName, KCF_DEVICE_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
        );

    if (!NT_SUCCESS(status))
        goto ErrorExit;

    KcfClientInitialization();
    KcfFilterInitialization();

    ExInitializeFastMutex(&KcfClientListLock);
    InitializeListHead(&KcfClientListHead);

    KcfDeviceObject = deviceObject;

    // Set up I/O.

    DriverObject->MajorFunction[IRP_MJ_CREATE] = KcfDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KcfDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KcfDispatchDeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    dprintf("Driver loaded\n");

    return status;

ErrorExit:
    KcfPsUninitialization();

    return status;
}

VOID DriverUnload(
    __in PDRIVER_OBJECT DriverObject
    )
{
    PAGED_CODE();

    IoDeleteDevice(KcfDeviceObject);
    KcfPsUninitialization();
    KcfClientUninitialization();

    dprintf("Driver unloaded\n");
}

NTSTATUS KcfDispatchCreate(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION stackLocation;
    PFILE_OBJECT fileObject;
    PIO_SECURITY_CONTEXT securityContext;
    PKCF_CLIENT client;

    dprintf("Client (PID %Iu) is connecting\n", PsGetCurrentProcessId());

    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    fileObject = stackLocation->FileObject;
    securityContext = stackLocation->Parameters.Create.SecurityContext;

    if (KcfParameters.SecurityLevel == KcfSecurityPrivilegeCheck)
    {
        UCHAR requiredPrivilegesBuffer[FIELD_OFFSET(PRIVILEGE_SET, Privilege) + sizeof(LUID_AND_ATTRIBUTES)];
        PPRIVILEGE_SET requiredPrivileges;

        // Check for SeDebugPrivilege.

        requiredPrivileges = (PPRIVILEGE_SET)requiredPrivilegesBuffer;
        requiredPrivileges->PrivilegeCount = 1;
        requiredPrivileges->Control = PRIVILEGE_SET_ALL_NECESSARY;
        requiredPrivileges->Privilege[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
        requiredPrivileges->Privilege[0].Luid.HighPart = 0;
        requiredPrivileges->Privilege[0].Attributes = 0;

        if (!SePrivilegeCheck(
            requiredPrivileges,
            &securityContext->AccessState->SubjectSecurityContext,
            Irp->RequestorMode
            ))
        {
            status = STATUS_PRIVILEGE_NOT_HELD;
            dprintf("Client (PID %Iu) was rejected\n", PsGetCurrentProcessId());
        }
    }

    if (NT_SUCCESS(status))
    {
        status = KcfCreateClient(&client);

        if (NT_SUCCESS(status))
        {
            ExAcquireFastMutex(&KcfClientListLock);
            InsertTailList(&KcfClientListHead, &client->ListEntry);
            ExReleaseFastMutex(&KcfClientListLock);

            fileObject->FsContext = client;
        }
        else
        {
            dprintf("Unable to create client object: 0x%x\n", status);
        }
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

NTSTATUS KcfDispatchClose(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION stackLocation;
    PFILE_OBJECT fileObject;
    PKCF_CLIENT client;

    dprintf("Client (PID %Iu) is disconnecting\n", PsGetCurrentProcessId());

    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    fileObject = stackLocation->FileObject;
    client = fileObject->FsContext;

    if (client)
    {
        KcfiSetFilters(NULL, 0, client, KernelMode);

        ExAcquireFastMutex(&KcfClientListLock);
        RemoveEntryList(&client->ListEntry);
        ExReleaseFastMutex(&KcfClientListLock);

        KcfCancelClient(client);
        KcfDereferenceClient(client);
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

/**
 * Reads an integer (REG_DWORD) parameter from the registry.
 *
 * \param KeyHandle A handle to the Parameters key. If NULL, the function
 * fails immediately and returns \a DefaultValue.
 * \param ValueName The name of the parameter.
 * \param DefaultValue The value that is returned if the function fails
 * to retrieve the parameter from the registry.
 *
 * \return The parameter value, or \a DefaultValue if the function failed.
 */
ULONG KcfpReadIntegerParameter(
    __in_opt HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName,
    __in ULONG DefaultValue
    )
{
    NTSTATUS status;
    UCHAR buffer[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) + sizeof(ULONG)];
    PKEY_VALUE_PARTIAL_INFORMATION info;
    ULONG resultLength;

    PAGED_CODE();

    if (!KeyHandle)
        return DefaultValue;

    info = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    status = ZwQueryValueKey(
        KeyHandle,
        ValueName,
        KeyValuePartialInformation,
        info,
        sizeof(buffer),
        &resultLength
        );

    if (info->Type != REG_DWORD)
        status = STATUS_OBJECT_TYPE_MISMATCH;

    if (!NT_SUCCESS(status))
    {
        dprintf("Unable to query parameter %.*S: 0x%x\n", ValueName->Length / sizeof(WCHAR), ValueName->Buffer, status);
        return DefaultValue;
    }

    return *(PULONG)info->Data;
}

/**
 * Reads the driver parameters.
 *
 * \param RegistryPath The registry path of the driver.
 */
NTSTATUS KcfpReadDriverParameters(
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status;
    HANDLE parametersKeyHandle;
    UNICODE_STRING parametersString;
    UNICODE_STRING parametersKeyName;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING valueName;

    PAGED_CODE();

    // Open the Parameters key.

    RtlInitUnicodeString(&parametersString, L"\\Parameters");

    parametersKeyName.Length = RegistryPath->Length + parametersString.Length;
    parametersKeyName.MaximumLength = parametersKeyName.Length;
    parametersKeyName.Buffer = ExAllocatePoolWithTag(PagedPool, parametersKeyName.MaximumLength, 'TfcK');

    if (!parametersKeyName.Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    memcpy(parametersKeyName.Buffer, RegistryPath->Buffer, RegistryPath->Length);
    memcpy(&parametersKeyName.Buffer[RegistryPath->Length / sizeof(WCHAR)], parametersString.Buffer, parametersString.Length);

    InitializeObjectAttributes(
        &objectAttributes,
        &parametersKeyName,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
        );
    status = ZwOpenKey(
        &parametersKeyHandle,
        KEY_READ,
        &objectAttributes
        );
    ExFreePoolWithTag(parametersKeyName.Buffer, 'TfcK');

    if (!NT_SUCCESS(status))
    {
        dprintf("Unable to open Parameters key: 0x%x\n", status);
        status = STATUS_SUCCESS;
        parametersKeyHandle = NULL;
        // Continue so we can set up defaults.
    }

    // Read in the parameters.

    RtlInitUnicodeString(&valueName, L"SecurityLevel");
    KcfParameters.SecurityLevel = KcfpReadIntegerParameter(parametersKeyHandle, &valueName, KcfSecurityPrivilegeCheck);

    if (parametersKeyHandle)
        ZwClose(parametersKeyHandle);

    return status;
}

NTSTATUS KcfiQueryVersion(
    __out PULONG Version,
    __in KPROCESSOR_MODE AccessMode
    )
{
    PAGED_CODE();

    if (AccessMode != KernelMode)
    {
        __try
        {
            ProbeForWrite(Version, sizeof(ULONG), sizeof(ULONG));
            *Version = KCF_VERSION;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else
    {
        *Version = KCF_VERSION;
    }

    return STATUS_SUCCESS;
}

// Taken from Process Hacker, basesup.c
/**
 * Locates a string in a string.
 *
 * \param String1 The string to search.
 * \param String2 The string to search for.
 * \param IgnoreCase TRUE to perform a case-insensitive search, otherwise
 * FALSE.
 *
 * \return The index, in characters, of the first occurrence of
 * \a String2 in \a String1. If \a String2 was not found, -1 is returned.
 */
ULONG_PTR KcfFindUnicodeStringInUnicodeString(
    __in PUNICODE_STRING String1,
    __in PUNICODE_STRING String2,
    __in BOOLEAN IgnoreCase
    )
{
    SIZE_T length1;
    SIZE_T length2;
    UNICODE_STRING us1;
    UNICODE_STRING us2;
    WCHAR c;
    SIZE_T i;

    length1 = String1->Length / sizeof(WCHAR);
    length2 = String2->Length / sizeof(WCHAR);

    // Can't be a substring if it's bigger than the first string.
    if (length2 > length1)
        return -1;
    // We always get a match if the substring is zero-length.
    if (length2 == 0)
        return 0;

    us1.Buffer = String1->Buffer;
    us1.Length = String2->Length - sizeof(WCHAR);
    us1.MaximumLength = us1.Length;
    us2.Buffer = String2->Buffer;
    us2.Length = String2->Length - sizeof(WCHAR);
    us2.MaximumLength = us2.Length;

    if (!IgnoreCase)
    {
        c = *us2.Buffer++;

        for (i = length1 - length2 + 1; i != 0; i--)
        {
            if (*us1.Buffer++ == c && RtlEqualUnicodeString(&us1, &us2, FALSE))
            {
                goto FoundUString;
            }
        }
    }
    else
    {
        c = RtlUpcaseUnicodeChar(*us2.Buffer++);

        for (i = length1 - length2 + 1; i != 0; i--)
        {
            if (RtlUpcaseUnicodeChar(*us1.Buffer++) == c && RtlEqualUnicodeString(&us1, &us2, TRUE))
            {
                goto FoundUString;
            }
        }
    }

    return -1;
FoundUString:
    return (ULONG_PTR)(us1.Buffer - String1->Buffer - 1);
}
