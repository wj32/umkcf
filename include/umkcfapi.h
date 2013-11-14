#ifndef _UMKCFAPI_H
#define _UMKCFAPI_H

// This file contains UMKCF definitions shared across
// kernel-mode and user-mode.

#define KCF_DEVICE_SHORT_NAME L"UMKCF"
#define KCF_DEVICE_TYPE 0x9999
#define KCF_DEVICE_NAME (L"\\Device\\" KCF_DEVICE_SHORT_NAME)
#define KCF_VERSION 1

// Parameters

typedef enum _KCF_SECURITY_LEVEL
{
    KcfSecurityNone = 0, // all clients are allowed
    KcfSecurityPrivilegeCheck = 1, // require SeDebugPrivilege
    KcfMaxSecurityLevel
} KCF_SECURITY_LEVEL, *PKCF_SECURITY_LEVEL;

typedef ULONG KCF_CALLBACK_ID, *PKCF_CALLBACK_ID;

#define KCF_EVENT_FRAMEWORK 0x1ull
#define KCF_EVENT_CREATE_PROCESS 0x2ull
#define KCF_EVENT_EXIT_PROCESS 0x4ull

typedef struct _KCF_CALLBACK_DATA
{
    ULONGLONG EventId;
    CLIENT_ID ClientId; // ID of current thread

    union
    {
        struct
        {
            HANDLE ProcessId;
            HANDLE ParentProcessId;
            CLIENT_ID CreatingThreadId;
            UNICODE_STRING ImageFileName;
            UNICODE_STRING CommandLine;
            BOOLEAN FileOpenNameAvailable;
        } CreateProcess;
        struct
        {
            HANDLE ProcessId;
        } ExitProcess;
    } Parameters;
} KCF_CALLBACK_DATA, *PKCF_CALLBACK_DATA;

typedef struct _KCF_CALLBACK_RETURN_DATA
{
    ULONGLONG EventId;

    union
    {
        struct
        {
            NTSTATUS CreationStatus;
        } CreateProcess;
    } Parameters;
} KCF_CALLBACK_RETURN_DATA, *PKCF_CALLBACK_RETURN_DATA;

// Control codes

#define KCF_CTL_CODE(x) CTL_CODE(KCF_DEVICE_TYPE, 0x800 + x, METHOD_NEITHER, FILE_ANY_ACCESS)

#define KCF_QUERYVERSION KCF_CTL_CODE(0)

#endif