/**
 *	Mpeg2DataParser.h
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

#ifndef MPEG2DATAPARSER_H
#define MPEG2DATAPARSER_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <mpeg2data.h>
#include <mpeg2bits.h>

class Mpeg2DataParser  
{
public:
	Mpeg2DataParser();
	virtual ~Mpeg2DataParser();

	void SetNetworkNumber(int network);
	void SetFilter(CComPtr <IBaseFilter> pBDASecTab);

	void StartMpeg2DataScan();
	void StartMpeg2DataScanThread();

	void Reset();
	void WaitForScanToFinish(DWORD timeout);

	BOOL IsVerbose() { return verbose; }
	void ToggleVerbose() { verbose = (verbose ? 0 : 1); }

private:
	void SetupFilter (struct section_buf* s, int pid, int tid, int run_once, int segmented, int timeout);
	void AddFilter (struct section_buf *s);
	void ReadFilters(void);
	void ReadSection(struct section_buf *s);

	struct service *alloc_service(struct transponder *tp, int service_id);
	struct service *find_service(struct transponder *tp, int service_id);
	void parse_iso639_language_descriptor (const unsigned char *buf, struct service *s);
	void parse_network_name_descriptor (const unsigned char *buf);
	void print_unknown_descriptor(const unsigned char *buf, int descriptor_len);
	void parse_terrestrial_delivery_system_descriptor (const unsigned char *buf, struct transponder *t);
	void parse_frequency_list_descriptor (const unsigned char *buf);
	void parse_terrestrial_uk_channel_number (const unsigned char *buf);
	void parse_service_descriptor (const unsigned char *buf, struct service *s);
	int find_descriptor(__int8 tag, const unsigned char *buf, int remaining_length, const unsigned char **desc, int *desc_len);
	void parse_descriptorsPMT(const unsigned char *buf, int remaining_length, struct service *data);
	void parse_descriptorsNIT(const unsigned char *buf, int remaining_length, struct transponder *data);
	void parse_descriptorsSDT(const unsigned char *buf, int remaining_length, struct service *data);
	void parse_pat(const unsigned char *buf, int section_length, int transport_stream_id);
	void parse_pmt (const unsigned char *buf, int section_length, int service_id);
	void parse_nit (const unsigned char *buf, int section_length, int network_id);
	void parse_sdt (const unsigned char *buf, int section_length, int transport_stream_id);
	int parse_section (struct section_buf *s);
	int read_sections (struct section_buf *s);

	
	void ParseMpeg2Section(SECTION *pSection, DWORD dwPacketLength);
	void ParsePAT(const BYTE *buf, int remainingLength);
	void ParsePMT(const BYTE *buf, int remainingLength);
	
	void PrintByteArray(const BYTE *pData, long cbSize);
	void PrintWordArray(const BYTE *pData, long cbSize);
	void PaddingForNumber(long number, long totalWidth);

	CComPtr <IMpeg2Data> m_piIMpeg2Data;

	HANDLE m_hScanningDoneEvent;

	int long_timeout;
	int m_networkNumber;

	BOOL verbose;
};

#endif
