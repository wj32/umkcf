// 'function': was declared deprecated
#pragma warning(disable: 4996)

#include <ntwin.h>
#include <ntbasic.h>
#include <phnt.h>
#include <phsup.h>
#include <umkcfcl.h>
#include <wchar.h>

UCHAR buffer[4096 * 8];

int __cdecl wmain(int argc, wchar_t *argv[])
{
    NTSTATUS status;
    WCHAR umkcfFileName[500];
    KCF_PARAMETERS parameters;
    KCF_FILTER_DATA filters[2];

    GetCurrentDirectory(sizeof(umkcfFileName) / 2, umkcfFileName);
    wcscat_s(umkcfFileName, sizeof(umkcfFileName) / 2, L"\\umkcf.sys");

    parameters.SecurityLevel = KcfSecurityNone;

    if (!NT_SUCCESS(status = KcfConnect2Ex(KCF_DEVICE_SHORT_NAME, umkcfFileName, &parameters)))
    {
        wprintf(L"Couldn't connect to UMKCF: 0x%x\n", status);
        return 1;
    }

    wprintf(L"Connected.\n");

    filters[0].Type = FilterInclude;
    filters[0].Category = KCF_CATEGORY_ALL;
    filters[0].EventMask = KCF_EVENT_MASK_ALL;
    filters[0].Key = FilterKeyNone;
    filters[1].Type = FilterExclude;
    filters[1].Category = KCF_CATEGORY_ALL;
    filters[1].EventMask = KCF_EVENT_MASK_ALL;
    filters[1].Key = FilterKeyProcessId;
    filters[1].Mode = FilterModeEquals;
    filters[1].DataItem.Type = DataTypeInteger;
    filters[1].DataItem.u.Integer = (ULONGLONG)(ULONG_PTR)NtCurrentProcessId();
    status = KcfSetFilters(filters, 2);

    if (!NT_SUCCESS(status))
        wprintf(L"KcfSetFilters: 0x%x\n", status);

    while (1)
    {
        KCF_CALLBACK_ID callbackId;
        PKCF_CALLBACK_DATA data;
        KCF_CALLBACK_RETURN_DATA returnData;

        wprintf(L"Waiting...\n");
        status = KcfRemoveCallback(NULL, &callbackId, (PKCF_CALLBACK_DATA)buffer, sizeof(buffer), NULL);

        if (!NT_SUCCESS(status))
        {
            wprintf(L"KcfRemoveCallback: 0x%x\n", status);
            return 1;
        }

        data = (PKCF_CALLBACK_DATA)buffer;
        memset(&returnData, 0, sizeof(KCF_CALLBACK_RETURN_DATA));
        returnData.EventId = data->EventId;

        if (data->EventId.Event == KCF_PROCESS_EVENT_PROCESS_CREATE)
        {
            //int result;

            wprintf(L"Process create (%Iu): %.*s\n", data->Parameters.ProcessCreate.ProcessId, data->Parameters.ProcessCreate.ImageFileName.Length / 2, data->Parameters.ProcessCreate.ImageFileName.Buffer);

            //wprintf(L"Press ENTER to return, or n followed by ENTER to disallow.\n");
            //result = getchar();

            //if (result == 'n')
            //{
            //    getchar();
            //    returnData.Parameters.ProcessCreate.CreationStatus = STATUS_NOT_IMPLEMENTED;
            //}
        }
        else if (data->EventId.Event == KCF_PROCESS_EVENT_PROCESS_EXIT)
        {
            wprintf(L"Process exit (%Iu)\n", data->Parameters.ProcessExit.ProcessId);
        }
        else if (data->EventId.Event == KCF_PROCESS_EVENT_THREAD_CREATE)
        {
            wprintf(L"Thread create (PID %Iu, TID %Iu)\n", data->Parameters.ThreadCreateExit.ThreadId.UniqueProcess, data->Parameters.ThreadCreateExit.ThreadId.UniqueThread);
        }
        else if (data->EventId.Event == KCF_PROCESS_EVENT_THREAD_EXIT)
        {
            wprintf(L"Thread exit (PID %Iu, TID %Iu)\n", data->Parameters.ThreadCreateExit.ThreadId.UniqueProcess, data->Parameters.ThreadCreateExit.ThreadId.UniqueThread);
        }
        else if (data->EventId.Event == KCF_PROCESS_EVENT_IMAGE_LOAD)
        {
            wprintf(L"Image load (%Iu): %.*s\n", data->Parameters.ImageLoad.ProcessId, data->Parameters.ImageLoad.FullImageName.Length / 2, data->Parameters.ImageLoad.FullImageName.Buffer);
        }

        status = KcfReturnCallback(callbackId, STATUS_SUCCESS, &returnData, sizeof(KCF_CALLBACK_RETURN_DATA));

        if (!NT_SUCCESS(status))
        {
            wprintf(L"KcfReturnCallback: 0x%x\n", status);
            return 1;
        }
    }

    getchar();

    return 0;
}
