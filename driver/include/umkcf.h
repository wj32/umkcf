#ifndef UMKCF_H
#define UMKCF_H

#include <ntifs.h>
#define PHNT_MODE PHNT_MODE_KERNEL
#include <phnt.h>
#include <ntfill.h>
#include <umkcfapi.h>

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

// main

extern KCF_PARAMETERS KcfParameters;

// devctrl

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL) DRIVER_DISPATCH KcfDispatchDeviceControl;

NTSTATUS KcfDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

#endif
