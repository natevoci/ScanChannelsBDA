/**
 *	XMLDocument.h
 *	Copyright (C) 2005 Nate
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

#ifndef XMLDOCUMENT_H
#define XMLDOCUMENT_H

#include "StdAfx.h"
#include <vector>
#include "FileReader.h"
#include "FileWriter.h"
#include "LogMessage.h"
#include "ReferenceCountingClass.h"

//////////////////////////////////////////////////////////////////////
// Attributes
//////////////////////////////////////////////////////////////////////
class XMLAttribute;
class XMLAttributeList
{
public:
	XMLAttributeList();
	virtual ~XMLAttributeList();

	void Add(XMLAttribute *pItem);
	void Remove(int nIndex);

	XMLAttribute *Item(int nIndex);
	XMLAttribute *Item(LPWSTR pName);
	int Count();

private:
	std::vector<XMLAttribute *> m_attributes;
};

class XMLAttribute : public ReferenceCountingClass
{
public:
	XMLAttribute();
	XMLAttribute(LPCWSTR pName, LPCWSTR pValue);
	virtual ~XMLAttribute();

	LPWSTR name;
	LPWSTR value;

};

//////////////////////////////////////////////////////////////////////
// Elements
//////////////////////////////////////////////////////////////////////
class XMLElement;
class XMLElementList
{
public:
	XMLElementList();
	virtual ~XMLElementList();

	void Add(XMLElement *pItem);
	void Remove(int nIndex);

	XMLElement *Item(int nIndex);
	XMLElement *Item(LPWSTR pName);
	int Count();

private:
	std::vector<XMLElement *> m_elements;
};

class XMLElement : public ReferenceCountingClass
{
public:
	XMLElement();
	XMLElement(LPCWSTR pName);
	virtual ~XMLElement();

	LPWSTR name;
	LPWSTR value;
	XMLAttributeList Attributes;
	XMLElementList Elements;
};

//////////////////////////////////////////////////////////////////////
// XMLDocument
//////////////////////////////////////////////////////////////////////
class XMLDocument : public LogMessageCaller
{
public:
	XMLDocument();
	virtual ~XMLDocument();

	HRESULT Load(LPWSTR filename);
	HRESULT Save(LPWSTR filename = NULL);

	XMLElementList Elements;

private:
	HRESULT ReadLine();
	HRESULT Parse();
	HRESULT ParseElement(XMLElement *pElement);
	HRESULT ParseAttribute(XMLAttribute *pAttribute);
	HRESULT ParseAttributeValue(XMLAttribute *pAttribute);
	HRESULT ParseElementData(XMLElement *pElement);
	HRESULT ParseElementEnd();

	HRESULT SaveElement(XMLElement *pElement, int depth);

	int NextToken(LPWSTR &pToken);

	LPWSTR m_filename;
	FileReader m_reader;
	FileWriter m_writer;

	LPWSTR m_pBuff;
	LPWSTR m_pLine;
	LPWSTR m_pCurr;

	enum XMLTokens
	{
		tokenNone,
		tokenTagStart,
		tokenTagEnd,
		tokenTagStartSlash,
		tokenTagEndSlash,
		tokenEquals,
		tokenQuote,
		tokenWhitespace,
		tokenCharacter,
		tokenEndOfLine
	};
};

#endif
