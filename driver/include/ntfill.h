#ifndef NTFILL_H
#define NTFILL_H

// IO

extern POBJECT_TYPE *IoDriverObjectType;

// KE

typedef enum _KAPC_ENVIRONMENT
{
    OriginalApcEnvironment,
    AttachedApcEnvironment,
    CurrentApcEnvironment,
    InsertApcEnvironment
} KAPC_ENVIRONMENT, *PKAPC_ENVIRONMENT;

typedef VOID (NTAPI *PKNORMAL_ROUTINE)(
    __in PVOID NormalContext,
    __in PVOID SystemArgument1,
    __in PVOID SystemArgument2
    );

typedef VOID KKERNEL_ROUTINE(
    __in PRKAPC Apc,
    __inout PKNORMAL_ROUTINE *NormalRoutine,
    __inout PVOID *NormalContext,
    __inout PVOID *SystemArgument1,
    __inout PVOID *SystemArgument2
    );

typedef KKERNEL_ROUTINE (NTAPI *PKKERNEL_ROUTINE);

typedef VOID (NTAPI *PKRUNDOWN_ROUTINE)(
    __in PRKAPC Apc
    );

NTKERNELAPI
VOID
NTAPI
KeInitializeApc(
    __out PRKAPC Apc,
    __in PRKTHREAD Thread,
    __in KAPC_ENVIRONMENT Environment,
    __in PKKERNEL_ROUTINE KernelRoutine,
    __in_opt PKRUNDOWN_ROUTINE RundownRoutine,
    __in_opt PKNORMAL_ROUTINE NormalRoutine,
    __in_opt KPROCESSOR_MODE ProcessorMode,
    __in_opt PVOID NormalContext
    );

NTKERNELAPI
BOOLEAN
NTAPI
KeInsertQueueApc(
    __inout PRKAPC Apc,
    __in_opt PVOID SystemArgument1,
    __in_opt PVOID SystemArgument2,
    __in KPRIORITY Increment
    );

// EX

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQuerySystemInformation(
    __in SYSTEM_INFORMATION_CLASS SystemInformationClass,
    __out_bcount_opt(SystemInformationLength) PVOID SystemInformation,
    __in ULONG SystemInformationLength,
    __out_opt PULONG ReturnLength
    );

// OB

#define OBJ_PROTECT_CLOSE 0x00000001

typedef POBJECT_TYPE (NTAPI *_ObGetObjectType)(
    __in PVOID Object
    );

NTKERNELAPI
NTSTATUS
NTAPI
ObOpenObjectByName(
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in POBJECT_TYPE ObjectType,
    __in KPROCESSOR_MODE PreviousMode,
    __in_opt PACCESS_STATE AccessState,
    __in_opt ACCESS_MASK DesiredAccess,
    __in PVOID ParseContext,
    __out PHANDLE Handle
    );

NTKERNELAPI
NTSTATUS
NTAPI
ObSetHandleAttributes(
    __in HANDLE Handle,
    __in POBJECT_HANDLE_FLAG_INFORMATION HandleFlags,
    __in KPROCESSOR_MODE PreviousMode
    );

NTKERNELAPI
NTSTATUS
ObCloseHandle(
    __in HANDLE Handle,
    __in KPROCESSOR_MODE PreviousMode
    );

// PS

typedef NTSTATUS (NTAPI *_PsAcquireProcessExitSynchronization)(
    __in PEPROCESS Process
    );

typedef NTSTATUS (NTAPI *_PsReleaseProcessExitSynchronization)(
    __in PEPROCESS Process
    );

typedef NTSTATUS (NTAPI *_PsSuspendProcess)(
    __in PEPROCESS Process
    );

typedef NTSTATUS (NTAPI *_PsResumeProcess)(
    __in PEPROCESS Process
    );

typedef BOOLEAN (NTAPI *_PsIsProtectedProcess)(
    __in PEPROCESS Process
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQueryInformationProcess(
    __in HANDLE ProcessHandle,
    __in PROCESSINFOCLASS ProcessInformationClass,
    __out_bcount(ProcessInformationLength) PVOID ProcessInformation,
    __in ULONG ProcessInformationLength,
    __out_opt PULONG ReturnLength
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwSetInformationProcess(
    __in HANDLE ProcessHandle,
    __in PROCESSINFOCLASS ProcessInformationClass,
    __in_bcount(ProcessInformationLength) PVOID ProcessInformation,
    __in ULONG ProcessInformationLength
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQueryInformationThread(
    __in HANDLE ThreadHandle,
    __in THREADINFOCLASS ThreadInformationClass,
    __out_bcount(ThreadInformationLength) PVOID ThreadInformation,
    __in ULONG ThreadInformationLength,
    __out_opt PULONG ReturnLength
    );

NTKERNELAPI
NTSTATUS
NTAPI
PsLookupProcessThreadByCid(
    __in PCLIENT_ID ClientId,
    __out_opt PEPROCESS *Process,
    __out PETHREAD *Thread
    );

NTKERNELAPI
PVOID
NTAPI
PsGetThreadWin32Thread(
    __in PETHREAD Thread
    );

NTKERNELAPI
NTSTATUS
NTAPI
PsGetContextThread(
    __in PETHREAD Thread,
    __inout PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE PreviousMode
    );

NTKERNELAPI
NTSTATUS
NTAPI
PsSetContextThread(
    __in PETHREAD Thread,
    __in PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE PreviousMode
    );

typedef struct _EJOB *PEJOB;

extern POBJECT_TYPE *PsJobType;

NTKERNELAPI
PEJOB
NTAPI
PsGetProcessJob(
    __in PEPROCESS Process
    );

// RTL

// Sensible limit that may or may not correspond to the actual Windows value.
#define MAX_STACK_DEPTH 64

#define RTL_WALK_USER_MODE_STACK 0x00000001
#define RTL_WALK_VALID_FLAGS 0x00000001

NTSYSAPI
ULONG
NTAPI
RtlWalkFrameChain(
    __out PVOID *Callers,
    __in ULONG Count,
    __in ULONG Flags
    );

#endif
