#ifndef _UMKCFAPI_H
#define _UMKCFAPI_H

// This file contains UMKCF definitions shared across
// kernel-mode and user-mode.

#define KCF_DEVICE_SHORT_NAME L"UMKCF"
#define KCF_DEVICE_TYPE 0x9999
#define KCF_DEVICE_NAME (L"\\Device\\" KCF_DEVICE_SHORT_NAME)

// Parameters

typedef enum _KCF_SECURITY_LEVEL
{
    KcfSecurityNone = 0, // all clients are allowed
    KcfSecurityPrivilegeCheck = 1, // require SeDebugPrivilege
    KcfMaxSecurityLevel
} KCF_SECURITY_LEVEL, *PKCF_SECURITY_LEVEL;

// Control codes

#define KCF_CTL_CODE(x) CTL_CODE(KCF_DEVICE_TYPE, 0x800 + x, METHOD_NEITHER, FILE_ANY_ACCESS)

#define KCF_GETFEATURES KCF_CTL_CODE(0)

#endif