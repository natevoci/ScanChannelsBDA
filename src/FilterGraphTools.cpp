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
#include <vector>

HRESULT FilterGraphTools::AddFilter(IGraphBuilder* piGraphBuilder, REFCLSID rclsid, IBaseFilter **ppiFilter, LPCWSTR pName, BOOL bSilent)
{
	HRESULT hr;
	if FAILED(hr = CoCreateInstance(rclsid, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, reinterpret_cast<void**>(ppiFilter)))
	{
		if (!bSilent)
		{
			if (hr == 0x80040154)
				(log << "Failed to load filter: " << pName << "  error number " << hr << "\n\nThe filter is not registered.\n").Write();
			else
				(log << "Failed to load filter: " << pName << "  error number " << hr << "\n").Write();
		}
		return hr;
	}

	if FAILED(hr = piGraphBuilder->AddFilter(*ppiFilter, pName))
	{
		(*ppiFilter)->Release();
		if (!bSilent)
		{
			(log << "Failed to add filter: " << pName << "\n").Write();
		}
		return hr;
	}
	return S_OK;
}

HRESULT FilterGraphTools::AddFilterByName(IGraphBuilder* piGraphBuilder, IBaseFilter **ppiFilter, CLSID clsidDeviceClass, LPCWSTR friendlyName)
{
	HRESULT hr = S_OK;
	CComPtr <IEnumMoniker> pEnum;
	CComPtr <ICreateDevEnum> pSysDevEnum;
	CComPtr <IMoniker> pMoniker;

	if FAILED(hr = pSysDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum))
	{
        (log << "AddFilterByName: Cannot CoCreate ICreateDevEnum\n").Write();
        return E_FAIL;
    }

	BOOL FilterAdded = FALSE;
	hr = pSysDevEnum->CreateClassEnumerator(clsidDeviceClass, &pEnum, 0);
	switch(hr)
	{
	case S_OK:
		while (pMoniker.Release(), pEnum->Next(1, &pMoniker, 0) == S_OK)
		{
			//Get friendly name
			CComVariant varBSTR;

			//Get filter PropertyBag
			CComPtr <IPropertyBag> pBag;
			if FAILED(hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pBag)))
			{
				(log << "AddFilterByName: Cannot BindToStorage\n").Write();
				break;
			}

		    if FAILED(hr = pBag->Read(L"FriendlyName", &varBSTR, NULL))
			{
				(log << "AddFilterByName: Failed to get name of filter\n").Write();
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
				continue;
			}
	
			//Load filter
			if FAILED(hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, reinterpret_cast<void**>(ppiFilter)))
			{
				(log << "AddFilterByName: Cannot BindToObject\n").Write();
				break;
			}

			//Add filter to graph
			if FAILED(hr = piGraphBuilder->AddFilter(*ppiFilter, varBSTR.bstrVal))
			{
				(log << "AddFilterByName: Failed to add Filter\n").Write();
				(*ppiFilter)->Release();
				return E_FAIL;
			}

			return S_OK;
		}
		break;
	case S_FALSE:
		(log << "AddFilterByName: Failed to create System Device Enumerator\n").Write();
		return E_FAIL;
		break;
	case E_OUTOFMEMORY:
		(log << "AddFilterByName: There is not enough memory available to create a class enumerator.\n").Write();
		return E_FAIL;
		break;
	case E_POINTER:
		(log << "AddFilterByName: Class Enumerator, NULL pointer argument\n").Write();
	  	return E_FAIL;
		break;
	}

	(log << "AddFilterByName: Failed to find matching filter.\n").Write();
	return E_FAIL;
}

HRESULT FilterGraphTools::AddFilterByDevicePath(IGraphBuilder* piGraphBuilder, IBaseFilter **ppiFilter, LPCWSTR pDevicePath, LPCWSTR pName)
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

	if (FAILED(hr = MkParseDisplayName(pBindCtx, pDevicePath, &dwEaten, &pMoniker)) || (pMoniker == NULL))
	{
		(log << "AddFilterByDevicePath: Could not create moniker from device path " << pDevicePath << "  (" << pName << ")\n").Write();
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

	if FAILED(hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, reinterpret_cast<void**>(ppiFilter)))
	{
		(log << "Could Not Create Filter: " << pGraphName << "\n").Write();
		return hr;
	}

	if FAILED(hr = piGraphBuilder->AddFilter(*ppiFilter, pGraphName))
	{
		(*ppiFilter)->Release();
		(log << "Failed to add filter: " << pGraphName << "\n").Write();
		return hr;
	}
	return S_OK;
} 


HRESULT FilterGraphTools::EnumPins(IBaseFilter* piSource)
{
	HRESULT hr;
	CComPtr <IEnumPins> piEnumPins;

	if SUCCEEDED(hr = piSource->EnumPins( &piEnumPins ))
	{
		char* string = (char*)malloc(2048);
		LPOLESTR str = (LPOLESTR)malloc(128);

		CComPtr <IPin> piPin;
		while (piPin.Release(), piEnumPins->Next(1, &piPin, 0) == NOERROR )
		{
			string[0] = '\0';

			PIN_INFO pinInfo;
			piPin->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
				pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.

			if (pinInfo.dir == PINDIR_INPUT)
				sprintf(string, "Input Pin Name: %S\n", pinInfo.achName);
			else if (pinInfo.dir == PINDIR_OUTPUT)
				sprintf(string, "Output Pin Name: %S\n", pinInfo.achName); 
			else
				sprintf(string, "Pin Name: %S\n", pinInfo.achName); 


			CComPtr <IEnumMediaTypes> piMediaTypes;
			hr = piPin->EnumMediaTypes(&piMediaTypes);
			if (hr == S_OK)
			{
				AM_MEDIA_TYPE *mediaType;
				while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
				{
					StringFromGUID2(mediaType->majortype, str, 127);
					sprintf(string, "%s  MajorType: %S\n", string, str);

					StringFromGUID2(mediaType->subtype, str, 127);
					sprintf(string, "%s  SubType: %S\n", string, str);
					if (mediaType->bFixedSizeSamples)
						sprintf(string, "%s  Fixed Sized Samples\n", string);
					else
						sprintf(string, "%s  Not Fixed Sized Samples\n", string);

					if (mediaType->bTemporalCompression)
						sprintf(string, "%s  Temporal Compression\n", string);
					else
						sprintf(string, "%s  Not Temporal Compression\n", string);
					StringFromGUID2(mediaType->formattype, str, 127);
					sprintf(string, "%s  Format Type: %S\n\n", string, str);
				}
			}
			(log << string << "\n").Write();
		}
	}
	return hr;
}

HRESULT FilterGraphTools::FindPin(IBaseFilter* piSource, LPCWSTR Id, IPin **ppiPin, REQUESTED_PIN_DIRECTION eRequestedPinDir)
{
	*ppiPin = NULL;

	if (piSource == NULL)
		return E_FAIL;
	HRESULT hr;
	CComPtr <IEnumPins> piEnumPins;
	
	if SUCCEEDED(hr = piSource->EnumPins( &piEnumPins ))
	{
		CComPtr <IPin> piPins;
		while (piPins.Release(), piEnumPins->Next(1, &piPins, 0) == NOERROR )
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
				pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			if (wcscmp(Id, pinInfo.achName) == 0)
			{
				if ((eRequestedPinDir == REQUESTED_PINDIR_ANY) || (eRequestedPinDir == pinInfo.dir))
				{
					*ppiPin = piPins;
					(*ppiPin)->AddRef();
					return S_OK;
				}
			}
		}
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FilterGraphTools::FindPinByMediaType(IBaseFilter* piSource, GUID majortype, GUID subtype, IPin **ppiPin, REQUESTED_PIN_DIRECTION eRequestedPinDir)
{
	*ppiPin = NULL;

	if (piSource == NULL)
		return E_FAIL;
	HRESULT hr;
	CComPtr <IEnumPins> piEnumPins;

	if SUCCEEDED(hr = piSource->EnumPins( &piEnumPins ))
	{
		CComPtr <IPin> piPins;
		while (piPins.Release(), piEnumPins->Next(1, &piPins, 0) == NOERROR )
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
			{
				pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			}
			if ((eRequestedPinDir == REQUESTED_PINDIR_ANY) || (eRequestedPinDir == pinInfo.dir))
			{
				CComPtr <IEnumMediaTypes> piMediaTypes;
				if SUCCEEDED(hr = piPins->EnumMediaTypes(&piMediaTypes))
				{
					AM_MEDIA_TYPE *mediaType;
					while (piMediaTypes->Next(1, &mediaType, 0) == NOERROR)
					{
						if ((mediaType->majortype == majortype) &&
							(mediaType->subtype == subtype))
						{
							*ppiPin = piPins;
							(*ppiPin)->AddRef();
							return S_OK;
						}
					}
				}
			}
		}
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FilterGraphTools::FindFirstFreePin(IBaseFilter* piSource, IPin **ppiPin, PIN_DIRECTION pinDirection)
{
	*ppiPin = NULL;

	if (piSource == NULL)
		return E_FAIL;
	HRESULT hr;
	CComPtr <IEnumPins> piEnumPins;

	if SUCCEEDED(hr = piSource->EnumPins( &piEnumPins ))
	{
		CComPtr <IPin> piPins;
		while (piPins.Release(), piEnumPins->Next(1, &piPins, 0) == NOERROR )
		{
			PIN_INFO pinInfo;
			hr = piPins->QueryPinInfo(&pinInfo);
			if (pinInfo.pFilter)
				pinInfo.pFilter->Release();	//QueryPinInfo adds a reference to the filter.
			if (pinInfo.dir == pinDirection)
			{
				//Check if pin is already connected
				CComPtr <IPin> pOtherPin;
				hr = piPins->ConnectedTo(&pOtherPin);
				if (FAILED(hr) && (hr != VFW_E_NOT_CONNECTED))
				{
					(log << "Failed to Determin if Pin is already connected\n").Write();
					return E_FAIL;
				}
				if (pOtherPin == NULL)
				{
					*ppiPin = piPins;
					(*ppiPin)->AddRef();
					return S_OK;
				}
			}
		}
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FilterGraphTools::FindFilter(IGraphBuilder* piGraphBuilder, LPCWSTR Id, IBaseFilter **ppiFilter)
{
	*ppiFilter = NULL;
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;
	CComPtr <IEnumFilters> piEnumFilters;
	
	if SUCCEEDED(hr = piGraphBuilder->EnumFilters(&piEnumFilters))
	{
		CComPtr <IBaseFilter> piFilter;
		while (piFilter.Release(), piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			FILTER_INFO filterInfo;
			hr = piFilter->QueryFilterInfo(&filterInfo);

			if (wcscmp(Id, filterInfo.achName) == 0)
			{
				*ppiFilter = piFilter;
				(*ppiFilter)->AddRef();
				return S_OK;
			}
		}
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FilterGraphTools::FindFilter(IGraphBuilder* piGraphBuilder, CLSID rclsid, IBaseFilter **ppiFilter)
{
	*ppiFilter = NULL;
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;

	CComPtr <IEnumFilters> piEnumFilters;
	
	if SUCCEEDED(hr = piGraphBuilder->EnumFilters(&piEnumFilters))
	{
		CComPtr <IBaseFilter> piFilter;
		while (piFilter.Release(), piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			CLSID clsid = GUID_NULL;
			piFilter->GetClassID(&clsid);

			if (IsEqualCLSID(rclsid, clsid))
			{
				*ppiFilter = piFilter;
				(*ppiFilter)->AddRef();
				return S_OK;
			}
		}
	}
	if (hr == S_OK)
		return E_FAIL;
	return hr;
}

HRESULT FilterGraphTools::ConnectFilters(IGraphBuilder* piGraphBuilder, IBaseFilter* piFilterUpstream, LPCWSTR sourcePinName, IBaseFilter* piFilterDownstream, LPCWSTR destPinName)
{
	if (piFilterUpstream == NULL)
	{
		(log << "ConnectPins: piFilterUpstream pointer is null\n").Write();
		return E_FAIL;
	}
	if (piFilterDownstream == NULL)
	{
		(log << "ConnectPins: piFilterDownstream pointer is null\n").Write();
		return E_FAIL;
	}
	HRESULT hr;
	CComPtr <IPin> piOutput;
	CComPtr <IPin> piInput;

	if (S_OK != (hr = FindPin(piFilterUpstream, sourcePinName, &piOutput, REQUESTED_PINDIR_OUTPUT)))
	{
		(log << "ConnectPins: Failed to find output pin named " << sourcePinName << "\n").Write();
		return E_FAIL;
	}

	if (S_OK != (hr = FindPin(piFilterDownstream, destPinName, &piInput, REQUESTED_PINDIR_INPUT)))
	{
		(log << "ConnectPins: Failed to find input pin named " << destPinName << "\n").Write();
		return E_FAIL;
	}

	hr = ConnectFilters(piGraphBuilder, piOutput, piInput);

	return hr;
}

HRESULT FilterGraphTools::ConnectFilters(IGraphBuilder* piGraphBuilder, IBaseFilter* piFilterUpstream, IBaseFilter* piFilterDownstream)
{
    HRESULT hr = S_OK;

    CComPtr <IEnumPins> piEnumPinsUpstream;
    if FAILED(hr = piFilterUpstream->EnumPins(&piEnumPinsUpstream))
	{
        return (log << "Cannot Enumerate Pins on Upstream Filter\n").Write(E_FAIL);
    }

    CComPtr <IPin> piPinUpstream;
    while (piPinUpstream.Release(), piEnumPinsUpstream->Next(1, &piPinUpstream, 0) == S_OK )
	{
	    PIN_INFO PinInfoUpstream;
        if FAILED(hr = piPinUpstream->QueryPinInfo (&PinInfoUpstream))
		{
            return (log << "Cannot Obtain Pin Info for Upstream Filter\n").Write(E_FAIL);
        }
		//QueryPinInfo increases the reference count on pFilter, so release it.
		if (PinInfoUpstream.pFilter != NULL)
			PinInfoUpstream.pFilter->Release();

		//Check if pin is an Output pin
		if (PinInfoUpstream.dir != PINDIR_OUTPUT)
		{
			continue;
		}

		//Check if pin is already connected
        CComPtr <IPin> pPinDown;
		hr = piPinUpstream->ConnectedTo(&pPinDown);
		if (FAILED(hr) && (hr != VFW_E_NOT_CONNECTED))
		{
			return (log << "Failed to Determin if Upstream Ouput Pin is already connected\n").Write(E_FAIL);
		}
		if (pPinDown != NULL)
		{
			continue;
		}


		CComPtr <IEnumPins> pIEnumPinsDownstream;
		if FAILED(hr = piFilterDownstream->EnumPins(&pIEnumPinsDownstream))
		{
			(log << "Cannot enumerate pins on downstream filter!\n").Write();
			return E_FAIL;
		}

		CComPtr <IPin> piPinDownstream;
		while (piPinDownstream.Release(), pIEnumPinsDownstream->Next (1, &piPinDownstream, 0) == S_OK )
		{
		    PIN_INFO PinInfoDownstream;
			if FAILED(hr = piPinDownstream->QueryPinInfo(&PinInfoDownstream))
			{
				return (log << "Cannot Obtain Pin Info for Downstream Filter\n").Write(E_FAIL);
			}
			//QueryPinInfo increases the reference count on pFilter, so release it.
			if (PinInfoDownstream.pFilter != NULL)
				PinInfoDownstream.pFilter->Release();

			//Check if pin is an Input pin
			if (PinInfoDownstream.dir != PINDIR_INPUT)
			{
				continue;
			}

			//Check if pin is already connected
			CComPtr <IPin>  pPinUp;
			hr = piPinDownstream->ConnectedTo(&pPinUp);
			if (FAILED(hr) && (hr != VFW_E_NOT_CONNECTED))
			{
				return (log << "Failed to Find if Downstream Input Pin is already connected\n").Write(E_FAIL);
			}
			if (pPinUp != NULL)
			{
				continue;
			}

			//Connect pins
			if SUCCEEDED(hr = ConnectFilters(piGraphBuilder, piPinUpstream, piPinDownstream))
			{
				return S_OK;
			}
		}
    }
	if (hr == S_OK)
		return E_FAIL;
    return hr;
}

HRESULT FilterGraphTools::ConnectFilters(IGraphBuilder* piGraphBuilder, IBaseFilter* piFilterUpstream, IPin* piPinDownstream)
{
    HRESULT hr = S_OK;

    CComPtr <IEnumPins> piEnumPinsUpstream;
    if FAILED(hr = piFilterUpstream->EnumPins(&piEnumPinsUpstream))
	{
        return (log << "Cannot Enumerate Pins on Upstream Filter\n").Write(E_FAIL);
    }

    CComPtr <IPin> piPinUpstream;
    while (piPinUpstream.Release(), piEnumPinsUpstream->Next(1, &piPinUpstream, 0) == S_OK)
	{
	    PIN_INFO PinInfoUpstream;
        if FAILED(hr = piPinUpstream->QueryPinInfo (&PinInfoUpstream))
		{
            return (log << "Cannot Obtain Pin Info for Upstream Filter\n").Write(E_FAIL);
        }
		//QueryPinInfo increases the reference count on pFilter, so release it.
		if (PinInfoUpstream.pFilter != NULL)
			PinInfoUpstream.pFilter->Release();

		//Check if pin is an Output pin
		if (PinInfoUpstream.dir != PINDIR_OUTPUT)
		{
			continue;
		}

		//Check if pin is already connected
        CComPtr <IPin> pPinDown;
		hr = piPinUpstream->ConnectedTo(&pPinDown);
		if (FAILED(hr) && (hr != VFW_E_NOT_CONNECTED))
		{
			return (log << "Failed to Determin if Upstream Ouput Pin is already connected\n").Write(E_FAIL);
		}
		if (pPinDown != NULL)
		{
			continue;
		}

		if SUCCEEDED(hr = ConnectFilters(piGraphBuilder, piPinUpstream, piPinDownstream))
		{
			return S_OK;
		}

		piPinUpstream.Release();
    }
	if (hr == S_OK)
		return E_FAIL;
    return hr;
}

HRESULT FilterGraphTools::ConnectFilters(IGraphBuilder* piGraphBuilder, IPin* piPinUpstream, IBaseFilter* piFilterDownstream)
{
    HRESULT hr = S_OK;

	CComPtr <IEnumPins> pIEnumPinsDownstream;
	if FAILED(hr = piFilterDownstream->EnumPins(&pIEnumPinsDownstream))
	{
		(log << "Cannot enumerate pins on downstream filter!\n").Write();
		return E_FAIL;
	}

	CComPtr <IPin> piPinDownstream;
	while (piPinDownstream.Release(), pIEnumPinsDownstream->Next (1, &piPinDownstream, 0) == S_OK)
	{
	    PIN_INFO PinInfoDownstream;
		if FAILED(hr = piPinDownstream->QueryPinInfo(&PinInfoDownstream))
		{
			return (log << "Cannot Obtain Pin Info for Downstream Filter\n").Write(E_FAIL);
		}
		//QueryPinInfo increases the reference count on pFilter, so release it.
		if (PinInfoDownstream.pFilter != NULL)
			PinInfoDownstream.pFilter->Release();

		//Check if pin is an Input pin
		if (PinInfoDownstream.dir != PINDIR_INPUT)
		{
			continue;
		}

		//Check if pin is already connected
		CComPtr <IPin>  pPinUp;
		hr = piPinDownstream->ConnectedTo(&pPinUp);
		if (FAILED(hr) && (hr != VFW_E_NOT_CONNECTED))
		{
			return (log << "Failed to Find if Downstream Input Pin is already connected\n").Write(E_FAIL);
		}
		if (pPinUp != NULL)
		{
			continue;
		}

		//Connect pins
		if SUCCEEDED(hr = ConnectFilters(piGraphBuilder, piPinUpstream, piPinDownstream))
		{
			return S_OK;
		}
		piPinDownstream.Release();
	}

	if (hr == S_OK)
		return E_FAIL;
    return hr;
}

HRESULT FilterGraphTools::ConnectFilters(IGraphBuilder* piGraphBuilder, IPin* piPinUpstream, IPin* piPinDownstream)
{
	HRESULT hr = piGraphBuilder->ConnectDirect(piPinUpstream, piPinDownstream, NULL);

	if SUCCEEDED(hr)
	{
		::OutputDebugString("Pin connection - SUCCEEDED\n");
	}
	else
	{
		::OutputDebugString("Pin connection - FAILED !!!\n");
	}

	return hr;
}

HRESULT FilterGraphTools::RenderPin(IGraphBuilder* piGraphBuilder, IBaseFilter* piSource, LPCWSTR pinName)
{
	HRESULT hr = S_OK;

	CComPtr <IPin> piPin;
	if (S_OK != (hr = FindPin(piSource, pinName, &piPin, REQUESTED_PINDIR_OUTPUT)))
		return hr;

	return piGraphBuilder->Render(piPin);
}


HRESULT FilterGraphTools::DisconnectAllPins(IGraphBuilder* piGraphBuilder)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;
	CComPtr <IEnumFilters> piEnumFilters;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if SUCCEEDED(hr)
	{
		CComPtr <IBaseFilter> piFilter;
		while (piFilter.Release(), piEnumFilters->Next(1, &piFilter, 0) == NOERROR )
		{
			CComPtr <IEnumPins> piEnumPins;
			hr = piFilter->EnumPins( &piEnumPins );
			if SUCCEEDED(hr)
			{
				CComPtr <IPin> piPin;
				while (piPin.Release(), piEnumPins->Next(1, &piPin, 0) == NOERROR )
				{
					hr = piPin->Disconnect();
					if (hr == VFW_E_NOT_STOPPED)
						(log << "Could not disconnect pin. The filter is active\n").Write();
				}
			}
		}
	}
	return hr;
}

HRESULT FilterGraphTools::RemoveAllFilters(IGraphBuilder* piGraphBuilder)
{
	if (piGraphBuilder == NULL)
		return E_FAIL;
	HRESULT hr;

	CComPtr <IEnumFilters> piEnumFilters;
	hr = piGraphBuilder->EnumFilters(&piEnumFilters);
	if ((hr != S_OK) || (piEnumFilters == NULL))
	{
		(log << "Error removing filters. Can't enumerate graph\n").Write();
		return hr;
	}

	IBaseFilter *piFilter = NULL;
	std::vector<IBaseFilter *> filterArray;

	piEnumFilters->Reset();
	while ((hr == S_OK) && ( piEnumFilters->Next(1, &piFilter, 0) == S_OK ) && (piFilter != NULL))
	{
		filterArray.push_back(piFilter);
	}

	std::vector<IBaseFilter *>::iterator it = filterArray.begin();
	for ( ; it < filterArray.end() ; it++ )
	{
		piFilter = *it;
		piGraphBuilder->RemoveFilter(piFilter);
		piFilter->Release();
	}

	return hr;
}

HRESULT FilterGraphTools::GetOverlayMixer(IGraphBuilder* piGraphBuilder, IBaseFilter **ppiFilter)
{
	HRESULT hr;

	*ppiFilter = NULL;
	hr = FindFilter(piGraphBuilder, CLSID_OverlayMixer, ppiFilter);
	if (hr != S_OK)
	{
		//Overlay Mixer 2
		CLSID clsid = GUID_NULL;
		CLSIDFromString(L"{A0025E90-E45B-11D1-ABE9-00A0C905F375}", &clsid);
		hr = FindFilter(piGraphBuilder, clsid, ppiFilter);
	}

	return hr;
}

HRESULT FilterGraphTools::GetOverlayMixerInputPin(IGraphBuilder* piGraphBuilder, LPCWSTR pinName, IPin **ppiPin)
{
	HRESULT hr;
	CComPtr <IBaseFilter> pfOverlayMixer;

	hr = GetOverlayMixer(piGraphBuilder, &pfOverlayMixer);
	if (hr == S_OK)
	{
		hr = FindPin(pfOverlayMixer, pinName, ppiPin, REQUESTED_PINDIR_INPUT);
		if (hr != S_OK)
			(log << "Error: Could not find Input pin on Overlay Mixer\n").Write();
	}
	return hr;
}

HRESULT FilterGraphTools::AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    CComPtr <IMoniker> pMoniker;
    CComPtr <IRunningObjectTable> pROT;
    if FAILED(GetRunningObjectTable(0, &pROT))
	{
        return E_FAIL;
    }
	WCHAR *wsz = (WCHAR *)malloc(256);
	swprintf(wsz, L"FilterGraph %08x pid %08x", (DWORD)pUnkGraph, GetCurrentProcessId());

    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if SUCCEEDED(hr)
	{
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph, pMoniker, pdwRegister);
    }
    return hr;
}

void FilterGraphTools::RemoveFromRot(DWORD pdwRegister)
{
    CComPtr <IRunningObjectTable> pROT;
    if SUCCEEDED(GetRunningObjectTable(0, &pROT))
	{
        pROT->Revoke(pdwRegister);
    }
	pdwRegister = 0;
}


/////////////////////////////////////////////////////////////////////////////
//BDA functions
/////////////////////////////////////////////////////////////////////////////

HRESULT FilterGraphTools::InitDVBTTuningSpace(CComPtr <ITuningSpace> &piTuningSpace)
{
	HRESULT hr = S_OK;

	if FAILED(hr = piTuningSpace.CoCreateInstance(CLSID_DVBTuningSpace))
	{
		(log << "Could not create DVBTuningSpace\n").Write();
		return hr;
	}

	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(piTuningSpace);
	if (!piDVBTuningSpace)
	{
		piTuningSpace.Release();
		(log << "Could not QI TuningSpace\n").Write();
		return E_FAIL;
	}
	if FAILED(hr = piDVBTuningSpace->put_SystemType(DVB_Terrestrial))
	{
		piDVBTuningSpace.Release();
		piTuningSpace.Release();
		(log << "Could not put SystemType\n").Write();
		return hr;
	}
	CComBSTR bstrNetworkType = "{216C62DF-6D7F-4E9A-8571-05F14EDB766A}";
	if FAILED(hr = piDVBTuningSpace->put_NetworkType(bstrNetworkType))
	{
		piDVBTuningSpace.Release();
		piTuningSpace.Release();
		(log << "Could not put NetworkType\n").Write();
		return hr;
	}
	piDVBTuningSpace.Release();

	return S_OK;
}

HRESULT FilterGraphTools::CreateDVBTTuneRequest(CComPtr <ITuningSpace> piTuningSpace, CComPtr <ITuneRequest> &pExTuneRequest, long frequency, long bandwidth)
{
	HRESULT hr = S_OK;

	if (!piTuningSpace)
	{
		(log << "Tuning Space is NULL\n").Write();
		return E_FAIL;
	}

	//Get an interface to the TuningSpace
	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(piTuningSpace);
    if (!piDVBTuningSpace)
	{
        (log << "Can't Query Interface for an IDVBTuningSpace2\n").Write();
		return E_FAIL;
	}

	//Get new TuneRequest from tuning space
	if FAILED(hr = piDVBTuningSpace->CreateTuneRequest(&pExTuneRequest))
	{
		(log << "Failed to create Tune Request\n").Write();
		return hr;
	}

	//Get interface to the TuneRequest
	CComQIPtr <IDVBTuneRequest> piDVBTuneRequest(pExTuneRequest);
	if (!piDVBTuneRequest)
	{
		pExTuneRequest.Release();
        (log << "Can't Query Interface for an IDVBTuneRequest.\n").Write();
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
		(log << "The DVBTLocator class isn't registered in the registration database.\n").Write();
		return hr;

	case CLASS_E_NOAGGREGATION:
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		(log << "The DVBTLocator class can't be created as part of an aggregate.\n").Write();
		return hr;
	}

	if FAILED(hr = pDVBTLocator->put_CarrierFrequency(frequency))
	{
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		(log << "Can't set Frequency on Locator.\n").Write();
		return hr;
	}
	if FAILED(hr = pDVBTLocator->put_SymbolRate(bandwidth))
	{
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
		(log << "Can't set Bandwidth on Locator.\n").Write();
		return hr;
	}
	//
	//End
	//

	// Bind the locator to the tune request.
	
    if FAILED(hr = piDVBTuneRequest->put_Locator(pDVBTLocator))
	{
		pDVBTLocator.Release();
		pExTuneRequest.Release();
		piDVBTuneRequest.Release();
        (log << "Cannot put the locator on DVB Tune Request\n").Write();
		return hr;
    }

	pDVBTLocator.Release();
	piDVBTuneRequest.Release();

	return hr;
}

