/*
 * Filtermapper unit tests for Quartz
 *
 * Copyright (C) 2008 Alexander Dorofeyev
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "wine/test.h"
#include "winbase.h"
#include "dshow.h"
#include "winternl.h"
#include "initguid.h"
#include "wine/fil_data.h"

static ULONG get_refcount(void *iface)
{
    IUnknown *unknown = iface;
    IUnknown_AddRef(unknown);
    return IUnknown_Release(unknown);
}

/* Helper function, checks if filter with given name was enumerated. */
static BOOL enum_find_filter(const WCHAR *wszFilterName, IEnumMoniker *pEnum)
{
    IMoniker *pMoniker = NULL;
    BOOL found = FALSE;
    ULONG nb;
    HRESULT hr;

    while(!found && IEnumMoniker_Next(pEnum, 1, &pMoniker, &nb) == S_OK)
    {
        IPropertyBag * pPropBagCat = NULL;
        VARIANT var;

        VariantInit(&var);

        hr = IMoniker_BindToStorage(pMoniker, NULL, NULL, &IID_IPropertyBag, (LPVOID*)&pPropBagCat);
        ok(SUCCEEDED(hr), "IMoniker_BindToStorage failed with %x\n", hr);

        hr = IPropertyBag_Read(pPropBagCat, L"FriendlyName", &var, NULL);
        ok(SUCCEEDED(hr), "IPropertyBag_Read failed with %x\n", hr);

        if (!wcscmp(V_BSTR(&var), wszFilterName))
            found = TRUE;

        IPropertyBag_Release(pPropBagCat);
        IMoniker_Release(pMoniker);
        VariantClear(&var);
    }

    return found;
}

static void test_fm2_enummatchingfilters(void)
{
    IEnumRegFilters *enum_reg;
    IFilterMapper2 *pMapper = NULL;
    IFilterMapper *mapper;
    HRESULT hr;
    REGFILTER2 rgf2;
    REGFILTERPINS2 rgPins2[2];
    REGPINTYPES rgPinType;
    CLSID clsidFilter1;
    CLSID clsidFilter2;
    IEnumMoniker *pEnum = NULL;
    BOOL found, registered = TRUE;
    REGFILTER *regfilter;
    ULONG count;

    ZeroMemory(&rgf2, sizeof(rgf2));

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = CoCreateGuid(&clsidFilter1);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);
    hr = CoCreateGuid(&clsidFilter2);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);

    /* Test that a test renderer filter is returned when enumerating filters with bRender=FALSE */
    rgf2.dwVersion = 2;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S2(U(rgf2)).cPins2 = 1;
    S2(U(rgf2)).rgPins2 = rgPins2;

    rgPins2[0].dwFlags = REG_PINFLAG_B_RENDERER;
    rgPins2[0].cInstances = 1;
    rgPins2[0].nMediaTypes = 1;
    rgPins2[0].lpMediaType = &rgPinType;
    rgPins2[0].nMediums = 0;
    rgPins2[0].lpMedium = NULL;
    rgPins2[0].clsPinCategory = NULL;

    rgPinType.clsMajorType = &GUID_NULL;
    rgPinType.clsMinorType = &GUID_NULL;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter1, L"Testfilter1", NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    if (hr == E_ACCESSDENIED)
    {
        registered = FALSE;
        skip("Not authorized to register filters\n");
    }
    else
    {
        ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

        rgPins2[0].dwFlags = 0;

        rgPins2[1].dwFlags = REG_PINFLAG_B_OUTPUT;
        rgPins2[1].cInstances = 1;
        rgPins2[1].nMediaTypes = 1;
        rgPins2[1].lpMediaType = &rgPinType;
        rgPins2[1].nMediums = 0;
        rgPins2[1].lpMedium = NULL;
        rgPins2[1].clsPinCategory = NULL;

        S2(U(rgf2)).cPins2 = 2;

        hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter2, L"Testfilter2", NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
        ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(L"Testfilter1", pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 1\n");
        }

        if (pEnum) IEnumMoniker_Release(pEnum);
        pEnum = NULL;

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);
        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(L"Testfilter2", pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 2\n");
        }

        if (pEnum) IEnumMoniker_Release(pEnum);
        pEnum = NULL;

        /* Non renderer must not be returned with bRender=TRUE */

        hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, TRUE, FALSE, 0, NULL, NULL, &GUID_NULL);
        ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);

        if (SUCCEEDED(hr) && pEnum)
        {
            found = enum_find_filter(L"Testfilter1", pEnum);
            ok(found, "EnumMatchingFilters failed to return the test filter 1\n");
        }

        hr = IFilterMapper2_QueryInterface(pMapper, &IID_IFilterMapper, (void **)&mapper);
        ok(hr == S_OK, "QueryInterface(IFilterMapper) failed: %#x\n", hr);

        found = FALSE;
        hr = IFilterMapper_EnumMatchingFilters(mapper, &enum_reg, MERIT_UNLIKELY,
            FALSE, GUID_NULL, GUID_NULL, FALSE, FALSE, GUID_NULL, GUID_NULL);
        ok(hr == S_OK, "IFilterMapper_EnumMatchingFilters failed: %#x\n", hr);
        while (!found && IEnumRegFilters_Next(enum_reg, 1, &regfilter, &count) == S_OK)
        {
            if (!wcscmp(regfilter->Name, L"Testfilter1") && IsEqualGUID(&clsidFilter1, &regfilter->Clsid))
                found = TRUE;
        }
        IEnumRegFilters_Release(enum_reg);
        ok(found, "IFilterMapper didn't find filter\n");
    }

    if (pEnum) IEnumMoniker_Release(pEnum);
    pEnum = NULL;

    hr = IFilterMapper2_EnumMatchingFilters(pMapper, &pEnum, 0, TRUE, MERIT_UNLIKELY, TRUE,
                0, NULL, NULL, &GUID_NULL, TRUE, FALSE, 0, NULL, NULL, &GUID_NULL);
    ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed with %x\n", hr);

    if (SUCCEEDED(hr) && pEnum)
    {
        found = enum_find_filter(L"Testfilter2", pEnum);
        ok(!found, "EnumMatchingFilters should not return the test filter 2\n");
    }

    if (registered)
    {
        hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL,
                &clsidFilter1);
        ok(SUCCEEDED(hr), "IFilterMapper2_UnregisterFilter failed with %x\n", hr);

        hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL,
                &clsidFilter2);
        ok(SUCCEEDED(hr), "IFilterMapper2_UnregisterFilter failed with %x\n", hr);
    }

    out:

    if (pEnum) IEnumMoniker_Release(pEnum);
    if (pMapper) IFilterMapper2_Release(pMapper);
}

static void test_legacy_filter_registration(void)
{
    IEnumRegFilters *enum_reg;
    IEnumMoniker *enum_mon;
    IFilterMapper2 *mapper2;
    IFilterMapper *mapper;
    REGFILTER *regfilter;
    WCHAR clsidstring[40];
    WCHAR key_name[50];
    ULONG count;
    CLSID clsid;
    LRESULT ret;
    HRESULT hr;
    BOOL found;
    HKEY hkey;

    /* Register* functions need a filter class key to write pin and pin media
     * type data to. Create a bogus class key for it. */
    CoCreateGuid(&clsid);
    StringFromGUID2(&clsid, clsidstring, ARRAY_SIZE(clsidstring));
    wcscpy(key_name, L"CLSID\\");
    wcscat(key_name, clsidstring);
    ret = RegCreateKeyExW(HKEY_CLASSES_ROOT, key_name, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL);
    if (ret == ERROR_ACCESS_DENIED)
    {
        skip("Not authorized to register filters\n");
        return;
    }

    /* Test if legacy filter registration scheme works (filter is added to HKCR\Filter). IFilterMapper_RegisterFilter
     * registers in this way. Filters so registered must then be accessible through both IFilterMapper_EnumMatchingFilters
     * and IFilterMapper2_EnumMatchingFilters. */
    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (void **)&mapper2);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);

    hr = IFilterMapper2_QueryInterface(mapper2, &IID_IFilterMapper, (void **)&mapper);
    ok(hr == S_OK, "IFilterMapper2_QueryInterface failed with %x\n", hr);

    /* Set default value - this is interpreted as "friendly name" later. */
    RegSetValueExW(hkey, NULL, 0, REG_SZ, (const BYTE *)L"Testfilter", sizeof(L"Testfilter"));
    RegCloseKey(hkey);

    hr = IFilterMapper_RegisterFilter(mapper, clsid, L"Testfilter", MERIT_UNLIKELY);
    ok(hr == S_OK, "RegisterFilter failed: %#x\n", hr);

    hr = IFilterMapper_RegisterPin(mapper, clsid, L"Pin1", TRUE, FALSE, FALSE, FALSE, GUID_NULL, NULL);
    ok(hr == S_OK, "RegisterPin failed: %#x\n", hr);

    hr = IFilterMapper_RegisterPinType(mapper, clsid, L"Pin1", GUID_NULL, GUID_NULL);
    ok(hr == S_OK, "RegisterPinType failed: %#x\n", hr);

    hr = IFilterMapper2_EnumMatchingFilters(mapper2, &enum_mon, 0, TRUE, MERIT_UNLIKELY, TRUE,
            0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
    ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed: %x\n", hr);
    ok(enum_find_filter(L"Testfilter", enum_mon), "IFilterMapper2 didn't find filter\n");
    IEnumMoniker_Release(enum_mon);

    found = FALSE;
    hr = IFilterMapper_EnumMatchingFilters(mapper, &enum_reg, MERIT_UNLIKELY, TRUE, GUID_NULL, GUID_NULL,
        FALSE, FALSE, GUID_NULL, GUID_NULL);
    ok(hr == S_OK, "IFilterMapper_EnumMatchingFilters failed with %x\n", hr);
    while(!found && IEnumRegFilters_Next(enum_reg, 1, &regfilter, &count) == S_OK)
    {
        if (!wcscmp(regfilter->Name, L"Testfilter") && IsEqualGUID(&clsid, &regfilter->Clsid))
            found = TRUE;
    }
    IEnumRegFilters_Release(enum_reg);
    ok(found, "IFilterMapper didn't find filter\n");

    hr = IFilterMapper_UnregisterFilter(mapper, clsid);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    hr = IFilterMapper2_EnumMatchingFilters(mapper2, &enum_mon, 0, TRUE, MERIT_UNLIKELY, TRUE,
            0, NULL, NULL, &GUID_NULL, FALSE, FALSE, 0, NULL, NULL, &GUID_NULL);
    ok(hr == S_OK, "IFilterMapper2_EnumMatchingFilters failed: %x\n", hr);
    ok(!enum_find_filter(L"Testfilter", enum_mon), "IFilterMapper2 shouldn't find filter\n");
    IEnumMoniker_Release(enum_mon);

    found = FALSE;
    hr = IFilterMapper_EnumMatchingFilters(mapper, &enum_reg, MERIT_UNLIKELY, TRUE, GUID_NULL, GUID_NULL,
        FALSE, FALSE, GUID_NULL, GUID_NULL);
    ok(hr == S_OK, "IFilterMapper_EnumMatchingFilters failed with %x\n", hr);
    while(!found && IEnumRegFilters_Next(enum_reg, 1, &regfilter, &count) == S_OK)
    {
        if (!wcscmp(regfilter->Name, L"Testfilter") && IsEqualGUID(&clsid, &regfilter->Clsid))
            found = TRUE;
    }
    IEnumRegFilters_Release(enum_reg);
    ok(!found, "IFilterMapper shouldn't find filter\n");

    ret = RegDeleteKeyW(HKEY_CLASSES_ROOT, key_name);
    ok(!ret, "RegDeleteKeyA failed: %lu\n", ret);

    hr = IFilterMapper_RegisterFilter(mapper, clsid, L"Testfilter", MERIT_UNLIKELY);
    ok(hr == S_OK, "RegisterFilter failed: %#x\n", hr);

    hr = IFilterMapper_UnregisterFilter(mapper, clsid);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    IFilterMapper_Release(mapper);
    IFilterMapper2_Release(mapper2);
}

static ULONG getRefcount(IUnknown *iface)
{
    IUnknown_AddRef(iface);
    return IUnknown_Release(iface);
}

static void test_ifiltermapper_from_filtergraph(void)
{
    IFilterGraph2* pgraph2 = NULL;
    IFilterMapper2 *pMapper2 = NULL;
    IFilterGraph *filtergraph = NULL;
    HRESULT hr;
    ULONG refcount;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterGraph2, (LPVOID*)&pgraph2);
    ok(hr == S_OK, "CoCreateInstance failed with %08x\n", hr);
    if (!pgraph2) goto out;

    hr = IFilterGraph2_QueryInterface(pgraph2, &IID_IFilterMapper2, (LPVOID*)&pMapper2);
    ok(hr == S_OK, "IFilterGraph2_QueryInterface failed with %08x\n", hr);
    if (!pMapper2) goto out;

    refcount = getRefcount((IUnknown*)pgraph2);
    ok(refcount == 2, "unexpected reference count: %u\n", refcount);
    refcount = getRefcount((IUnknown*)pMapper2);
    ok(refcount == 2, "unexpected reference count: %u\n", refcount);

    IFilterMapper2_AddRef(pMapper2);
    refcount = getRefcount((IUnknown*)pgraph2);
    ok(refcount == 3, "unexpected reference count: %u\n", refcount);
    refcount = getRefcount((IUnknown*)pMapper2);
    ok(refcount == 3, "unexpected reference count: %u\n", refcount);
    IFilterMapper2_Release(pMapper2);

    hr = IFilterMapper2_QueryInterface(pMapper2, &IID_IFilterGraph, (LPVOID*)&filtergraph);
    ok(hr == S_OK, "IFilterMapper2_QueryInterface failed with %08x\n", hr);
    if (!filtergraph) goto out;

    IFilterMapper2_Release(pMapper2);
    pMapper2 = NULL;
    IFilterGraph_Release(filtergraph);
    filtergraph = NULL;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterMapper2, (LPVOID*)&pMapper2);
    ok(hr == S_OK, "CoCreateInstance failed with %08x\n", hr);
    if (!pMapper2) goto out;

    hr = IFilterMapper2_QueryInterface(pMapper2, &IID_IFilterGraph, (LPVOID*)&filtergraph);
    ok(hr == E_NOINTERFACE, "IFilterMapper2_QueryInterface unexpected result: %08x\n", hr);

    out:

    if (pMapper2) IFilterMapper2_Release(pMapper2);
    if (filtergraph) IFilterGraph_Release(filtergraph);
    if (pgraph2) IFilterGraph2_Release(pgraph2);
}

static void test_register_filter_with_null_clsMinorType(void)
{
    static WCHAR wszPinName[] = L"Pin";
    IFilterMapper2 *pMapper = NULL;
    HRESULT hr;
    REGFILTER2 rgf2;
    REGFILTERPINS rgPins;
    REGFILTERPINS2 rgPins2;
    REGPINTYPES rgPinType;
    CLSID clsidFilter1;
    CLSID clsidFilter2;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok(hr == S_OK, "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = CoCreateGuid(&clsidFilter1);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);
    hr = CoCreateGuid(&clsidFilter2);
    ok(hr == S_OK, "CoCreateGuid failed with %x\n", hr);

    rgPinType.clsMajorType = &GUID_NULL;
    /* Make sure quartz accepts it without crashing */
    rgPinType.clsMinorType = NULL;

    /* Test with pin descript version 1 */
    ZeroMemory(&rgf2, sizeof(rgf2));
    rgf2.dwVersion = 1;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S1(U(rgf2)).cPins = 1;
    S1(U(rgf2)).rgPins = &rgPins;

    rgPins.strName = wszPinName;
    rgPins.bRendered = 1;
    rgPins.bOutput = 0;
    rgPins.bZero = 0;
    rgPins.bMany = 0;
    rgPins.clsConnectsToFilter = NULL;
    rgPins.strConnectsToPin = NULL;
    rgPins.nMediaTypes = 1;
    rgPins.lpMediaType = &rgPinType;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter1, L"Testfilter1", NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    if (hr == E_ACCESSDENIED)
    {
        skip("Not authorized to register filters\n");
        goto out;
    }
    ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

    hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL, &clsidFilter1);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    /* Test with pin descript version 2 */
    ZeroMemory(&rgf2, sizeof(rgf2));
    rgf2.dwVersion = 2;
    rgf2.dwMerit = MERIT_UNLIKELY;
    S2(U(rgf2)).cPins2 = 1;
    S2(U(rgf2)).rgPins2 = &rgPins2;

    rgPins2.dwFlags = REG_PINFLAG_B_RENDERER;
    rgPins2.cInstances = 1;
    rgPins2.nMediaTypes = 1;
    rgPins2.lpMediaType = &rgPinType;
    rgPins2.nMediums = 0;
    rgPins2.lpMedium = NULL;
    rgPins2.clsPinCategory = NULL;

    hr = IFilterMapper2_RegisterFilter(pMapper, &clsidFilter2, L"Testfilter2", NULL,
                    &CLSID_LegacyAmFilterCategory, NULL, &rgf2);
    ok(hr == S_OK, "IFilterMapper2_RegisterFilter failed with %x\n", hr);

    hr = IFilterMapper2_UnregisterFilter(pMapper, &CLSID_LegacyAmFilterCategory, NULL, &clsidFilter2);
    ok(hr == S_OK, "FilterMapper_UnregisterFilter failed with %x\n", hr);

    out:

    if (pMapper) IFilterMapper2_Release(pMapper);
}

static void test_parse_filter_data(void)
{
    static const BYTE data_block[] = {
  0x02,0x00,0x00,0x00,0xff,0xff,0x5f,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x70,0x69,0x33,
  0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x30,0x74,0x79,0x33,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x31,0x70,0x69,0x33,
  0x08,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x30,0x74,0x79,0x33,0x00,0x00,0x00,0x00,0x60,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x76,0x69,0x64,0x73,
  0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    BYTE *prgbRegFilter2 = NULL;
    REGFILTER2 *pRegFilter = NULL;
    IFilterMapper2 *pMapper = NULL;
    SAFEARRAYBOUND saBound;
    SAFEARRAY *psa = NULL;
    LPBYTE pbSAData = NULL;
    HRESULT hr;

    IAMFilterData *pData = NULL;

    hr = CoCreateInstance(&CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper2, (LPVOID*)&pMapper);
    ok((hr == S_OK || broken(hr != S_OK)), "CoCreateInstance failed with %x\n", hr);
    if (FAILED(hr)) goto out;

    hr = IFilterMapper2_QueryInterface(pMapper, &IID_IAMFilterData, (LPVOID*)&pData);
    ok((hr == S_OK || broken(hr != S_OK)), "Unable to find IID_IAMFilterData interface\n");
    if (FAILED(hr)) goto out;

    saBound.lLbound = 0;
    saBound.cElements = sizeof(data_block);
    psa = SafeArrayCreate(VT_UI1, 1, &saBound);
    ok(psa != NULL, "Unable to create safe array\n");
    if (!psa) goto out;
    hr = SafeArrayAccessData(psa, (LPVOID *)&pbSAData);
    ok(hr == S_OK, "Unable to access array data\n");
    if (FAILED(hr)) goto out;
    memcpy(pbSAData, data_block, sizeof(data_block));

    hr = IAMFilterData_ParseFilterData(pData, pbSAData, sizeof(data_block), &prgbRegFilter2);
    /* We cannot do anything here.  prgbRegFilter2 is very unstable */
    /* Pre Vista, this is a stack pointer so anything that changes the stack invalidats it */
    /* Post Vista, it is a static pointer in the data section of the module */
    pRegFilter =((REGFILTER2**)prgbRegFilter2)[0];
    ok (hr==S_OK,"Failed to Parse filter Data\n");

    ok(IsBadReadPtr(prgbRegFilter2,sizeof(REGFILTER2*))==0,"Bad read pointer returned\n");
    ok(IsBadReadPtr(pRegFilter,sizeof(REGFILTER2))==0,"Bad read pointer for FilterData\n");
    ok(pRegFilter->dwMerit == 0x5fffff,"Incorrect merit returned\n");

out:
    CoTaskMemFree(pRegFilter);
    if (psa)
    {
        SafeArrayUnaccessData(psa);
        SafeArrayDestroy(psa);
    }
    if (pData)
        IAMFilterData_Release(pData);
    if (pMapper)
        IFilterMapper2_Release(pMapper);
}

static const GUID test_iid = {0x33333333};
static LONG outer_ref = 1;

static HRESULT WINAPI outer_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IFilterMapper3)
            || IsEqualGUID(iid, &test_iid))
    {
        *out = (IUnknown *)0xdeadbeef;
        return S_OK;
    }
    ok(0, "unexpected call %s\n", wine_dbgstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI outer_AddRef(IUnknown *iface)
{
    return InterlockedIncrement(&outer_ref);
}

static ULONG WINAPI outer_Release(IUnknown *iface)
{
    return InterlockedDecrement(&outer_ref);
}

static const IUnknownVtbl outer_vtbl =
{
    outer_QueryInterface,
    outer_AddRef,
    outer_Release,
};

static IUnknown test_outer = {&outer_vtbl};

static void test_aggregation(void)
{
    IFilterMapper3 *mapper, *mapper2;
    IUnknown *unk, *unk2;
    HRESULT hr;
    ULONG ref;

    mapper = (IFilterMapper3 *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_FilterMapper2, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IFilterMapper3, (void **)&mapper);
    ok(hr == E_NOINTERFACE, "Got hr %#x.\n", hr);
    ok(!mapper, "Got interface %p.\n", mapper);

    hr = CoCreateInstance(&CLSID_FilterMapper2, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void **)&unk);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);
    ok(unk != &test_outer, "Returned IUnknown should not be outer IUnknown.\n");
    ref = get_refcount(unk);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);

    ref = IUnknown_AddRef(unk);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);

    ref = IUnknown_Release(unk);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);

    hr = IUnknown_QueryInterface(unk, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == unk, "Got unexpected IUnknown %p.\n", unk2);
    IUnknown_Release(unk2);

    hr = IUnknown_QueryInterface(unk, &IID_IFilterMapper3, (void **)&mapper);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IFilterMapper3_QueryInterface(mapper, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    hr = IFilterMapper3_QueryInterface(mapper, &IID_IFilterMapper3, (void **)&mapper2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(mapper2 == (IFilterMapper3 *)0xdeadbeef, "Got unexpected IFilterMapper3 %p.\n", mapper2);

    hr = IUnknown_QueryInterface(unk, &test_iid, (void **)&unk2);
    ok(hr == E_NOINTERFACE, "Got hr %#x.\n", hr);
    ok(!unk2, "Got unexpected IUnknown %p.\n", unk2);

    hr = IFilterMapper3_QueryInterface(mapper, &test_iid, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    IFilterMapper3_Release(mapper);
    ref = IUnknown_Release(unk);
    ok(!ref, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);
}

START_TEST(filtermapper)
{
    CoInitialize(NULL);

    test_fm2_enummatchingfilters();
    test_legacy_filter_registration();
    test_ifiltermapper_from_filtergraph();
    test_register_filter_with_null_clsMinorType();
    test_parse_filter_data();
    test_aggregation();

    CoUninitialize();
}
