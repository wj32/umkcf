/*
 * Event filtering
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

NTSTATUS KcfCreateFilter(
    __out PKCF_FILTER *Filter,
    __in PKCF_CLIENT Client
    );

VOID KcfDestroyFilter(
    __post_invalid PKCF_FILTER Filter
    );

VOID KcfpClearFilters(
    __in PKCF_CLIENT Client
    );

BOOLEAN KcfpMatchDataItem(
    __in KCF_FILTER_MODE Mode,
    __in PKCF_DATA_ITEM DataItem1,
    __in PKCF_DATA_ITEM DataItem2
    );

BOOLEAN KcfpMatchFilter(
    __in PKCF_FILTER_DATA FilterData,
    __in PKCF_CALLBACK_DATA Data,
    __in_opt PKCF_DATA_ITEM CustomValues
    );

VOID KcfpSetDefaultValues(
    __inout PKCF_DATA_ITEM Values,
    __in PKCF_CALLBACK_DATA Data
    );

BOOLEAN KcfpFilterClient(
    __in PKCF_CLIENT Client,
    __in PKCF_CALLBACK_DATA Data,
    __in PKCF_DATA_ITEM Values
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KcfFilterInitialization)
#pragma alloc_text(PAGE, KcfDeleteDataItem)
#pragma alloc_text(PAGE, KcfDeleteFilterData)
#pragma alloc_text(PAGE, KcfCreateFilter)
#pragma alloc_text(PAGE, KcfDestroyFilter)
#pragma alloc_text(PAGE, KcfpClearFilters)
#pragma alloc_text(PAGE, KcfiSetFilters)
#pragma alloc_text(PAGE, KcfpMatchDataItem)
#pragma alloc_text(PAGE, KcfpMatchFilter)
#pragma alloc_text(PAGE, KcfpSetDefaultValues)
#pragma alloc_text(PAGE, KcfpFilterClient)
#pragma alloc_text(PAGE, KcfGetClientsForCallback)
#endif

VOID KcfFilterInitialization(
    VOID
    )
{
    PAGED_CODE();

    NOTHING;
}

VOID KcfDeleteDataItem(
    __in PKCF_DATA_ITEM DataItem
    )
{
    PAGED_CODE();

    if (DataItem->Type == DataTypeString && DataItem->u.String.Buffer)
        ExFreePoolWithTag(DataItem->u.String.Buffer, 'FfcK');
}

VOID KcfDeleteFilterData(
    __in PKCF_FILTER_DATA FilterData
    )
{
    PAGED_CODE();

    KcfDeleteDataItem(&FilterData->DataItem);
}

NTSTATUS KcfCreateFilter(
    __out PKCF_FILTER *Filter,
    __in PKCF_CLIENT Client
    )
{
    PKCF_FILTER filter;

    PAGED_CODE();

    filter = ExAllocatePoolWithTag(PagedPool, sizeof(KCF_FILTER), 'FfcK');

    if (!filter)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(filter, 0, sizeof(KCF_FILTER));
    filter->Client = Client;

    *Filter = filter;

    return STATUS_SUCCESS;
}

VOID KcfDestroyFilter(
    __post_invalid PKCF_FILTER Filter
    )
{
    PAGED_CODE();

    KcfDeleteFilterData(&Filter->Data);
    ExFreePoolWithTag(Filter, 'FfcK');
}

VOID KcfpClearFilters(
    __in PKCF_CLIENT Client
    )
{
    ULONG i;
    PLIST_ENTRY currentEntry;
    PKCF_FILTER filter;

    PAGED_CODE();

    for (i = 0; i < KCF_CATEGORY_MAXIMUM; i++)
    {
        currentEntry = Client->FilterListHeads[i].Flink;

        while (currentEntry != &Client->FilterListHeads[i])
        {
            filter = CONTAINING_RECORD(currentEntry, KCF_FILTER, ListEntry);
            currentEntry = currentEntry->Flink;
            KcfDestroyFilter(filter);
        }

        InitializeListHead(&Client->FilterListHeads[i]);
    }
}

NTSTATUS KcfiSetFilters(
    __in PKCF_FILTER_DATA Filters,
    __in ULONG NumberOfFilters,
    __in PKCF_CLIENT Client,
    __in KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status;
    PKCF_FILTER *capturedFilters;
    ULONG numberOfCapturedFilters;
    ULONG i;

    PAGED_CODE();

    if (NumberOfFilters == 0)
    {
        // Delete all filters for this client.
        ExAcquireFastMutex(&Client->FilterListLock);
        KcfpClearFilters(Client);
        ExReleaseFastMutex(&Client->FilterListLock);
        return STATUS_SUCCESS;
    }

    if (NumberOfFilters > KCF_MAXIMUM_FILTERS)
        return STATUS_INVALID_PARAMETER_2;

    if (AccessMode != KernelMode)
    {
        __try
        {
            ProbeForRead(Filters, NumberOfFilters * sizeof(KCF_FILTER_DATA), sizeof(ULONG));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    capturedFilters = ExAllocatePoolWithTag(PagedPool, NumberOfFilters * sizeof(PKCF_FILTER), 'FfcK');

    if (!capturedFilters)
        return STATUS_INSUFFICIENT_RESOURCES;

    numberOfCapturedFilters = 0;

    __try
    {
        for (i = 0; i < NumberOfFilters; i++)
        {
            KCF_FILTER_DATA data;
            PKCF_FILTER filter;

            data = Filters[i];

            // Validate the filter data.

            if (data.Type >= FilterTypeMaximum ||
                data.Category >= KCF_CATEGORY_MAXIMUM ||
                data.Key >= FilterKeyMaximum)
            {
                ExRaiseStatus(STATUS_INVALID_PARAMETER);
            }

            if (data.Key != FilterKeyNone)
            {
                if (data.Mode >= FilterModeMaximum ||
                    data.DataItem.Type == DataTypeInvalid ||
                    data.DataItem.Type >= DataTypeMaximum)
                {
                    ExRaiseStatus(STATUS_INVALID_PARAMETER);
                }

                if (data.DataItem.Type == DataTypeString)
                {
                    if (data.DataItem.u.String.Length != 0)
                    {
                        if (AccessMode != KernelMode)
                            ProbeForRead(data.DataItem.u.String.Buffer, data.DataItem.u.String.Length, sizeof(WCHAR));
                    }
                    else
                    {
                        data.DataItem.u.String.Buffer = NULL;
                    }
                }
            }

            // Create the filter.

            status = KcfCreateFilter(&filter, Client);

            if (!NT_SUCCESS(status))
                ExRaiseStatus(status);

            filter->Data = data;
            memset(&filter->Data.DataItem, 0, sizeof(KCF_DATA_ITEM));
            capturedFilters[numberOfCapturedFilters++] = filter;

            // Capture the string, if necessary.

            if (data.DataItem.Type == DataTypeString)
            {
                PWSTR buffer;

                buffer = ExAllocatePoolWithTag(PagedPool, data.DataItem.u.String.Length, 'FfcK');

                if (!buffer)
                    ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);

                filter->Data.DataItem.Type = DataTypeString;
                filter->Data.DataItem.u.String.Length = data.DataItem.u.String.Length;
                filter->Data.DataItem.u.String.MaximumLength = data.DataItem.u.String.Length;
                filter->Data.DataItem.u.String.Buffer = buffer;

                memcpy(buffer, data.DataItem.u.String.Buffer, data.DataItem.u.String.Length);
            }
            else if (data.DataItem.Type == DataTypeInteger)
            {
                filter->Data.DataItem.Type = DataTypeInteger;
                filter->Data.DataItem.u.Integer = data.DataItem.u.Integer;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        for (i = 0; i < numberOfCapturedFilters; i++)
            KcfDestroyFilter(capturedFilters[i]);

        ExFreePoolWithTag(capturedFilters, 'FfcK');
        return GetExceptionCode();
    }

    // We have successfully captured all filters. Add them to the list.

    ExAcquireFastMutex(&Client->FilterListLock);

    KcfpClearFilters(Client);

    for (i = 0; i < NumberOfFilters; i++)
    {
        PKCF_FILTER filter;

        filter = capturedFilters[i];
        InsertTailList(&Client->FilterListHeads[filter->Data.Category], &filter->ListEntry);
    }

    ExReleaseFastMutex(&Client->FilterListLock);

    ExFreePoolWithTag(capturedFilters, 'FfcK');

    return STATUS_SUCCESS;
}

BOOLEAN KcfpMatchDataItem(
    __in KCF_FILTER_MODE Mode,
    __in PKCF_DATA_ITEM DataItem1,
    __in PKCF_DATA_ITEM DataItem2
    )
{
    BOOLEAN numeric;
    KCF_DATA_ITEM dataItem1;
    KCF_DATA_ITEM dataItem2;
    WCHAR buffer1[21];
    WCHAR buffer2[21];
    ULONG tempInt;
    UNICODE_STRING tempString;

    PAGED_CODE();

    numeric = Mode == FilterModeGreaterThan || Mode == FilterModeLessThan;
    dataItem1 = *DataItem1;
    dataItem2 = *DataItem2;

    // Type conversions
    if (numeric)
    {
        if (DataItem1->Type == DataTypeString)
        {
            if (!NT_SUCCESS(RtlUnicodeStringToInteger(&DataItem1->u.String, 0, &tempInt)))
                return FALSE;

            dataItem1.Type = DataTypeInteger;
            dataItem1.u.Integer = tempInt;
        }

        if (DataItem2->Type == DataTypeString)
        {
            if (!NT_SUCCESS(RtlUnicodeStringToInteger(&DataItem2->u.String, 0, &tempInt)))
                return FALSE;

            dataItem2.Type = DataTypeInteger;
            dataItem2.u.Integer = tempInt;
        }
    }
    else if (Mode != FilterModeEquals || DataItem1->Type != DataItem2->Type)
    {
        if (DataItem1->Type == DataTypeInteger)
        {
            tempString.Length = 0;
            tempString.MaximumLength = sizeof(buffer1);
            tempString.Buffer = buffer1;

            if (!NT_SUCCESS(RtlInt64ToUnicodeString(DataItem1->u.Integer, 10, &tempString)))
                return FALSE;

            dataItem1.Type = DataTypeString;
            dataItem1.u.String = tempString;
        }

        if (DataItem2->Type == DataTypeInteger)
        {
            tempString.Length = 0;
            tempString.MaximumLength = sizeof(buffer2);
            tempString.Buffer = buffer2;

            if (!NT_SUCCESS(RtlInt64ToUnicodeString(DataItem2->u.Integer, 10, &tempString)))
                return FALSE;

            dataItem2.Type = DataTypeString;
            dataItem2.u.String = tempString;
        }
    }

    switch (Mode)
    {
    case FilterModeEquals:
        if (dataItem1.Type == DataTypeInteger)
            return dataItem1.u.Integer == dataItem2.u.Integer;
        else
            return RtlEqualUnicodeString(&dataItem1.u.String, &dataItem2.u.String, TRUE);
    case FilterModeContains:
        return KcfFindUnicodeStringInUnicodeString(&dataItem2.u.String, &dataItem1.u.String, TRUE);
    case FilterModeStartsWith:
        return RtlPrefixUnicodeString(&dataItem1.u.String, &dataItem2.u.String, TRUE);
    case FilterModeEndsWith:
        return KcfSuffixUnicodeString(&dataItem2.u.String, &dataItem1.u.String, TRUE);
    case FilterModeGreaterThan:
        return dataItem1.u.Integer > dataItem2.u.Integer;
    case FilterModeLessThan:
        return dataItem1.u.Integer < dataItem2.u.Integer;
    default:
        return FALSE;
    }
}

BOOLEAN KcfpMatchFilter(
    __in PKCF_FILTER_DATA FilterData,
    __in PKCF_CALLBACK_DATA Data,
    __in PKCF_DATA_ITEM Values
    )
{
    PAGED_CODE();

    if (FilterData->Category != KCF_CATEGORY_ALL && FilterData->Category != Data->EventId.Category)
        return FALSE;
    if (!(FilterData->EventMask & (1 << Data->EventId.Event)))
        return FALSE;
    if (FilterData->Key == FilterKeyNone)
        return TRUE;

    return KcfpMatchDataItem(FilterData->Mode, &FilterData->DataItem, &Values[FilterData->Key]);
}

VOID KcfpSetDefaultValues(
    __inout PKCF_DATA_ITEM Values,
    __in PKCF_CALLBACK_DATA Data
    )
{
    PAGED_CODE();

    if (Values[FilterKeyProcessId].Type == DataTypeInvalid)
    {
        Values[FilterKeyProcessId].Type = DataTypeInteger;
        Values[FilterKeyProcessId].u.Integer = (ULONGLONG)(ULONG_PTR)Data->ClientId.UniqueProcess;
    }
}

FORCEINLINE VOID KcfpFilterCategory(
    __in PKCF_CLIENT Client,
    __in USHORT Category,
    __in PKCF_CALLBACK_DATA Data,
    __in PKCF_DATA_ITEM Values,
    __inout PBOOLEAN Include,
    __inout PBOOLEAN Exclude
    )
{
    PLIST_ENTRY listHead;
    PLIST_ENTRY currentEntry;
    PKCF_FILTER filter;

    listHead = &Client->FilterListHeads[Category];

    ExAcquireFastMutex(&Client->FilterListLock);

    for (currentEntry = listHead->Flink; currentEntry != listHead; currentEntry = currentEntry->Flink)
    {
        filter = CONTAINING_RECORD(currentEntry, KCF_FILTER, ListEntry);

        if (KcfpMatchFilter(&filter->Data, Data, Values))
        {
            if (filter->Data.Type == FilterInclude)
            {
                *Include = TRUE;
            }
            else
            {
                *Exclude = TRUE;
                break;
            }
        }
    }

    ExReleaseFastMutex(&Client->FilterListLock);
}

BOOLEAN KcfpFilterClient(
    __in PKCF_CLIENT Client,
    __in PKCF_CALLBACK_DATA Data,
    __in PKCF_DATA_ITEM Values
    )
{
    PLIST_ENTRY listHead;
    PLIST_ENTRY currentEntry;
    PKCF_FILTER filter;
    BOOLEAN include;
    BOOLEAN exclude;

    PAGED_CODE();

    include = FALSE;
    exclude = FALSE;

    KcfpFilterCategory(Client, KCF_CATEGORY_ALL, Data, Values, &include, &exclude);

    if (exclude)
        return FALSE;

    KcfpFilterCategory(Client, Data->EventId.Category, Data, Values, &include, &exclude);

    if (exclude)
        return FALSE;

    return include;
}

BOOLEAN KcfGetClientsForCallback(
    __out PKCF_CLIENT *Clients,
    __in ULONG MaximumClients,
    __out PULONG NumberOfClients,
    __in PKCF_CALLBACK_DATA Data,
    __in_opt PKCF_DATA_ITEM CustomValues,
    __in ULONG NumberOfCustomValues
    )
{
    KCF_DATA_ITEM values[FilterKeyMaximum];
    ULONG numberOfClients;
    PLIST_ENTRY currentEntry;
    PKCF_CLIENT client;

    PAGED_CODE();

    memset(values, 0, sizeof(values));

    if (CustomValues)
    {
        if (NumberOfCustomValues > FilterKeyMaximum)
            NumberOfCustomValues = FilterKeyMaximum;

        memcpy(values, CustomValues, NumberOfCustomValues * sizeof(KCF_DATA_ITEM));
    }

    KcfpSetDefaultValues(values, Data);

    numberOfClients = 0;

    ExAcquireFastMutex(&KcfClientListLock);

    for (currentEntry = KcfClientListHead.Flink;
        currentEntry != &KcfClientListHead && numberOfClients < MaximumClients;
        currentEntry = currentEntry->Flink)
    {
        client = CONTAINING_RECORD(currentEntry, KCF_CLIENT, ListEntry);

        if (KcfpFilterClient(client, Data, values))
        {
            KcfReferenceClient(client);
            Clients[numberOfClients++] = client;
        }
    }

    ExReleaseFastMutex(&KcfClientListLock);

    *NumberOfClients = numberOfClients;

    return numberOfClients != 0;
}
