/**
 *	XMLDocument.cpp
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

#include "XMLDocument.h"
#include "GlobalFunctions.h"

//////////////////////////////////////////////////////////////////////
// XMLAttribute
//////////////////////////////////////////////////////////////////////
XMLAttribute::XMLAttribute()
{
	name = NULL;
	value = NULL;
}

XMLAttribute::XMLAttribute(LPCWSTR pName, LPCWSTR pValue)
{
	name = NULL;
	value = NULL;
	strCopy(name, pName);
	strCopy(value, pValue);
}

XMLAttribute::~XMLAttribute()
{
	if (name)
		delete name;
	if (value)
		delete value;
}

//////////////////////////////////////////////////////////////////////
// XMLAttributeList
//////////////////////////////////////////////////////////////////////
XMLAttributeList::XMLAttributeList()
{
}

XMLAttributeList::~XMLAttributeList()
{
	std::vector<XMLAttribute *>::iterator it = m_attributes.begin();
	for ( ; it < m_attributes.end() ; it++ )
	{
		XMLAttribute *item = *it;
		item->Release();
	}
	m_attributes.clear();
}

void XMLAttributeList::Add(XMLAttribute *pItem)
{
	m_attributes.push_back(pItem);
}

void XMLAttributeList::Remove(int nIndex)
{
	m_attributes.erase(m_attributes.begin()+nIndex);
}

XMLAttribute *XMLAttributeList::Item(int nIndex)
{
	return m_attributes.at(nIndex);
}

XMLAttribute *XMLAttributeList::Item(LPWSTR pName)
{
	std::vector<XMLAttribute *>::iterator it = m_attributes.begin();
	for ( ; it < m_attributes.end() ; it++ )
	{
		XMLAttribute *item = *it;
		if (_wcsicmp(item->name, pName) == 0)
			return item;
	}
	return NULL;
}

int XMLAttributeList::Count()
{
	return m_attributes.size();
}

//////////////////////////////////////////////////////////////////////
// XMLElement
//////////////////////////////////////////////////////////////////////
XMLElement::XMLElement()
{
	name = NULL;
	value = NULL;
}

XMLElement::XMLElement(LPCWSTR pName)
{
	name = NULL;
	value = NULL;
	strCopy(name, pName);
}

XMLElement::~XMLElement()
{
	if (name)
		delete name;
	if (value)
		delete value;
}

//////////////////////////////////////////////////////////////////////
// XMLElementList
//////////////////////////////////////////////////////////////////////
XMLElementList::XMLElementList()
{
}

XMLElementList::~XMLElementList()
{
	std::vector<XMLElement *>::iterator it = m_elements.begin();
	for ( ; it < m_elements.end() ; it++ )
	{
		XMLElement *item = *it;
		item->Release();
	}
	m_elements.clear();
}

void XMLElementList::Add(XMLElement *pItem)
{
	m_elements.push_back(pItem);
}

void XMLElementList::Remove(int nIndex)
{
	m_elements.erase(m_elements.begin()+nIndex);
}

XMLElement *XMLElementList::Item(int nIndex)
{
	if ((nIndex < 0) || (nIndex >= (int)m_elements.size()))
		return NULL;
	return m_elements.at(nIndex);
}

XMLElement *XMLElementList::Item(LPWSTR pName)
{
	std::vector<XMLElement *>::iterator it = m_elements.begin();
	for ( ; it < m_elements.end() ; it++ )
	{
		XMLElement *item = *it;
		if (_wcsicmp(item->name, pName) == 0)
			return item;
	}
	return NULL;
}

int XMLElementList::Count()
{
	return m_elements.size();
}

//////////////////////////////////////////////////////////////////////
// XMLDocument
//////////////////////////////////////////////////////////////////////
XMLDocument::XMLDocument()
{
	m_filename = NULL;
	m_pBuff = NULL;
	m_pLine = NULL;
	m_pCurr = NULL;
}

XMLDocument::~XMLDocument()
{
	if (m_pBuff)
		delete m_pBuff;
	m_pBuff = NULL;
}

HRESULT XMLDocument::Load(LPWSTR filename)
{
	(log << "Loading XML file: " << filename << "\n").Write();
	LogMessageIndent indent(&log);

	strCopy(m_filename, filename);

	HRESULT hr;
	if FAILED(hr = m_reader.Open(m_filename))
	{
		return (log << "Could not open file: " << m_filename << " : " << hr << "\n").Write(hr);
	}


	try
	{
		hr = Parse();
	}
	catch (LPWSTR str)
	{
		(log << "Error caught reading XML file: " << str << "\n").Show();
		m_reader.Close();
		return E_FAIL;
	}
	m_reader.Close();

	indent.Release();
	(log << "Finished Loading XML file: " << filename << "\n").Write();

	return hr;
}

HRESULT XMLDocument::ReadLine()
{
	HRESULT hr;

	if (m_pBuff)
		delete m_pBuff;
	m_pBuff = NULL;

	hr = m_reader.ReadLine(m_pLine);
	if (hr != S_OK)
		return hr;
	while (TRUE)
	{
		LPWSTR pFirst = m_pLine;
		LPWSTR pCommentStart = wcsstr(m_pLine, L"<!--");
		if (!pCommentStart)
		{
			return S_OK;
		}
		int firstLength = pCommentStart - pFirst;

		LPWSTR pLookFrom = pCommentStart+4;
		LPWSTR pCommentEnd;

		while (TRUE)
		{
			pCommentEnd = wcsstr(pLookFrom, L"-->");
			if (pCommentEnd)
				break;
			if (pFirst == m_pLine)
			{
				pFirst = NULL;
				strCopy(pFirst, m_pLine, firstLength);
			}
			hr = m_reader.ReadLine(m_pLine);
			if (hr != S_OK)
				return hr;
			pLookFrom = m_pLine;
		}

		pCommentEnd += 3;

		LPWSTR newBuff = new wchar_t[firstLength + wcslen(pCommentEnd) + 1];
		memcpy(newBuff, pFirst, firstLength * sizeof(wchar_t));
		memcpy(newBuff + firstLength, pCommentEnd, (wcslen(pCommentEnd)+1)*sizeof(wchar_t));

		if (m_pBuff)
			delete m_pBuff;
		m_pBuff = newBuff;

		if (pFirst != m_pLine)
			delete pFirst;

		m_pLine = m_pBuff;
	}
	return E_FAIL;
}

HRESULT XMLDocument::Parse()
{
	HRESULT hr;

	m_pCurr = L"";
	LPWSTR pToken = m_pCurr;

	BOOL bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenWhitespace:
			skipWhitespaces(pToken);
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
			{
				bLoop = FALSE;
				break;
			}
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		case tokenTagStart:
			{
				XMLElement *pElement = new XMLElement();
				m_pCurr = pToken;
				hr = ParseElement(pElement);
				pToken = m_pCurr;
				if (hr == S_OK)
					Elements.Add(pElement);
			}
			break;

		default:
			return E_FAIL;
		};
	}
	return S_OK;
}

HRESULT XMLDocument::ParseElement(XMLElement *pElement)
{
	HRESULT hr;

	m_pCurr += 1;

	LPWSTR pStart = m_pCurr;
	LPWSTR pToken = m_pCurr;
	BOOL bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenCharacter:
			pToken += 1;
			break;

		case tokenWhitespace:
		case tokenTagEnd:
		case tokenTagEndSlash:
			bLoop = FALSE;
			break;

		case tokenEndOfLine:
			//TODO: copy existing line data to element name
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				return hr;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		default:
			return E_FAIL;
		};
	}

	strCopy(pElement->name, pStart, pToken-pStart);

	m_pCurr = pToken;

	bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenWhitespace:
			skipWhitespaces(pToken);
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				break;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		case tokenTagEnd:
			m_pCurr = pToken+1;
			ParseElementData(pElement);
			pToken = m_pCurr;
			bLoop = FALSE;
			return S_OK;
			//break;

		case tokenTagEndSlash:
			m_pCurr = pToken + 2;
			return S_OK;

		case tokenCharacter:
			{
				XMLAttribute *pAttribute = new XMLAttribute();
				m_pCurr = pToken;
				if FAILED(hr = ParseAttribute(pAttribute))
					return hr;
				if (hr == S_OK)
				{
					pElement->Attributes.Add(pAttribute);
					pToken = m_pCurr;
					continue;
				}
			}
			break;

		default:
			return E_FAIL;
		};
	}

	return E_FAIL;
}

HRESULT XMLDocument::ParseAttribute(XMLAttribute *pAttribute)
{
	HRESULT hr;

	LPWSTR pStart = m_pCurr;
	LPWSTR pToken = m_pCurr;
	BOOL bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenCharacter:
			pToken += 1;
			break;

		case tokenWhitespace:
		case tokenTagEnd:
		case tokenTagEndSlash:
		case tokenEquals:
			bLoop = FALSE;
			break;

		case tokenEndOfLine:
			//TODO: copy existing line data to attribute name
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				return hr;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		default:
			return E_FAIL;
		}
	}

	strCopy(pAttribute->name, pStart, pToken-pStart);

	m_pCurr = pToken;

	bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenWhitespace:
			skipWhitespaces(pToken);
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				break;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		case tokenEquals:
			m_pCurr = pToken + 1;
			ParseAttributeValue(pAttribute);
			return S_OK;

		case tokenCharacter:
		case tokenTagEnd:
		case tokenTagEndSlash:
			m_pCurr = pToken;
			strCopy(pAttribute->value, L"");
			return S_OK;

		default:
			return E_FAIL;
		};
	}

	return E_FAIL;
}

HRESULT XMLDocument::ParseAttributeValue(XMLAttribute *pAttribute)
{
	HRESULT hr;
	BOOL bInQuote = FALSE;

	skipWhitespaces(m_pCurr);
	if (m_pCurr[0] == '"')
	{
		m_pCurr += 1;
		bInQuote = TRUE;
	}
	LPWSTR pStart = m_pCurr;
	LPWSTR pToken = m_pCurr;


	BOOL bLoop = TRUE;
	while (bLoop)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenCharacter:
			pToken += 1;
			break;

		case tokenWhitespace:
		case tokenTagEnd:
		case tokenTagEndSlash:
		case tokenEquals:
			if (!bInQuote)
				bLoop = FALSE;
			else
				pToken += 1;
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				return hr;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		case tokenQuote:
			if (bInQuote)
				bLoop = FALSE;
			else
				pToken += 1;
			break;

		default:
			return E_FAIL;
		}
	}

	strCopy(pAttribute->value, pStart, pToken-pStart);

	m_pCurr = pToken;
	if (bInQuote)
		m_pCurr += 1;

	return S_OK;
}

HRESULT XMLDocument::ParseElementData(XMLElement *pElement)
{
	HRESULT hr;
	LPWSTR pToken = m_pCurr;
	LPWSTR pStart = pToken;

	while (TRUE)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenEndOfLine:
		case tokenTagStart:
		case tokenTagStartSlash:
			if (pToken-pStart > 0)
			{
				if (pElement->value)
				{
					long newLength = wcslen(pElement->value);
					newLength += pToken-pStart;
					LPWSTR newBuff = new wchar_t[newLength + 1];
					memcpy(newBuff, pElement->value, wcslen(pElement->value) * sizeof(wchar_t));
					memcpy(newBuff + wcslen(pElement->value), pStart, (pToken-pStart) * sizeof(wchar_t));
					newBuff[newLength] = '\0';
					delete pElement->value;
					pElement->value = newBuff;
				}
				else
				{
					strCopy(pElement->value, pStart, pToken-pStart);
				}
			}
			break;
		};

		switch (token)
		{
		case tokenWhitespace:
			skipWhitespaces(pToken);
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				break;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			pStart = pToken;
			break;

		case tokenTagStart:
			{
				XMLElement *pSubElement = new XMLElement();
				m_pCurr = pToken;
				hr = ParseElement(pSubElement);
				pToken = m_pCurr;
				pStart = pToken;
				pElement->Elements.Add(pSubElement);
			}
			break;

		case tokenTagStartSlash:
			m_pCurr = pToken;
			ParseElementEnd();
			return S_OK;

		default:
			pToken +=1;
			break;
		};
	}
	return S_OK;
}

HRESULT XMLDocument::ParseElementEnd()
{
	HRESULT hr;
	m_pCurr += 2;

	LPWSTR pToken = m_pCurr;

	while (TRUE)
	{
		int token = NextToken(pToken);

		switch (token)
		{
		case tokenWhitespace:
			skipWhitespaces(pToken);
			break;

		case tokenEndOfLine:
			if FAILED(hr = ReadLine())
				return hr;
			if (hr == S_FALSE)
				break;
			m_pCurr = m_pLine;
			pToken = m_pCurr;
			break;

		case tokenTagEnd:
			m_pCurr = pToken + 1;
			return S_OK;

		case tokenCharacter:
			pToken += 1;
			break;

		default:
			break;
		};
	}
	return S_OK;
}

int XMLDocument::NextToken(LPWSTR &pToken)
{
//	static int lastToken = tokenNone;
	int result = tokenNone;

	int length = wcslen(pToken);
	LPWSTR pCurr = pToken;
	for ( int i=0 ; i<=length ; i++ )
	{
		pToken = pCurr + i;

		if (pCurr[i] == '\0')
		{
			result = tokenEndOfLine;
			break;
		}

		if ((i < length) && (pCurr[i] == '<') && (pCurr[i+1] == '/'))
		{
			result = tokenTagStartSlash;
			break;
		}

		if ((i < length) && (pCurr[i] == '/') && (pCurr[i+1] == '>'))
		{
			result = tokenTagEndSlash;
			break;
		}

		if (pCurr[i] == '<')
		{
			result = tokenTagStart;
			break;
		}

		if (pCurr[i] == '>')
		{
			result = tokenTagEnd;
			break;
		}

		if (pCurr[i] == '=')
		{
			result = tokenEquals;
			break;
		}

		if (pCurr[i] == '"')
		{
			result = tokenQuote;
			break;
		}

		if /*((lastToken != tokenWhitespace) && */(isWhitespace(pCurr[i]))/*)*/
		{
			result = tokenWhitespace;
			break;
		}

/*		if (lastToken != tokenCharacter)
		{*/
			result = tokenCharacter;
			break;
/*		}*/
	}

	return result;
}

HRESULT XMLDocument::Save(LPWSTR filename)
{
	HRESULT hr;

	if (filename == NULL)
		filename = m_filename;

	(log << "Saving XML file: " << filename << "\n").Write();
	LogMessageIndent indent(&log);

	if FAILED(hr = m_writer.Open(filename))
		return (log << "Could not open file: " << filename << " : " << hr << "\n").Write(hr);

	try
	{
		for ( int item=0 ; item < Elements.Count() ; item++ )
		{
			SaveElement(Elements.Item(item), 0);
		}
	}
	catch (LPWSTR str)
	{
		(log << "Error caught saving XML file: " << str << "\n").Show();
		m_writer.Close();
		return E_FAIL;
	}
	m_writer.Close();
	return hr;
}

HRESULT XMLDocument::SaveElement(XMLElement *pElement, int depth)
{
	int i;
	for ( i=0 ; i<depth ; i++ )
		m_writer << "\t";

	m_writer << "<" << pElement->name;
	for ( i=0 ; i<pElement->Attributes.Count() ; i++ )
	{
		m_writer << " " << pElement->Attributes.Item(i)->name << "=\"" << pElement->Attributes.Item(i)->value << "\"";
	}

	if (pElement->Elements.Count() > 0)
	{
		m_writer << ">" << m_writer.EOL;
		for ( i=0 ; i<pElement->Elements.Count() ; i++ )
		{
			SaveElement(pElement->Elements.Item(i), depth+1);
		}
		m_writer << "</>" << m_writer.EOL;
	}
	else
	{
		m_writer << " />" << m_writer.EOL;
	}

	return S_OK;
}