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

#include "SystemDeviceEnumerator.h"
#include "GlobalFunctions.h"

//////////////////////////////////////////////////////////////////////
// DirectShowSystemDevice
//////////////////////////////////////////////////////////////////////

DirectShowSystemDevice::DirectShowSystemDevice()
{
	strFriendlyName = NULL;
	strDevicePath = NULL;
	bValid = FALSE;
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
		bValid = right.bValid;
	}
	return *this;
}

HRESULT DirectShowSystemDevice::CreateInstance(CComPtr <IBaseFilter> &pFilter)
{
	HRESULT hr;
	CComPtr <IBindCtx> pBindCtx;
	CComPtr <IMoniker> pMoniker;
	DWORD dwEaten;

	if FAILED(hr = CreateBindCtx(0, &pBindCtx))
	{
		(log << "AddFilterByDevicePath: Could not create bind context\n").Write();
		return hr;
	}

	if (FAILED(hr = MkParseDisplayName(pBindCtx, strDevicePath, &dwEaten, &pMoniker)) || (pMoniker == NULL))
	{
		(log << "AddFilterByDevicePath: Could not create moniker from device path " << strDevicePath << "  (" << strFriendlyName << ")\n").Write();
		pBindCtx.Release();
		return hr;
	}

	if FAILED(hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, reinterpret_cast<void**>(&pFilter)))
	{
		(log << "Could Not Create Filter: " << strFriendlyName << "\n").Write();
		return hr;
	}

	return S_OK;
}

//////////////////////////////////////////////////////////////////////
// DirectShowSystemDeviceEnumerator
//////////////////////////////////////////////////////////////////////

DirectShowSystemDeviceEnumerator::DirectShowSystemDeviceEnumerator(REFCLSID deviceClass)
{
	if FAILED(m_pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum))
	{
		(log << "Could not create system device enumerator\n").Write();
	}
	else
	{
		if FAILED(m_pSysDevEnum->CreateClassEnumerator(deviceClass, &m_pEnum, 0))
		{
			(log << "Could not create device class enumerator\n").Write();
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
	if FAILED(hr = CreateBindCtx(0, &pBindCtx))
	{
		(log << "Could not create bind context\n").Write();
		return hr;
	}

	CComPtr <IPropertyBag> pPropBag;
	if FAILED(hr = pMoniker->BindToStorage(pBindCtx, 0, IID_IPropertyBag, reinterpret_cast<void**>(&pPropBag)))
	{
		(log << "Could not get property bag\n").Write();
		return hr;
	}


	*ppDevice = new DirectShowSystemDevice();
	(*ppDevice)->SetLogCallback(m_pLogCallback);

	VARIANT varName;
	VariantInit(&varName);
	if SUCCEEDED(hr = pPropBag->Read(L"FriendlyName", &varName, 0))
	{
		if (varName.vt == VT_BSTR)
		{
			strCopy((*ppDevice)->strFriendlyName, varName.bstrVal);
		}
		else
		{
			(log << "FriendlyName is not of type VT_BSTR. It's type " << varName.vt << ". Setting to blank string\n").Write();
			strCopy((*ppDevice)->strFriendlyName, L"");
		}
	}
	else
	{
		(log << "FriendlyName does not exist. Setting to blank string\n").Write();
		strCopy((*ppDevice)->strFriendlyName, L"");
	}
	VariantClear(&varName);
	pPropBag.Release();

	LPWSTR pDisplayName;
	if FAILED(hr = pMoniker->GetDisplayName(pBindCtx, NULL, &pDisplayName))
	{
		delete *ppDevice;
		*ppDevice = NULL;
		(log << "Could not get device path\n").Write();
		return hr;
	}
	strCopy((*ppDevice)->strDevicePath, pDisplayName);

	IMalloc* memAlloc = NULL;
	CoGetMalloc(1, &memAlloc);
	memAlloc->Free(pDisplayName);

	(*ppDevice)->bValid = TRUE;

	return S_OK;
}

HRESULT DirectShowSystemDeviceEnumerator::Reset()
{
	return m_pEnum->Reset();
}
