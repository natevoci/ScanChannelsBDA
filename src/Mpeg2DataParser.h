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
#include <vector>

#define AUDIO_CHAN_MAX (32)
#define CA_SYSTEM_ID_MAX (16)

enum running_mode {
	RM_NOT_RUNNING = 0x01,
	RM_STARTS_SOON = 0x02,
	RM_PAUSING     = 0x03,
	RM_RUNNING     = 0x04
};

struct section_buf
{
	unsigned int run_once  : 1;
	unsigned int segmented : 1;	/* segmented by table_id_ext */
	int fd;
	int pid;
	int table_id;
	int table_id_ext;
	int section_version_number;
	__int8 section_done[32];
	int sectionfilter_done;
	unsigned char *buf;
	time_t timeout;
	time_t start_time;
	time_t running_time;
	struct section_buf *next_seg;	/* this is used to handle
									 * segmented tables (like NIT-other)
									 */
};

struct service
{
	int transport_stream_id;
	int service_id;
	unsigned char *provider_name;
	unsigned char *service_name;
	__int16 pmt_pid;
	__int16 pcr_pid;
	__int16 video_pid;
	__int16 audio_pid[AUDIO_CHAN_MAX];
	char audio_lang[AUDIO_CHAN_MAX][4];
	int audio_num;
	__int16 ca_id[CA_SYSTEM_ID_MAX];
	int ca_num;
	__int16 teletext_pid;
	__int16 subtitling_pid;
	__int16 ac3_pid;
	unsigned int type         : 8;
	unsigned int scrambled	  : 1;
	enum running_mode running;
	struct section_buf *priv;
	int channel_num;
};

struct transponder {
	vector<service *> services;
	int network_id;
	int transport_stream_id;
	int original_network_id;
	enum fe_type type;
	//struct dvb_frontend_parameters param;
	enum polarisation polarisation;		/* only for DVB-S */
	int orbital_pos;			/* only for DVB-S */
	unsigned int we_flag		  : 1;	/* West/East Flag - only for DVB-S */
	unsigned int scan_done		  : 1;
	unsigned int last_tuning_failed	  : 1;
	unsigned int other_frequency_flag : 1;	/* DVB-T */
	int n_other_f;
	__int32 *other_f;			/* DVB-T freqeuency-list descriptor */
	unsigned char *network_name;
	int frequency;
	int bandwidth;
};

static int get_bit (__int8 *bitfield, int bit)
{
	return (bitfield[bit/8] >> (bit % 8)) & 1;
}

static void set_bit (__int8 *bitfield, int bit)
{
	bitfield[bit/8] |= 1 << (bit % 8);
}


class Mpeg2DataParser  
{
public:
	Mpeg2DataParser();
	virtual ~Mpeg2DataParser();

	void SetNetworkNumber(int network);
	void SetFilter(CComPtr <IBaseFilter> pBDASecTab);
	void ReleaseFilter();

	void StartMpeg2DataScan();
	void StartMpeg2DataScanThread();

	void Reset();
	void WaitForScanToFinish(DWORD timeout);

	void PrintDigitalWatchChannelsIni();

	void SetVerbose(BOOL verb) { verbose = verb; }

private:
	void SetupFilter (struct section_buf* s, int pid, int tid, int run_once, int segmented, int timeout);
	void AddFilter (struct section_buf *s);
	void ReadFilters(void);
	void ReadSection(struct section_buf *s);

	struct transponder *alloc_transponder(int transport_stream_id);
	struct transponder *find_transponder(int transport_stream_id);
	struct service *alloc_service(struct transponder *tp, int service_id);
	struct service *find_service(struct transponder *tp, int service_id);

	int parse_section (struct section_buf *s);
	void parse_pat(const unsigned char *buf, int section_length, int transport_stream_id);
	void parse_pmt (const unsigned char *buf, int section_length, int service_id);
	void parse_nit (const unsigned char *buf, int section_length, int network_id);
	void parse_sdt (const unsigned char *buf, int section_length, int transport_stream_id);
	
	void parse_descriptorsPMT(const unsigned char *buf, int remaining_length, struct service *s);
	void parse_descriptorsNIT(const unsigned char *buf, int remaining_length, struct transponder *tp);
	void parse_descriptorsSDT(const unsigned char *buf, int remaining_length, struct service *s);

	void parse_iso639_language_descriptor (const unsigned char *buf, struct service *s);
	void parse_network_name_descriptor (const unsigned char *buf, struct transponder *tp);
	void print_unknown_descriptor(const unsigned char *buf, int descriptor_len);
	void parse_terrestrial_delivery_system_descriptor (const unsigned char *buf, struct transponder *t);
	void parse_frequency_list_descriptor (const unsigned char *buf, struct transponder *t);
	void parse_terrestrial_uk_channel_number (const unsigned char *buf, struct transponder *t);
	void parse_service_descriptor (const unsigned char *buf, struct service *s);
	int find_descriptor(__int8 tag, const unsigned char *buf, int remaining_length, const unsigned char **desc, int *desc_len);

	
	void ParseMpeg2Section(SECTION *pSection, DWORD dwPacketLength);
	void ParsePAT(const BYTE *buf, int remainingLength);
	void ParsePMT(const BYTE *buf, int remainingLength);
	
	void PrintByteArray(const BYTE *pData, long cbSize);
	void PrintWordArray(const BYTE *pData, long cbSize);
	void PaddingForNumber(long number, long totalWidth);

	void DeleteTransponder(struct transponder *t);
	void DeleteService(struct service *s);

	CComPtr <IMpeg2Data> m_piIMpeg2Data;

	HANDLE m_hScanningDoneEvent;

	int long_timeout;
	int m_networkNumber;

	BOOL verbose;

	struct transponder *  current_tp;
	vector<transponder *> transponders;

	vector<section_buf *> waiting_filters;
};

#endif
