/**
 *	BDACardCollection.cpp
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

#include "BDACard.h"
#include "StdAfx.h"

//#include "GlobalFunctions.h"
//#include "FilterGraphTools.h"
//#include <dshow.h>
//#include <ks.h> // Must be included before ksmedia.h
//#include <ksmedia.h> // Must be included before bdamedia.h
//#include <bdatypes.h> // Must be included before bdamedia.h
//#include <bdamedia.h>

BDACard::BDACard()
{
	bActive = FALSE;
	bNew = FALSE;
	bDetected = FALSE;
}

BDACard::~BDACard()
{
}

