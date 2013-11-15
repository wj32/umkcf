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
    KCF_FILTER_DATA filter;

    GetCurrentDirectory(sizeof(umkcfFileName) / 2, umkcfFileName);
    wcscat_s(umkcfFileName, sizeof(umkcfFileName) / 2, L"\\umkcf.sys");

    parameters.SecurityLevel = KcfSecurityNone;

    if (!NT_SUCCESS(status = KcfConnect2Ex(KCF_DEVICE_SHORT_NAME, umkcfFileName, &parameters)))
    {
        wprintf(L"Couldn't connect to UMKCF: 0x%x\n", status);
        return 1;
    }

    wprintf(L"Connected.\n");

    filter.Type = FilterInclude;
    filter.Category = KCF_CATEGORY_PROCESS;
    filter.EventMask = KCF_EVENT_MASK_ALL;
    filter.Key = FilterKeyProcessId;
    filter.Mode = FilterModeEquals;
    filter.DataItem.Type = DataTypeInteger;
    filter.DataItem.u.Integer = 2648;
    status = KcfSetFilters(&filter, 1);

    if (!NT_SUCCESS(status))
        wprintf(L"KcfSetFilters: 0x%x\n", status);

    while (1)
    {
        KCF_CALLBACK_ID callbackId;
        PKCF_CALLBACK_DATA data;

        wprintf(L"Waiting...\n");
        status = KcfRemoveCallback(NULL, &callbackId, (PKCF_CALLBACK_DATA)buffer, sizeof(buffer), NULL);

        if (!NT_SUCCESS(status))
        {
            wprintf(L"KcfRemoveCallback: 0x%x\n", status);
            return 1;
        }

        data = (PKCF_CALLBACK_DATA)buffer;

        if (data->EventId.Event == KCF_PROCESS_EVENT_PROCESS_CREATE)
        {
            wprintf(L"Process create (%Iu): %.*s\n", data->Parameters.ProcessCreate.ProcessId, data->Parameters.ProcessCreate.ImageFileName.Length / 2, data->Parameters.ProcessCreate.ImageFileName.Buffer);
        }

        wprintf(L"Press ENTER to return.\n");
        getchar();

        status = KcfReturnCallback(callbackId, STATUS_SUCCESS, NULL, 0);

        if (!NT_SUCCESS(status))
        {
            wprintf(L"KcfReturnCallback: 0x%x\n", status);
            return 1;
        }
    }

    getchar();

    return 0;
}
