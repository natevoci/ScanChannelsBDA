/**
 *	FilterGraphTools.cpp
 *	Copyright (C) 2003-2004 Nate
 *  Copyright (C) 2004 Bionic Donkey
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

#include "StdAfx.h"
#include "FilterGraphTools.h"
#include "GlobalFunctions.h"
#include <stdio.h>
#include <winerror.h>

BOOL ErrorMessageBox(LPCTSTR message)
{
	MessageBox(NULL, message, "DigitalWatch", MB_ICONERROR | MB_OK);
	return FALSE;
}

HRESULT AddFilter(IGraphBuilder* piGraphBuilder, REFCLSID rclsid, IBaseFilter* &pFilter, LPCWSTR pName, BOOL bSilent)
{
	HRESULT hr;
	hr = CoCreateInstance(rclsid, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void **)&pFilter);
	if FAILED(hr)
	{
		if (!bSilent)
		{
			char text[256];
			sprintf((char*)&text, "Failed to load filter: %S  error number %X", pName, (int)hr);
			if (hr == 0x80040154)
				sprintf((char*)&text, "%s\n\nThe filter is not registered.", text);
			ErrorMessageBox(text);
		}
		return hr;
	}

	hr = piGraphBuilder->AddFilter(pFilter, pName);
	if FAILED(hr)
	{
		if (!bSilent)
		{
			char text[256];
			sprintf((char*)&text, "Failed to add filter: %S", pName);
			ErrorMessageBox(text);
		}
		return hr;
	}
	return S_OK;
}

HRESULT AddFilterByName(IGraphBuilder* piGraphBuilder, IBaseFilter* &pFilter, CLSID clsidDeviceClass, LPCWSTR friendlyName)
{
	HRESULT hr = S_OK;
	CComPtr <IEnumMoniker> pEnum;
	CComPtr <ICreateDevEnum> pSysDevEnum;
	CComPtr <IMoniker> pMoniker;

	if(FAILED(hr = pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum)))
	{
        ErrorMessageBox("AddFilterByName: Cannot CoCreate ICreateDevEnum");
        return E_FAIL;
    }

	BOOL FilterAdded = FALSE;
	hr = pSysDevEnum->CreateClassEnumerator(clsidDeviceClass, &pEnum, 0);
	switch(hr)
	{
	case S_OK:
		while(pEnum->Next(1, &pMoniker, 0) == S_OK)
		{
			//Get friendly name
			CComVariant varBSTR;

			//Get filter PropertyBag
			CComPtr <IPropertyBag> pBag;
			if (FAILED(hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pBag))))
			{
				ErrorMessageBox("AddFilterByName: Cannot BindToStorage");
				pMoniker.Release();
				break;
			}

		    if (FAILED(hr = pBag->Read(L"FriendlyName", &varBSTR, NULL)))
			{
				//ErrorMessageBox("AddFilterByName: Failed to get name of filter");
				pBag.Release();
				pMoniker.Release();
				continue;
			}

			LPOLESTR pStr;
			IBindCtx *pBindCtx;
			hr = CreateBindCtx(0, &pBindCtx);

			hr = pMoniker->GetDisplayName(pBindCtx, NULL, &pStr);

			DWORD hash = 0;
			hr = pMoniker->Hash(&hash);

			//Compare names
			CComVariant tmp = friendlyName;
			if(varBSTR.operator !=(tmp))
			{
				pBag.Release();
				pMoniker.Release();
				continue;
			}
	
			//Load filter
			if (FAILED(hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pFilter))))
			{
				ErrorMessageBox("AddFilterByName: Cannot BindToObject");
				pBag.Release();
				pMoniker.Release();
				break;
			}

			//Add filter to graph
			if(FAILED(hr = piGraphBuilder->AddFilter(pFilter, varBSTR.bstrVal)))
			{
				ErrorMessageBox("AddFilterByName: Failed to add Filter");
				pBag.Release();
				pMoniker.Release();
				return E_FAIL;
			}

			pBag.Release();
			pMoniker.Release();
			pEnum.Release();
			pSysDevEnum.Release();
			return S_OK;
		}
		pEnum.Release();
		break;
	case S_FALSE:
		ErrorMessageBox("AddFilterByName: Failed to create System Device Enumerator");
		return E_FAIL;
		break;
	case E_OUTOFMEMORY:
		ErrorMessageBox("AddFilterByName: There is not enough memory available to create a class enumerator.");
		return E_FAIL;
		break;
	case E_POINTER:
		ErrorMessageBox("AddFilterByName: Class Enumerator, NULL pointer argument");
		return E_FAIL;
		break;
	}

	pSysDevEnum.Release();
	ErrorMessageBox("AddFilterByName: Failed to find matching filter.");
	return E_FAIL;
}

HRESULT AddFilterByDevicePath(IGraphBuilder* piGraphBuilder, IBaseFilter* &pFilter, LPCWSTR pDevicePath, LPCWSTR pName)
{
	HRESULT hr;
	CComPtr <IBindCtx> pBindCtx;
	CComPtr <IMoniker> pMoniker;
	DWORD dwEaten;

	if (FAILED(hr = CreateBindCtx(0, &pBindCtx.p)))
	{
		cout << "AddFilterByDevicePath: Could not create bind context" << endl;
		return hr;
	}

	if (FAILED(hr = MkParseDisplayName(pBindCtx, pDevicePath, &dwEaten, &pMoniker)) || (pMoniker.p == NULL))
	{
		cout << "AddFilterByDevicePath: Could not create moniker from device path " << pDevicePath << "  (" << pName << ")" << endl;
		pBindCtx.Release();
		return hr;
	}

	LPWSTR pGraphName = NULL;
	if (pName != NULL)
	{
		strCopy(pGraphName, pName);
	}
	else
	{
		strCopy(pGraphName, pDevicePath);
	}

	hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
	pMoniker.Release();
	if (FAILED(hr))
	{
		cout << "Could Not Create Filter: " << pGraphName << endl;
		return hr;
	}

	hr = piGraphBuilder->AddFilter(pFilter, pGraphName);
	if FAILED(hr)
	{
		cout << "Failed to add filter: " << pGraphName << endl;
		return hr;
	}
	return S_OK;
} 


HRESULT EnumPins(IBaseFilter* source)
{
	HRESULT hr;
	IEnumPins * piEnumPins;
	hr = source->EnumPins( &piEnumPins );
	if SUCCEEDED(hr)
	{
		IPin* piPin;
		while ( piEnumPins->Next(1, &piPin, 0) == NOERROR )
		{
			PIN_INFO pinInfo;
			piPin->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
				pinInfo.pFilter->Release();
			char buff[128];
			if (pinInfo.dir == PINDIR_INPUT)
				sprintf((char*)&buff, "Input Pin Name: %S", pinInfo.achName);
			else if (pinInfo.dir == PINDIR_OUTPUT)
				sprintf((char*)&buff, "Output Pin Name: %S", pinInfo.achName); 
			else
				sprintf((char*)&buff, "Pin Name: %S", pinInfo.achName); 

			MessageBox(NULL, buff, "Pin Name", MB_OK);
			piPin->Release();
		}
		piEnumPins->Release();
	}
	return hr;
}

HRESULT FindPin(IBaseFilter* source, LPCWSTR Id, IPin **pin, REQUESTED_PIN_DIRECTION eRequestedPinDir)
{
	ULONG refCount;

	if (source == NULL)
		return E_FAIL;
	HRESULT hr;
	IEnumPins * piEnumPins;
	hr = source->EnumPins( &piEnumPins );

	if SUCCEEDED(hr)
	{
		IPin *piPins = NULL;
		*pin = NULL;
		while (( piEnumPins->Next(1, &piPins, 0) == NOERROR ) && (*pin == NULL))
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
			{
				refCount = pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			}
			if (wcscmp(Id, pinInfo.achName) == 0)
			{
				if ((eRequestedPinDir == REQUESTED_PINDIR_ANY) || (eRequestedPinDir == pinInfo.dir))
				{
					*pin = piPins;
					refCount = piEnumPins->Release();
					return S_OK;
				}
			}
			refCount = piPins->Release();
		}
		refCount = piEnumPins->Release();
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FindPinByMediaType(IBaseFilter* source, GUID majortype, GUID subtype, IPin **pin, REQUESTED_PIN_DIRECTION eRequestedPinDir)
{
	ULONG refCount;

	if (source == NULL)
		return E_FAIL;
	HRESULT hr;
	IEnumPins * piEnumPins;
	hr = source->EnumPins( &piEnumPins );

	if SUCCEEDED(hr)
	{
		IPin *piPins = NULL;
		*pin = NULL;
		while (( piEnumPins->Next(1, &piPins, 0) == NOERROR ) && (*pin == NULL))
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
			{
				refCount = pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			}
			if ((eRequestedPinDir == REQUESTED_PINDIR_ANY) || (eRequestedPinDir == pinInfo.dir))
			{
				IEnumMediaTypes *piMediaTypes;
				hr = piPins->EnumMediaTypes(&piMediaTypes);
				if (hr == S_OK)
				{
					AM_MEDIA_TYPE *mediaType;
					while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
					{
						if ((mediaType->majortype == majortype) &&
							(mediaType->subtype == subtype))
						{
							*pin = piPins;
							piMediaTypes->Release();
							piEnumPins->Release();
							return S_OK;
						}
					}
					piMediaTypes->Release();
				}
			}
			refCount = piPins->Release();
		}
		refCount = piEnumPins->Release();
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FindFirstFreePin(IBaseFilter* source, PIN_DIRECTION pinDirection, IPin **pin)
{
	if (source == NULL)
		return E_FAIL;
	HRESULT hr;
	ULONG refCount;
	CComPtr <IEnumPins> piEnumPins;
	hr = source->EnumPins( &piEnumPins );

	if SUCCEEDED(hr)
	{
		CComPtr <IPin> piPins;
		*pin = NULL;
		while (( piEnumPins->Next(1, &piPins, 0) == NOERROR ) && (*pin == NULL))
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
				refCount = pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			if (pinInfo.dir == pinDirection)
			{
				//Check if pin is already connected
				CComPtr <IPin> pOtherPin;
				hr = piPins->ConnectedTo(&pOtherPin);
				if ((FAILED(hr)) && (hr != VFW_E_NOT_CONNECTED))
				{
					ErrorMessageBox("Failed to Determin if Pin is already connected");
					piPins.Release();
					piEnumPins.Release();
					return E_FAIL;
				}
				if (pOtherPin != NULL)
				{
					pOtherPin.Release();
					piPins.Release();
					continue;
				}
				*pin = piPins;
				piEnumPins.Release();
				return S_OK;
			}
			piPins.Release();
		}
		piEnumPins.Release();
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FindFilter(IGraphBuilder* piGraphBuilder, LPCWSTR Id, IBaseFilter **filter)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr ;
	ULONG refCount;
	IEnumFilters * piEnumFilters = NULL;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if SUCCEEDED(hr)
	{
		IBaseFilter * piFilter;
		*filter = NULL;
		while ( piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			FILTER_INFO filterInfo;
			hr = piFilter->QueryFilterInfo(&filterInfo);

			if (wcscmp(Id, filterInfo.achName) == 0)
			{
				*filter = piFilter;
				refCount = piEnumFilters->Release();
				return S_OK;
			}
			refCount = piFilter->Release();
		}
		refCount = piEnumFilters->Release();
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FindFilter(IGraphBuilder* piGraphBuilder, CLSID rclsid, IBaseFilter **filter)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr ;
	ULONG refCount;
	IEnumFilters * piEnumFilters = NULL;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if SUCCEEDED(hr)
	{
		IBaseFilter * piFilter;
		*filter = NULL;
		while ( piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			CLSID clsid = GUID_NULL;
			piFilter->GetClassID(&clsid);

			if (IsEqualCLSID(rclsid, clsid))
			{
				*filter = piFilter;
				refCount = piEnumFilters->Release();
				return S_OK;
			}
			refCount = piFilter->Release();
		}
		refCount = piEnumFilters->Release();
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT ConnectPins(IGraphBuilder* piGraphBuilder, IBaseFilter* source, LPCWSTR sourcePinName, IBaseFilter* dest, LPCWSTR destPinName)
{
	if (source == NULL)
	{
		ErrorMessageBox("ConnectPins: source pointer is null");
		return E_FAIL;
	}
	if (dest == NULL)
	{
		ErrorMessageBox("ConnectPins: dest pointer is null");
		return E_FAIL;
	}
	HRESULT hr;
	IPin* Output = NULL;
	IPin* Input = NULL;

	hr = FindPin(source, sourcePinName, &Output, REQUESTED_PINDIR_OUTPUT);

	if (hr != S_OK)
	{
		char buff[128];
		sprintf((char*)&buff, "ConnectPins: Failed to find output pin named %S", sourcePinName);
		ErrorMessageBox(buff);
		return E_FAIL;
	}

	hr = FindPin(dest, destPinName, &Input, REQUESTED_PINDIR_INPUT);
	if (hr != S_OK)
	{
		HelperRelease(Output);
		char buff[128];
		sprintf((char*)&buff, "ConnectPins: Failed to find input pin named %S", destPinName);
		ErrorMessageBox(buff);
		return E_FAIL;
	}

  //hr = Output->Connect(Input, NULL);
  //hr = piGraphBuilder->Connect(Output, Input);
    hr = piGraphBuilder->ConnectDirect(Output, Input, NULL);

	if (SUCCEEDED(hr))
	{
		::OutputDebugString("Pin connection - SUCCEEDED\n");
	}
	else
	{
		::OutputDebugString("Pin connection - FAILED !!!\n");
	}

/*
	//This code will print out the pin format in a MessageBox
	if (hr != S_OK)
	{
		char* string = (char*)malloc(2048);
		LPOLESTR str = (LPOLESTR)malloc(128);
		string[0] = '\0';

		IEnumMediaTypes *piMediaTypes;
		hr = Output->EnumMediaTypes(&piMediaTypes);
		if (hr == S_OK)
		{
			AM_MEDIA_TYPE *mediaType;
			while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
			{
				StringFromGUID2(mediaType->majortype, str, 127);
				sprintf(string, "%sMajorType: %S\n", string, str);
				StringFromGUID2(mediaType->subtype, str, 127);
				sprintf(string, "%sSubType: %S\n", string, str);
				if (mediaType->bFixedSizeSamples)
					sprintf(string, "%sFixed Sized Samples\n", string);
				else
					sprintf(string, "%sNot Fixed Sized Samples\n", string);

				if (mediaType->bTemporalCompression)
					sprintf(string, "%sTemporal Compression\n", string);
				else
					sprintf(string, "%sNot Temporal Compression\n", string);
				StringFromGUID2(mediaType->formattype, str, 127);
				sprintf(string, "%sFormat Type: %S\n\n", string, str);
			}
			piMediaTypes->Release();
		}
		sprintf(string, "%s-----\n", string);

		hr = Input->EnumMediaTypes(&piMediaTypes);
		if (hr == S_OK)
		{
			AM_MEDIA_TYPE *mediaType;
			while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
			{
				StringFromGUID2(mediaType->majortype, str, 127);
				sprintf(string, "%sMajorType: %S\n", string, str);
				StringFromGUID2(mediaType->subtype, str, 127);
				sprintf(string, "%sSubType: %S\n", string, str);
				if (mediaType->bFixedSizeSamples)
					sprintf(string, "%sFixed Sized Samples\n", string);
				else
					sprintf(string, "%sNot Fixed Sized Samples\n", string);

				if (mediaType->bTemporalCompression)
					sprintf(string, "%sTemporal Compression\n", string);
				else
					sprintf(string, "%sNot Temporal Compression\n", string);
				StringFromGUID2(mediaType->formattype, str, 127);
				sprintf(string, "%sFormat Type: %S\n\n", string, str);
			}
			piMediaTypes->Release();
		}
		ErrorMessageBox(string);
	}
*/
	HelperRelease(Output);
	HelperRelease(Input);

	return hr;
}

HRESULT ConnectFilters(IGraphBuilder* piGraphBuilder, IBaseFilter* pFilterUpstream, IBaseFilter* pFilterDownstream)\
{
    HRESULT hr = S_OK;

    CComPtr <IPin> pIPinUpstream;
    CComPtr <IEnumPins> pIEnumPinsUpstream;

    PIN_INFO PinInfoUpstream;
    PIN_INFO PinInfoDownstream;
	BOOL filtersConnected = FALSE;

    if (FAILED(hr = pFilterUpstream->EnumPins(&pIEnumPinsUpstream)))
	{
        ErrorMessageBox("Cannot Enumerate Pins on Upstream Filter");
        return E_FAIL;
    }

    while (pIEnumPinsUpstream->Next(1, &pIPinUpstream, 0) == S_OK)
	{
        if (FAILED(hr = pIPinUpstream->QueryPinInfo (&PinInfoUpstream)))
		{
            ErrorMessageBox("Cannot Obtain Pin Info for Upstream Filter");
			pIPinUpstream.Release();
			pIEnumPinsUpstream.Release();
            return E_FAIL;
        }
		//QueryPinInfo increases the reference count on pFilter, so release it.
		if (PinInfoUpstream.pFilter != NULL)
			PinInfoUpstream.pFilter->Release();

		//Check if pin is an Output pin
		if (PinInfoUpstream.dir != PINDIR_OUTPUT)
		{
			pIPinUpstream.Release();
			continue;
		}

		//Check if pin is already connected
        CComPtr <IPin> pPinDown;
		hr = pIPinUpstream->ConnectedTo(&pPinDown);
		if ((FAILED(hr)) && (hr != VFW_E_NOT_CONNECTED))
		{
			ErrorMessageBox("Failed to Determin if Upstream Ouput Pin is already connected");
			pIPinUpstream.Release();
			pIEnumPinsUpstream.Release();
			return E_FAIL;
		}
		if (pPinDown != NULL)
		{
			pPinDown.Release();
			pIPinUpstream.Release();
			continue;
		}


		CComPtr <IEnumPins> pIEnumPinsDownstream;
		if(FAILED(hr = pFilterDownstream->EnumPins(&pIEnumPinsDownstream)))
		{
			ErrorMessageBox("Cannot enumerate pins on downstream filter!");
			pIPinUpstream.Release();
			pIEnumPinsUpstream.Release();
			return E_FAIL;
		}

		CComPtr <IPin> pIPinDownstream;
		while(pIEnumPinsDownstream->Next (1, &pIPinDownstream, 0) == S_OK)
		{
			if (FAILED(hr = pIPinDownstream->QueryPinInfo(&PinInfoDownstream)))
			{
				ErrorMessageBox("Cannot Obtain Pin Info for Downstream Filter");
				pIPinDownstream.Release();
				pIEnumPinsDownstream.Release();
				pIPinUpstream.Release();
				pIEnumPinsUpstream.Release();
				return E_FAIL;
			}
			//QueryPinInfo increases the reference count on pFilter, so release it.
			if (PinInfoDownstream.pFilter != NULL)
				PinInfoDownstream.pFilter->Release();

			//Check if pin is an Input pin
			if (PinInfoDownstream.dir != PINDIR_INPUT)
			{
				pIPinDownstream.Release();
				continue;
			}

			//Check if pin is already connected
			CComPtr <IPin>  pPinUp;
			hr = pIPinDownstream->ConnectedTo(&pPinUp);
			if((FAILED(hr)) && (hr != VFW_E_NOT_CONNECTED))
			{
				ErrorMessageBox("Failed to Find if Downstream Input Pin is already connected");
				pIPinDownstream.Release();
				pIEnumPinsDownstream.Release();
				pIPinUpstream.Release();
				pIEnumPinsUpstream.Release();
				return E_FAIL;
			}
			if (pPinUp != NULL)
			{
				pPinUp.Release();
				pIPinDownstream.Release();
				continue;
			}

			//Connect pins
			if (SUCCEEDED(hr = ConnectPins(piGraphBuilder, pFilterUpstream, PinInfoUpstream.achName, pFilterDownstream, PinInfoDownstream.achName)))
			{
				pIPinDownstream.Release();
				pIEnumPinsDownstream.Release();
				pIPinUpstream.Release();
				pIEnumPinsUpstream.Release();
				return S_OK;
			}
			pIPinDownstream.Release();
		}
		pIPinUpstream.Release();
    }
	if (hr == S_OK)
		return E_FAIL;
    return hr;
}

HRESULT RenderPin(IGraphBuilder* piGraphBuilder, IBaseFilter* source, LPCWSTR pinName)
{
	HRESULT hr = S_OK;

	IPin* pin = NULL;
//	hr = source->FindPin(pinName, &pin);
	hr = FindPin(source, pinName, &pin, REQUESTED_PINDIR_OUTPUT);
	if (hr != S_OK)
		return hr;

	hr = piGraphBuilder->Render(pin);

	HelperRelease(pin);
	return hr;
}


HRESULT DisconnectAllPins(IGraphBuilder* piGraphBuilder)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;
	ULONG refCount;
	IEnumFilters * piEnumFilters = NULL;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if SUCCEEDED(hr)
	{
		IBaseFilter * piFilter;
		while ( piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			IEnumPins * piEnumPins;
			hr = piFilter->EnumPins( &piEnumPins );
			if SUCCEEDED(hr)
			{
				IPin * piPin;
				while ( piEnumPins->Next(1, &piPin, 0) == NOERROR )
				{
					hr = piPin->Disconnect();
					if (hr == VFW_E_NOT_STOPPED)
						ErrorMessageBox("Could not disconnect pin. The filter is active");
					if (hr == S_OK)
						refCount = piPin->Release();
				}
				piEnumPins->Release();
			}
			piFilter->Release();
		}
		piEnumFilters->Release();
	}
	return hr;
}

HRESULT RemoveAllFilters(IGraphBuilder* piGraphBuilder)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;
	IBaseFilter* piFilter = NULL;
	IBaseFilter* piFilterArray[100];
	int i;
	for ( i=0 ; i<100 ; i++ )
	{
		piFilterArray[i] = NULL;
	}

	IEnumFilters * piEnumFilters = NULL;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if ((hr != S_OK) || (piEnumFilters == NULL))
	{
		ErrorMessageBox("Error removing filters. Can't enumerate graph");
		return hr;
	}

	i=0;
	while ((hr == S_OK) && ( piEnumFilters->Next(1, &piFilter, 0) == S_OK ))
	{
		if (piFilter == NULL)
		{
			ErrorMessageBox("unexpected null pointer. piFilter");
			return E_FAIL;
		}

		piFilterArray[i] = piFilter;
		i++;
		if (i >= 100)
		{
			ErrorMessageBox("More than 100 filters to remove");
			return E_FAIL;
		}
	}
	HelperRelease(piEnumFilters);

	for ( i=0 ; i<100 && (piFilterArray[i] != NULL) ; i++ )
	{
		piGraphBuilder->RemoveFilter(piFilterArray[i]);
	}
	/*for ( i=0 ; i<100 && (piFilterArray[i] != NULL) ; i++ )
	{
		HelperFullRelease(piFilterArray[i]);
	}*/

	return hr;
}

HRESULT GetOverlayMixer(IGraphBuilder* piGraphBuilder, IBaseFilter **filter)
{
	HRESULT hr;
	CLSID clsid = GUID_NULL;

	*filter = NULL;
	CLSIDFromString(L"{CD8743A1-3736-11D0-9E69-00C04FD7C15B}", &clsid);
	hr = FindFilter(piGraphBuilder, clsid, filter);
	if (hr != S_OK)
	{
		CLSIDFromString(L"{A0025E90-E45B-11D1-ABE9-00A0C905F375}", &clsid);
		hr = FindFilter(piGraphBuilder, clsid, filter);
	}

	return hr;
}

HRESULT GetOverlayMixerInputPin(IGraphBuilder* piGraphBuilder, LPCWSTR pinName, IPin **pin)
{
	HRESULT hr;
	IBaseFilter* pfOverlayMixer = NULL;

	hr = GetOverlayMixer(piGraphBuilder, &pfOverlayMixer);
	if (hr == S_OK)
	{
		//int refcount = pfOverlayMixer->AddRef(); refcount = pfOverlayMixer->Release();

		IPin* piPin = NULL;
		hr = FindPin(pfOverlayMixer, pinName, pin, REQUESTED_PINDIR_INPUT);
		if (hr != S_OK)
			MessageBox(NULL, "Error: Could not find Input pin on Overlay Mixer", "DigitalWatch", MB_OK);

		//refcount = (*pin)->AddRef(); refcount = (*pin)->Release();
		
		HelperRelease(pfOverlayMixer);
	}
	return hr;
}


ULONG HelperRelease(IUnknown* piUnknown)
{
	ULONG result = 0;
	try
	{
		if (piUnknown != NULL)
		{
			result = piUnknown->Release();
			if (result <= 0)
				piUnknown = NULL;
		}
	} 
	catch (...)
	{
		piUnknown = NULL;
	}
	return result;
}

ULONG HelperFullRelease(IUnknown* piUnknown)
{
	ULONG result = 0;
	try
	{
		while (piUnknown != NULL)
		{
			result = piUnknown->Release();
			if (result <= 0)
				piUnknown = NULL;
		}
	}
	catch (...)
	{
		piUnknown = NULL;
	}
	return 0;
}

HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    IMoniker * pMoniker;
    IRunningObjectTable *pROT;
    if (FAILED(GetRunningObjectTable(0, &pROT))) {
        return E_FAIL;
    }
	char *buff = (char *)malloc(256);
	wsprintf(buff, "FilterGraph %08x pid %08x", (DWORD)pUnkGraph, GetCurrentProcessId());

	WCHAR *wsz = (WCHAR *)malloc(256);
	mbstowcs(wsz, buff, 256);

    //wsprintf(wsz, L"FilterGraph %08x pid %08x", (DWORD_PTR)pUnkGraph, GetCurrentProcessId());
    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) {
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, pMoniker, pdwRegister);
        pMoniker->Release();
    }
    pROT->Release();
    return hr;
}

void RemoveFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;
    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
	pdwRegister = 0;
}


/////////////////////////////////////////////////////////////////////////////
//BDA functions
/////////////////////////////////////////////////////////////////////////////

HRESULT InitDVBTTuningSpace(CComPtr<ITuningSpace> &piTuningSpace)
{
	HRESULT hr = S_OK;

	if FAILED(hr = piTuningSpace.CoCreateInstance(CLSID_DVBTuningSpace))
	{
		cout << "Could not create DVBTuningSpace" << endl;
		return hr;
	}

	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(piTuningSpace);
	if (!piDVBTuningSpace)
	{
		piTuningSpace.Release();
		cout << "Could not QI TuningSpace" << endl;
		return E_FAIL;
	}
	if (FAILED(hr = piDVBTuningSpace->put_SystemType(DVB_Terrestrial)))
	{
		piDVBTuningSpace.Release();
		piTuningSpace.Release();
		cout << "Could not put SystemType" << endl;
		return hr;
	}
	CComBSTR bstrNetworkType = "{216C62DF-6D7F-4E9A-8571-05F14EDB766A}";
	if (FAILED(hr = piDVBTuningSpace->put_NetworkType(bstrNetworkType)))
	{
		piDVBTuningSpace.Release();
		piTuningSpace.Release();
		cout << "Could not put NetworkType" << endl;
		return hr;
	}
	piDVBTuningSpace.Release();

	return S_OK;
}

HRESULT SubmitDVBTTuneRequest(CComPtr<ITuningSpace> piTuningSpace, CComPtr<ITuneRequest> &pExTuneRequest, long frequency, long bandwidth)
{
	HRESULT hr = S_OK;

	if (!piTuningSpace)
	{
		cout << "Tuning Space is NULL" << endl;
		return E_FAIL;
	}

	//Get an interface to the TuningSpace
	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(piTuningSpace);
    if (!piDVBTuningSpace)
	{
        cout << "Can't Query Interface for an IDVBTuningSpace2" << endl;
		return E_FAIL;
	}

	//Get new TuneRequest from tuning space
	hr = piDVBTuningSpace->CreateTuneRequest(&pExTuneRequest);
	piDVBTuningSpace.Release();
    if (FAILED(hr))
	{
		cout << "Failed to create Tune Request" << endl;
		return hr;
	}

	//Get interface to the TuneRequest
	CComQIPtr <IDVBTuneRequest> piDVBTuneRequest(pExTuneRequest);
	if (!piDVBTuneRequest)
	{
		pExTuneRequest.Release();
        cout << "Can't Query Interface for an IDVBTuneRequest." << endl;
		return E_FAIL;
	}

	//
	// Start
	//
	CComPtr <IDVBTLocator> pDVBTLocator;
	hr = pDVBTLocator.CoCreateInstance(CLSID_DVBTLocator);
	switch (hr)
	{ 
	case REGDB_E_CLASSNOTREG:
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		cout << "The DVBTLocator class isn't registered in the registration database." << endl;
		return hr;

	case CLASS_E_NOAGGREGATION:
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		cout << "The DVBTLocator class can't be created as part of an aggregate." << endl;
		return hr;
	}

	if (FAILED(hr = pDVBTLocator->put_CarrierFrequency(frequency)))
	{
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		cout << "Can't set Frequency on Locator." << endl;
		return hr;
	}
	if(FAILED(hr = pDVBTLocator->put_SymbolRate(bandwidth)))
	{
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		cout << "Can't set Bandwidth on Locator." << endl;
		return hr;
	}
	//
	//End
	//

	// Bind the locator to the tune request.
	
    if(FAILED(hr = piDVBTuneRequest->put_Locator(pDVBTLocator))) {
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
        cout << "Cannot put the locator on DVB Tune Request" << endl;
		return hr;
    }

	//hr = pTuneRequest.QueryInterface(pExTuneRequest);

	pDVBTLocator.Release();
	piDVBTuneRequest.Release();

	return hr;
}




