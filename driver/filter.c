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

LIST_ENTRY KcfFilterListHeads[CategoryMaximum];

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KcfFilterInitialization)
#pragma alloc_text(PAGE, KcfFilterData)
#endif

VOID KcfFilterInitialization(
    VOID
    )
{
    ULONG i;

    PAGED_CODE();

    for (i = 0; i < CategoryMaximum; i++)
        InitializeListHead(&KcfFilterListHeads[i]);
}

BOOLEAN KcfFilterData(
    __in PKCF_CALLBACK_DATA Data,
    __in_opt PKCF_DATA_ITEM CustomValues
    )
{
    PAGED_CODE();

    return FALSE;
}
