/**
 *	SystemDeviceEnumerator.cpp
 *	Copyright (C) 2004 Nate
 *
 *	This file is part of DigitalWatch, a free DTV watching and recording
 *	program for the VisionPlus DVB-T.
 *
 *	DigitalWatch is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	DigitalWatch is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with DigitalWatch; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

//this is a file from DigitalWatch 2 that i've hacked up to work here.

#include "SystemDeviceEnumerator.h"
//#include "LogMessage.h"
#include "GlobalFunctions.h"

DirectShowSystemDevice::DirectShowSystemDevice()
{
	strFriendlyName = NULL;
	strDevicePath = NULL;
}

DirectShowSystemDevice::~DirectShowSystemDevice()
{
	if (strFriendlyName)
		delete[] strFriendlyName;
	if (strDevicePath)
		delete[] strDevicePath;
}

const DirectShowSystemDevice &DirectShowSystemDevice::operator=(const DirectShowSystemDevice &right)
{
	if (&right != this)
	{
		strCopy(strFriendlyName, right.strFriendlyName);
		strCopy(strDevicePath, right.strDevicePath);
	}
	return *this;
}

HRESULT DirectShowSystemDevice::CreateInstance(CComPtr <IBaseFilter> &pFilter)
{
	HRESULT hr;
	CComPtr <IBindCtx> pBindCtx;
	CComPtr <IMoniker> pMoniker;
	DWORD dwEaten;

	if (FAILED(hr = CreateBindCtx(0, &pBindCtx.p)))
	{
		//(g_log << "AddFilterByDevicePath: Could not create bind context").Write();
		return hr;
	}

	if (FAILED(hr = MkParseDisplayName(pBindCtx, strDevicePath, &dwEaten, &pMoniker)) || (pMoniker.p == NULL))
	{
		//(g_log << "AddFilterByDevicePath: Could not create moniker from device path " << strDevicePath << "  (" << strFriendlyName << ")").Write();
		pBindCtx.Release();
		return hr;
	}

	hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
	pMoniker.Release();
	if (FAILED(hr))
	{
		//(g_log << "Could Not Create Filter: " << strFriendlyName).Write();
		return hr;
	}

	return S_OK;
}

DirectShowSystemDeviceEnumerator::DirectShowSystemDeviceEnumerator(REFCLSID deviceClass)
{
	if (FAILED(m_pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum)))
	{
		//(g_log << "Could not create system device enumerator").Write();
	}
	else
	{
		if (FAILED(m_pSysDevEnum->CreateClassEnumerator(deviceClass, &m_pEnum, 0)))
		{
			//(g_log << "Could not create device class enumerator").Write();
			m_pSysDevEnum.Release();
		}
	}


}

DirectShowSystemDeviceEnumerator::~DirectShowSystemDeviceEnumerator()
{
	m_pEnum.Release();
	m_pSysDevEnum.Release();
}

HRESULT DirectShowSystemDeviceEnumerator::Next(DirectShowSystemDevice** ppDevice)
{
	if (!m_pSysDevEnum)
		return E_FAIL;
	if (!m_pEnum)
		return E_FAIL;

	HRESULT hr;
	CComPtr <IMoniker> pMoniker;
	if ((hr = m_pEnum->Next(1, &pMoniker, 0)) != S_OK)
		return hr;
	if (!pMoniker)
		return E_FAIL;

	CComPtr <IBindCtx> pBindCtx;
	if (FAILED(hr = CreateBindCtx(0, &pBindCtx)))
	{
		pMoniker.Release();
		//(g_log << "Could not create bind context").Write();
		return hr;
	}

	CComPtr <IPropertyBag> pPropBag;
	hr = pMoniker->BindToStorage(pBindCtx, 0, IID_IPropertyBag, (void**)&pPropBag);
	if (FAILED(hr))
	{
		pMoniker.Release();
		pBindCtx.Release();
		//(g_log << "Could not get property bag").Write();
		return hr;
	}


	*ppDevice = new DirectShowSystemDevice();

	VARIANT varName;
	VariantInit(&varName);
	if (SUCCEEDED(hr = pPropBag->Read(L"FriendlyName", &varName, 0)))
	{
		if (varName.vt == VT_BSTR)
		{
			strCopy((*ppDevice)->strFriendlyName, varName.bstrVal);
		}
		else
		{
			//(g_log << "FriendlyName is not of type VT_BSTR. It's type " << varName.vt << ". Setting to blank string").Write();
			strCopy((*ppDevice)->strFriendlyName, L"");
		}
	}
	else
	{
		//(g_log << "FriendlyName does not exist. Setting to blank string").Write();
		strCopy((*ppDevice)->strFriendlyName, L"");
	}
	VariantClear(&varName);
	pPropBag.Release();

	LPWSTR pDisplayName;
	hr = pMoniker->GetDisplayName(pBindCtx, NULL, &pDisplayName);
	strCopy((*ppDevice)->strDevicePath, pDisplayName);

	IMalloc* memAlloc = NULL;
	CoGetMalloc(1, &memAlloc);
	memAlloc->Free(pDisplayName);

	pBindCtx.Release();
	pMoniker.Release();
	if (FAILED(hr))
	{
		delete *ppDevice;
		*ppDevice = NULL;
		//(g_log << "Could not get device path").Write();
		return hr;
	}

	return S_OK;
}

HRESULT DirectShowSystemDeviceEnumerator::Reset()
{
	return m_pEnum->Reset();
}
