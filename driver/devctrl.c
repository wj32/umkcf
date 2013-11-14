/*
 * KProcessHacker
 *
 * Copyright (C) 2010-2011 wj32
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <umkcf.h>

NTSTATUS KcfDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    )
{
    NTSTATUS status;
    PIO_STACK_LOCATION stackLocation;
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
    originalInput = stackLocation->Parameters.DeviceIoControl.Type3InputBuffer;
    inputLength = stackLocation->Parameters.DeviceIoControl.InputBufferLength;
    ioControlCode = stackLocation->Parameters.DeviceIoControl.IoControlCode;
    accessMode = Irp->RequestorMode;

    // Make sure we actually have input if the input length
    // is non-zero.
    if (inputLength != 0 && !originalInput)
    {
        status = STATUS_INVALID_BUFFER_SIZE;
        goto ControlEnd;
    }

    // Make sure the caller isn't giving us a huge buffer.
    // If they are, it can't be correct because we have a
    // compile-time check that makes sure our buffer can
    // store the arguments for all the calls.
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
