/**
 *	BDACardCollection.h
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

#ifndef BDACARDCOLLECTION_H
#define BDACARDCOLLECTION_H

#include "StdAfx.h"
#include "SystemDeviceEnumerator.h"
#include <vector>
#include <bdatif.h>

class BDACard
{
public:
	BDACard();
	virtual ~BDACard();

	DirectShowSystemDevice tunerDevice;
	DirectShowSystemDevice demodDevice;
	DirectShowSystemDevice captureDevice;

	BOOL bActive;
	BOOL bNew;
	BOOL bDetected;
	BOOL bUsesDemod;
};


class BDACardCollection  
{
public:
	BDACardCollection();
	virtual ~BDACardCollection();

	BOOL LoadCards();
	BOOL LoadCards(LPWSTR filename);
	BOOL SaveCards(LPWSTR filename = NULL);

	std::vector<BDACard *> cards;

private:
	BOOL LoadCardsFromHardware();
	BOOL LoadCardsFromFile();
	void AddCardToList(BDACard* currCard);
	BOOL FindCaptureDevice(DirectShowSystemDevice* pTunerDevice, DirectShowSystemDevice** ppDemodDevice, DirectShowSystemDevice** ppCaptureDevice);

	LPWSTR m_filename;
};

#endif
