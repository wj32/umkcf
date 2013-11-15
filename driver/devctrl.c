/*
 * Device control dispatch
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

NTSTATUS KcfDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION stackLocation;
    PFILE_OBJECT fileObject;
    PKCF_CLIENT client;
    PVOID originalInput;
    ULONG inputLength;
    ULONG ioControlCode;
    KPROCESSOR_MODE accessMode;
    UCHAR capturedInput[16 * sizeof(ULONG_PTR)];
    PVOID capturedInputPointer;

#define VERIFY_INPUT_LENGTH \
    do { \
        /* Ensure at compile time that our local buffer fits this particular call. */ \
        C_ASSERT(sizeof(*input) <= sizeof(capturedInput)); \
        \
        if (inputLength != sizeof(*input)) \
        { \
            status = STATUS_INFO_LENGTH_MISMATCH; \
            goto ControlEnd; \
        } \
    } while (0)

    stackLocation = IoGetCurrentIrpStackLocation(Irp);
    fileObject = stackLocation->FileObject;
    client = fileObject->FsContext;

    if (!client)
    {
        dprintf("No client object on file object 0x%Ix in device control\n", fileObject);
        status = STATUS_INTERNAL_ERROR;
        goto ControlEnd;
    }

    originalInput = stackLocation->Parameters.DeviceIoControl.Type3InputBuffer;
    inputLength = stackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ioControlCode = stackLocation->Parameters.DeviceIoControl.IoControlCode;
    accessMode = Irp->RequestorMode;

    // Make sure we actually have input if the input length is non-zero.
    if (inputLength != 0 && !originalInput)
    {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto ControlEnd;
    }

    // Make sure the caller isn't giving us a huge buffer.
    // If they are, it can't be correct because we have a compile-time check that makes
    // sure our buffer can store the arguments for all the calls.
    if (inputLength > sizeof(capturedInput))
    {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto ControlEnd;
    }

    // Probe and capture the input buffer.
    if (accessMode != KernelMode)
    {
        __try
        {
            ProbeForRead(originalInput, inputLength, sizeof(UCHAR));
            memcpy(capturedInput, originalInput, inputLength);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            status = GetExceptionCode();
            goto ControlEnd;
        }
    }
    else
    {
        memcpy(capturedInput, originalInput, inputLength);
    }

    capturedInputPointer = capturedInput; // avoid casting below

    switch (ioControlCode)
    {
    case KCF_QUERYVERSION:
        {
            struct
            {
                PULONG Version;
            } *input = capturedInputPointer;

            VERIFY_INPUT_LENGTH;

            status = KcfiQueryVersion(
                input->Version,
                accessMode
                );
        }
        break;
    case KCF_REMOVECALLBACK:
        {
            struct
            {
                PLARGE_INTEGER Timeout;
                PKCF_CALLBACK_ID CallbackId;
                PKCF_CALLBACK_DATA Data;
                ULONG DataLength;
                PULONG ReturnLength;
            } *input = capturedInputPointer;

            VERIFY_INPUT_LENGTH;

            status = KcfiRemoveCallback(
                input->Timeout,
                input->CallbackId,
                input->Data,
                input->DataLength,
                input->ReturnLength,
                client,
                accessMode
                );
        }
        break;
    case KCF_RETURNCALLBACK:
        {
            struct
            {
                KCF_CALLBACK_ID CallbackId;
                NTSTATUS ReturnStatus;
                PKCF_CALLBACK_RETURN_DATA ReturnData;
                ULONG ReturnDataLength;
            } *input = capturedInputPointer;

            VERIFY_INPUT_LENGTH;

            status = KcfiReturnCallback(
                input->CallbackId,
                input->ReturnStatus,
                input->ReturnData,
                input->ReturnDataLength,
                client,
                accessMode
                );
        }
        break;
    case KCF_SETFILTERS:
        {
            struct
            {
                PKCF_FILTER_DATA Filters;
                ULONG NumberOfFilters;
            } *input = capturedInputPointer;

            VERIFY_INPUT_LENGTH;

            status = KcfiSetFilters(
                input->Filters,
                input->NumberOfFilters,
                client,
                accessMode
                );
        }
        break;
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

ControlEnd:
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}
