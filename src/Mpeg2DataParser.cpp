/**
 *	Mpeg2DataParser.cpp
 *	Copyright (C) 2004 Nate
 *
 *  significant portions of this code are taken from 
 *  the linuxtv-dvb-apps-1.1.0 scan.c file
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

#include "stdafx.h"
#include "Mpeg2DataParser.h"
#include "list.h"

#include <process.h>
#include <math.h>

enum table_type {
	PAT,
	PMT,
	SDT,
	NIT
};

enum running_mode {
	RM_NOT_RUNNING = 0x01,
	RM_STARTS_SOON = 0x02,
	RM_PAUSING     = 0x03,
	RM_RUNNING     = 0x04
};

#define AUDIO_CHAN_MAX (32)
#define CA_SYSTEM_ID_MAX (16)

struct section_buf
{
	struct list_head list;
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
	struct list_head list;
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
	struct list_head list;
	struct list_head services;
	int network_id;
	int transport_stream_id;
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

static struct transponder *current_tp;


static int get_bit (__int8 *bitfield, int bit)
{
	return (bitfield[bit/8] >> (bit % 8)) & 1;
}

static void set_bit (__int8 *bitfield, int bit)
{
	bitfield[bit/8] |= 1 << (bit % 8);
}



static LIST_HEAD(running_filters);
static LIST_HEAD(waiting_filters);
#define MAX_RUNNING 32
static struct section_buf* poll_section_bufs[MAX_RUNNING];



void ParseMpeg2DataThread( void *pParam )
{
	Mpeg2DataParser *scanner;
	scanner = (Mpeg2DataParser *)pParam;
	scanner->StartMpeg2DataScanThread();
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Mpeg2DataParser::Mpeg2DataParser()
{
	m_hScanningDoneEvent = CreateEvent(NULL, TRUE, FALSE, "ScanningDone");
	m_piIMpeg2Data = NULL;

	long_timeout = 0;

	m_networkNumber = 0;

	verbose = FALSE;
}

Mpeg2DataParser::~Mpeg2DataParser()
{
	if (m_piIMpeg2Data != NULL)
		m_piIMpeg2Data.Release();
}

void Mpeg2DataParser::SetNetworkNumber(int network)
{
	m_networkNumber = network;
}

void Mpeg2DataParser::SetFilter(CComPtr <IBaseFilter> pBDASecTab)
{
	if (m_piIMpeg2Data != NULL)
		m_piIMpeg2Data.Release(); 

	if (pBDASecTab != NULL) 
	{
		HRESULT hr = pBDASecTab->QueryInterface(__uuidof(IMpeg2Data), reinterpret_cast<void**>(&m_piIMpeg2Data));
	}
}

//////////////////////////////////////////////////////////////////////
// Event stuff
//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::Reset()
{
	ResetEvent(m_hScanningDoneEvent);
}

void Mpeg2DataParser::WaitForScanToFinish(DWORD timeout)
{
	WaitForSingleObject(m_hScanningDoneEvent, timeout);
}

//////////////////////////////////////////////////////////////////////
// Method just to kick off a separate thread
//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::StartMpeg2DataScan()
{
	//Start a new thread to do the scanning because the 
	//IGuideDataEvent::ServiceChanged method isn't allowed to block.
	DWORD dwThreadId = 0;
	unsigned long result = _beginthread(ParseMpeg2DataThread, 0, (void *) this);
}

//////////////////////////////////////////////////////////////////////
// The real work starts here
//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::StartMpeg2DataScanThread()
{
	HRESULT hr = S_OK;

	current_tp = (struct transponder *)calloc(1, sizeof(*current_tp));
	INIT_LIST_HEAD(&current_tp->list);
	INIT_LIST_HEAD(&current_tp->services);

	if (m_piIMpeg2Data != NULL) 
	{
		struct section_buf s0;
		SetupFilter(&s0, 0x00, 0x00, 1, 0, 5);  // PAT
		AddFilter(&s0);

		struct section_buf s2;
		SetupFilter(&s2, 0x10, 0x40, 1, 0, 15); // NIT
		AddFilter(&s2);

		struct section_buf s1;
		SetupFilter(&s1, 0x11, 0x42, 1, 0, 5);  // SDT
		AddFilter(&s1);

		do
		{
			ReadFilters ();
		} while (!(list_empty(&running_filters) && list_empty(&waiting_filters)));

		struct list_head *pos;
		struct service *s;

		printf("\nNetwork_%i(\"%s\", %i, %i, 1)\n", m_networkNumber, current_tp->network_name, current_tp->frequency, current_tp->bandwidth);

		int i=1;
		list_for_each(pos, &current_tp->services)
		{
			s = list_entry(pos, struct service, list);

			for (int audio = 0 ; audio <= s->audio_num ; audio++ )
			{
				if (((audio < s->audio_num) && (s->audio_pid[audio])) || (s->ac3_pid))
				{
					printf("  Program_");
					if (i < 10) printf(" ");
					printf("%i(\"%s", i, s->service_name);

					int len = strlen((char *)s->service_name);
					if ((audio > 0) && (audio == s->audio_num))
					{
						printf(" AC3");
						len += 4;
					}
					printf("\"");
					
					while ( len < 20 ) { printf(" "); len++; }
					printf(", ");

					PaddingForNumber(s->service_id, 4);
					printf("%i, ",	s->service_id);

					PaddingForNumber(s->video_pid, 4);
					printf("%i, ", s->video_pid);

					if (audio < s->audio_num)
					{
						PaddingForNumber(s->audio_pid[audio], 5);
						printf("%i, ", s->audio_pid[audio]);
					}
					else
					{
						PaddingForNumber(s->ac3_pid, 4);
						printf("A%i, ", s->ac3_pid);
					}
					
					PaddingForNumber(s->pmt_pid, 4);
					printf("%i)", s->pmt_pid);

					printf("    # %i", s->channel_num);

					printf("\n");

					i++;
				}
			}
		}
		printf("\n");
	}
	SetEvent(m_hScanningDoneEvent);
} 

//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::SetupFilter (struct section_buf* s, int pid, int tid, int run_once, int segmented, int timeout)
{
	memset (s, 0, sizeof(struct section_buf));

	s->fd = -1;
	s->pid = pid;
	s->table_id = tid;

	s->run_once = run_once;
	s->segmented = segmented;

	if (long_timeout)
		s->timeout = 5 * timeout;
	else
		s->timeout = timeout;

	s->table_id_ext = -1;
	s->section_version_number = -1;

	INIT_LIST_HEAD (&s->list);
}

void Mpeg2DataParser::AddFilter (struct section_buf *s)
{
//	printf("add filter pid 0x%04x\n", s->pid);
//	if (start_filter (s))
		list_add_tail (&s->list, &waiting_filters);
}

void Mpeg2DataParser::ReadFilters(void)
{
	struct section_buf *s;

	while (!list_empty(&waiting_filters))
	{
		s = list_entry (waiting_filters.next, struct section_buf, list);
		ReadSection(s);
		list_del(&s->list);
		//free (s);
	}
}





//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::ReadSection(struct section_buf *s)
{
	PID pid = s->pid;
	TID tid = s->table_id;
	DWORD dwTimeout = s->timeout * 1000;
	if (m_piIMpeg2Data != NULL)
	{
		HRESULT hr;
		ISectionList* piSectionList = NULL;

		if (SUCCEEDED(hr = m_piIMpeg2Data->GetTable(pid, tid, NULL, dwTimeout, &piSectionList)))
		{
			WORD cSections;
			if (SUCCEEDED(hr = piSectionList->GetNumberOfSections(&cSections)))
			{
				
				if (verbose) printf(_T("\n%d section(s) found for pid=%.4x, tid=%.2x\n"), cSections, pid, tid); 
			
				for (WORD i = 0; i < cSections; i++)
				{
					// Iterate through the list of sections.
					SECTION* pstSection;
					DWORD    ulSize;
					
					hr = piSectionList->GetSectionData(i, &ulSize, &pstSection);
					
					if (SUCCEEDED(hr))
					{
						s->buf = (unsigned char *)pstSection;
						parse_section(s);
					}
				}
			}
			else
			{
				printf(_T("Error 0x%x getting number of sections\n"), hr); 
			}
			
			piSectionList->Release();
		}
		else
		{
			printf(_T("Timeout getting table (pid=%.4x, tid=%.2x)\n"), pid, tid);
		}
	} 
}


/* service_ids are guaranteed to be unique within one TP
 * (the DVB standards say theay should be unique within one
 * network, but in real life...)
 */
struct service *Mpeg2DataParser::alloc_service(struct transponder *tp, int service_id)
{
	struct service *s = (struct service *)calloc(1, sizeof(*s));
	INIT_LIST_HEAD(&s->list);
	s->service_id = service_id;
	list_add_tail(&s->list, &tp->services);
	return s;
}

struct service *Mpeg2DataParser::find_service(struct transponder *tp, int service_id)
{
	struct list_head *pos;
	struct service *s;

	list_for_each(pos, &tp->services) {
		s = list_entry(pos, struct service, list);
		if (s->service_id == service_id)
			return s;
	}
	return NULL;
}

void Mpeg2DataParser::parse_iso639_language_descriptor (const unsigned char *buf, struct service *s)
{
	unsigned char len = buf [1];

	buf += 2;

	if (len >= 4)
	{
		if (verbose) printf("    LANG=%.3s %d\n", buf, buf[3]);
		memcpy(s->audio_lang[s->audio_num], buf, 3);
#if 0
		/* seems like the audio_type is wrong all over the place */
		//if (buf[3] == 0) -> normal
		if (buf[3] == 1)
			s->audio_lang[s->audio_num][3] = '!'; /* clean effects (no language) */
		else if (buf[3] == 2)
			s->audio_lang[s->audio_num][3] = '?'; /* for the hearing impaired */
		else if (buf[3] == 3)
			s->audio_lang[s->audio_num][3] = '+'; /* visually impaired commentary */
#endif
	}
}

void Mpeg2DataParser::parse_network_name_descriptor (const unsigned char *buf)
{
	if (current_tp->network_name)
		free (current_tp->network_name);

	unsigned char len = buf [1];

	current_tp->network_name = (unsigned char *)malloc (len + 1);
	memcpy (current_tp->network_name, buf+2, len);
	current_tp->network_name[len] = '\0';

	if (verbose) printf("    Network Name '%.*s'\n", len, buf + 2);
}

void Mpeg2DataParser::print_unknown_descriptor(const unsigned char *buf, int descriptor_len)
{
	PrintByteArray(buf, descriptor_len);
}

void Mpeg2DataParser::parse_terrestrial_delivery_system_descriptor (const unsigned char *buf, struct transponder *t)
{
	//static const fe_modulation_t m_tab [] = { QPSK, QAM_16, QAM_64, QAM_AUTO };
	//static const fe_code_rate_t ofec_tab [8] = { FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8 };
	//struct dvb_ofdm_parameters *o;

	if (!t) {
		if (verbose) printf("    terrestrial_delivery_system_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	//o = &t->param.u.ofdm;
	//t->type = FE_OFDM;

	current_tp->frequency = (buf[2] << 24) | (buf[3] << 16);
	current_tp->frequency |= (buf[4] << 8) | buf[5];
	current_tp->frequency /= 100;
	if (verbose) printf("    Frequency %i", current_tp->frequency);
	//t->param.inversion = spectral_inversion;

	//o->bandwidth = BANDWIDTH_8_MHZ + ((buf[6] >> 5) & 0x3);
	current_tp->bandwidth = 8 - ((buf[6] >> 5) & 0x3);
	if (verbose) printf("    Bandwidth %i", current_tp->bandwidth);

	//o->constellation = m_tab[(buf[7] >> 6) & 0x3];
	switch ((buf[7] >> 6) & 0x3)
	{
	case 0:
		if (verbose) printf("    QPSK\n");
		break;
	case 1:
		if (verbose) printf("    QAM_16\n");
		break;
	case 2:
		if (verbose) printf("    QAM_64\n");
		break;
	case 3:
		if (verbose) printf("    QAM_AUTO\n");
		break;
	}

	//o->hierarchy_information = HIERARCHY_NONE + ((buf[7] >> 3) & 0x3);

	//DWORD coderateHP = buf[7] & 0x7;
	switch (buf[7] & 0x7)
	{
	case 0:
		if (verbose) printf("    HP - FEC_1_2");
		break;
	case 1:
		if (verbose) printf("    HP - FEC_2_3");
		break;
	case 2:
		if (verbose) printf("    HP - FEC_3_4");
		break;
	case 3:
		if (verbose) printf("    HP - FEC_5_6");
		break;
	case 4:
		if (verbose) printf("    HP - FEC_7_8");
		break;
	default:
		if (verbose) printf("    HP - FEC_AUTO");
		break;
	}

	//if (((buf[8] >> 5) & 0x7) > 4)
	//DWORD coderateLP = (buf[8] >> 5) & 0x7;
	switch ((buf[8] >> 5) & 0x7)
	{
	case 0:
		if (verbose) printf("    LP - FEC_1_2");
		break;
	case 1:
		if (verbose) printf("    LP - FEC_2_3");
		break;
	case 2:
		if (verbose) printf("    LP - FEC_3_4");
		break;
	case 3:
		if (verbose) printf("    LP - FEC_5_6");
		break;
	case 4:
		if (verbose) printf("    LP - FEC_7_8");
		break;
	default:
		if (verbose) printf("    LP - FEC_AUTO");
		break;
	}

	//o->guard_interval = GUARD_INTERVAL_1_32 + ((buf[8] >> 3) & 0x3);

	//o->transmission_mode = (buf[8] & 0x2) ?
	//		       TRANSMISSION_MODE_8K :
	//		       TRANSMISSION_MODE_2K;
	switch (buf[8] & 0x2)
	{
	case 0:
		if (verbose) printf("    Transmission mode 2K\n");
		break;
	default:
		if (verbose) printf("    Transmission mode 8K\n");
		break;
	}

	//t->other_frequency_flag = (buf[8] & 0x01);
	DWORD other_freq = (buf[8] & 0x01);

	//if (verbosity >= 5) {
	//	debug("0x%#04x/0x%#04x ", t->network_id, t->transport_stream_id);
	//	dump_dvb_parameters (stderr, t);
	//	if (t->scan_done)
	//		dprintf(5, " (done)");
	//	if (t->last_tuning_failed)
	//		dprintf(5, " (tuning failed)");
	//	dprintf(5, "\n");
	//}
}

void Mpeg2DataParser::parse_frequency_list_descriptor (const unsigned char *buf)
{
	int n, i;

	n = (buf[1] - 1) / 4;
	if (n < 1 || (buf[2] & 0x03) != 3)
		return;

	buf += 3;
	for (i = 0; i < n; i++) {
		DWORD f = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		f /= 100;
		if (verbose) printf("    Alternate Frequency %i\n", f);
		buf += 4;
	}
}

void Mpeg2DataParser::parse_terrestrial_uk_channel_number (const unsigned char *buf)
{
	int i, n, channel_num, service_id;
	//struct list_head *p1;
	struct list_head *p2;
	//struct transponder *t;
	struct service *s;

	// 32 bits per record
	n = buf[1] / 4;
	if (n < 1)
		return;

	// desc id, desc len, (service id, service number)
	buf += 2;
	for (i = 0; i < n; i++) {
		service_id = (buf[0]<<8)|(buf[1]&0xff);
		channel_num = (buf[2]&0x03<<8)|(buf[3]&0xff);
		if (verbose) printf("    Service ID 0x%x has channel number %d\n", service_id, channel_num);
//		list_for_each(p1, &scanned_transponders) {
//			t = list_entry(p1, struct transponder, list);
			list_for_each(p2, &current_tp->services)
			{
				s = list_entry(p2, struct service, list);
				if (s->service_id == service_id)
					s->channel_num = channel_num;
			}
//		}
		buf += 4;
	}
}


void Mpeg2DataParser::parse_service_descriptor (const unsigned char *buf, struct service *s)
{
	unsigned char len;
	unsigned char *src, *dest;

	s->type = buf[2];

	buf += 3;
	len = *buf;
	buf++;

	if (s->provider_name)
		free (s->provider_name);

	s->provider_name = (unsigned char *)malloc (len + 1);
	memcpy (s->provider_name, buf, len);
	s->provider_name[len] = '\0';

	/* remove control characters (FIXME: handle short/long name) */
	/* FIXME: handle character set correctly (e.g. via iconv)
	 * c.f. EN 300 468 annex A */
	for (src = dest = s->provider_name; *src; src++)
		if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
			*dest++ = *src;
	*dest = '\0';
	if (!s->provider_name[0]) {
		/* zap zero length names */
		free (s->provider_name);
		s->provider_name = 0;
	}

	if (s->service_name)
		free (s->service_name);

	buf += len;
	len = *buf;
	buf++;

	s->service_name = (unsigned char *)malloc (len + 1);
	memcpy (s->service_name, buf, len);
	s->service_name[len] = '\0';

	/* remove control characters (FIXME: handle short/long name) */
	/* FIXME: handle character set correctly (e.g. via iconv)
	 * c.f. EN 300 468 annex A */
	for (src = dest = s->service_name; *src; src++)
		if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
			*dest++ = *src;
	*dest = '\0';
	if (!s->service_name[0]) {
		/* zap zero length names */
		free (s->service_name);
		s->service_name = 0;
	}

	if (verbose) 
		printf("  0x%04x 0x%04x: pmt_pid 0x%04x %s -- %s (%s%s)\n",
			s->transport_stream_id,
			s->service_id,
			s->pmt_pid,
			s->provider_name, s->service_name,
			s->running == RM_NOT_RUNNING ? "not running" :
			s->running == RM_STARTS_SOON ? "starts soon" :
			s->running == RM_PAUSING     ? "pausing" :
			s->running == RM_RUNNING     ? "running" : "???",
			s->scrambled ? ", scrambled" : "");
}

int Mpeg2DataParser::find_descriptor(__int8 tag, const unsigned char *buf, int remaining_length, const unsigned char **desc, int *desc_len)
{
	while (remaining_length > 0) {
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len) {
			if (verbose) printf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		if (tag == descriptor_tag) {
			if (desc)
				*desc = buf;
			if (desc_len)
				*desc_len = descriptor_len;
			return 1;
		}

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
	return 0;
}

void Mpeg2DataParser::parse_descriptorsPMT(const unsigned char *buf, int remaining_length, struct service *data)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			if (verbose) printf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x0a:
				parse_iso639_language_descriptor (buf, data);
				break;

			default:
				if (verbose) printf("    skip descriptor 0x%02x\n", descriptor_tag);
				print_unknown_descriptor(buf, descriptor_len);
		};

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
}

void Mpeg2DataParser::parse_descriptorsNIT(const unsigned char *buf, int remaining_length, struct transponder *data)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			if (verbose) printf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x40:
				if (verbose) printf("  Found a network name descriptor\n");
				parse_network_name_descriptor (buf);
				break;

			case 0x43:
				if (verbose) printf("  Found a satellite delivery system descriptor\n");
				//parse_satellite_delivery_system_descriptor (buf, data);
				print_unknown_descriptor(buf, descriptor_len);
				break;

			case 0x44:
				if (verbose) printf("  Found a cable delivery system descriptor\n");
				//parse_cable_delivery_system_descriptor (buf, data);
				print_unknown_descriptor(buf, descriptor_len);
				break;

			case 0x5a:
				if (verbose) printf("  Found a terrestrial delivery system descriptor\n");
				parse_terrestrial_delivery_system_descriptor (buf, data);
				break;

			case 0x62:
				if (verbose) printf("  Found a frequency list descriptor\n");
				parse_frequency_list_descriptor (buf);
				break;

			case 0x83:
				/* 0x83 is in the privately defined range of descriptor tags,
				 * so we parse this only if the user says so to avoid
				 * problems when 0x83 is something entirely different... */
				//if (vdr_dump_channum)
				if (verbose) printf("  Found a terrestrial uk channel number\n");
				parse_terrestrial_uk_channel_number (buf);
				break;

			default:
				if (verbose) printf("  skip descriptor 0x%02x\n", descriptor_tag);
				print_unknown_descriptor(buf, descriptor_len);
				
		};

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
}

void Mpeg2DataParser::parse_descriptorsSDT(const unsigned char *buf, int remaining_length, struct service *data)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			if (verbose) printf("  descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x48:
				parse_service_descriptor (buf, data);
				break;

/*			case 0x53:
				parse_ca_identifier_descriptor (buf, data);
				break;
*/
			default:
				if (verbose) printf("  skip descriptor 0x%02x\n", descriptor_tag);
				print_unknown_descriptor(buf, descriptor_len);
		};

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
}

void Mpeg2DataParser::parse_pat(const unsigned char *buf, int section_length, int transport_stream_id)
{
	while (section_length >= 4)
	{
		struct service *s;
		int service_id = (buf[0] << 8) | buf[1];
		int pmtPID = ((buf[2] & 0x1f) << 8) | buf[3];

		if (service_id != 0)	/*  skip nit pid entry... */
		{
			if (verbose) printf("  Found service id 0x%04x with PMT 0x%04x\n", service_id, pmtPID);
			/* SDT might have been parsed first... */
			s = find_service(current_tp, service_id);
			if (!s)
				s = alloc_service(current_tp, service_id);
			else
				if (verbose) printf("Existing service object found\n");

			s->pmt_pid = pmtPID;
			if (!s->pmt_pid)
			{
				if (verbose) printf("Skipping adding filter. pmt pid is 0x00\n");
			}
			else if (s->priv)
			{
				if (verbose) printf("Skipping adding filter.\n");
			}
			else
			{
				s->priv = (struct section_buf *)malloc(sizeof(struct section_buf));
				SetupFilter(s->priv, s->pmt_pid, 0x02, 1, 0, 5);

				AddFilter (s->priv);
			}
		}
		else
		{
			if (verbose) printf("Skipping nit pid entry with service_id 0x00\n");
		}

		buf += 4;
		section_length -= 4;
	};
}

void Mpeg2DataParser::parse_pmt (const unsigned char *buf, int section_length, int service_id)
{
	int program_info_len;
	struct service *s;
        char msg_buf[14 * AUDIO_CHAN_MAX + 1];
        char *tmp;
        int i;

	s = find_service (current_tp, service_id);
	if (!s) {
		if (verbose) printf("PMT for serivce_id 0x%04x was not in PAT\n", service_id);
		return;
	}

	s->pcr_pid = ((buf[0] & 0x1f) << 8) | buf[1];

	program_info_len = ((buf[2] & 0x0f) << 8) | buf[3];

	buf += program_info_len + 4;
	section_length -= program_info_len + 4;

	while (section_length >= 5) {
		int ES_info_len = ((buf[3] & 0x0f) << 8) | buf[4];
		int elementary_pid = ((buf[1] & 0x1f) << 8) | buf[2];
		int streamType = buf[0];
		buf += 5;
		section_length -= 5;

		switch (streamType) {
		case 0x01:
		case 0x02:
			if (verbose) printf("  VIDEO     : PID 0x%04x\n", elementary_pid);
			if (s->video_pid == 0)
				s->video_pid = elementary_pid;
			break;
		case 0x03:
		case 0x04:
			if (verbose) printf("  AUDIO     : PID 0x%04x\n", elementary_pid);
			if (s->audio_num < AUDIO_CHAN_MAX) {
				s->audio_pid[s->audio_num] = elementary_pid;
				parse_descriptorsPMT (buf, ES_info_len, s);
				s->audio_num++;
			}
			else
				if (verbose) printf("more than %i audio channels, truncating\n", AUDIO_CHAN_MAX);
			break;
		case 0x06:
			if (find_descriptor(0x56, buf, ES_info_len, NULL, NULL)) {
				if (verbose) printf("  TELETEXT  : PID 0x%04x\n", elementary_pid);
				s->teletext_pid = elementary_pid;
				break;
			}
			else if (find_descriptor(0x59, buf, ES_info_len, NULL, NULL)) {
				/* Note: The subtitling descriptor can also signal
				 * teletext subtitling, but then the teletext descriptor
				 * will also be present; so we can be quite confident
				 * that we catch DVB subtitling streams only here, w/o
				 * parsing the descriptor. */
				if (verbose) printf("  SUBTITLING: PID 0x%04x\n", elementary_pid);
				s->subtitling_pid = elementary_pid;
				break;
			}
			else if (find_descriptor(0x6a, buf, ES_info_len, NULL, NULL)) {
				if (verbose) printf("  AC3       : PID 0x%04x\n", elementary_pid);
				s->ac3_pid = elementary_pid;
				parse_descriptorsPMT(buf, ES_info_len, s);
				break;
			}
			/* fall through */
		default:
			if (verbose) printf("  OTHER     : PID 0x%04x TYPE 0x%02x\n", elementary_pid, streamType);
		};

		buf += ES_info_len;
		section_length -= ES_info_len;
	};


    tmp = msg_buf;
    tmp += sprintf(tmp, "0x%04x (%.4s)", s->audio_pid[0], s->audio_lang[0]);

	if (s->audio_num > AUDIO_CHAN_MAX) {
		if (verbose) printf("more than %i audio channels: %i, truncating to %i\n",
							AUDIO_CHAN_MAX, s->audio_num, AUDIO_CHAN_MAX);
		s->audio_num = AUDIO_CHAN_MAX;
	}

    for (i=1; i<s->audio_num; i++)
            tmp += sprintf(tmp, ", 0x%04x (%.4s)", s->audio_pid[i], s->audio_lang[i]);

    if (verbose)
		printf("0x%04x 0x%04x: %s -- %s, pmt_pid 0x%04x, vpid 0x%04x, apid %s\n",
				s->transport_stream_id,
				s->service_id,
				s->provider_name, s->service_name,
				s->pmt_pid, s->video_pid, msg_buf);
}


void Mpeg2DataParser::parse_nit (const unsigned char *buf, int section_length, int network_id)
{
	int descriptors_loop_len = ((buf[0] & 0x0f) << 8) | buf[1];

	if (section_length < descriptors_loop_len + 4)
	{
		if (verbose) printf("section too short: network_id == 0x%04x, section_length == %i, "
							"descriptors_loop_len == %i\n",
							network_id, section_length, descriptors_loop_len);
		return;
	}

	parse_descriptorsNIT (buf + 2, descriptors_loop_len, NULL);

	section_length -= descriptors_loop_len + 4;
	buf += descriptors_loop_len + 4;

	while (section_length > 6) {
		int transport_stream_id = (buf[0] << 8) | buf[1];
		struct transponder tn;
		//struct transponder *t;

		descriptors_loop_len = ((buf[4] & 0x0f) << 8) | buf[5];

		if (section_length < descriptors_loop_len + 4)
		{
			if (verbose) printf("section too short: transport_stream_id == 0x%04x, "
								"section_length == %i, descriptors_loop_len == %i\n",
								transport_stream_id, section_length,
								descriptors_loop_len);
			break;
		}

		if (verbose) printf("  transport_stream_id 0x%04x\n", transport_stream_id);

		memset(&tn, 0, sizeof(tn));
		//tn.type = -1;
		tn.network_id = network_id;
		tn.transport_stream_id = transport_stream_id;

		parse_descriptorsNIT (buf + 6, descriptors_loop_len, &tn);

/*		if (tn.type == fe_info.type) {
			// only add if develivery_descriptor matches FE type
			t = find_transponder(tn.param.frequency);
			if (!t)
				t = alloc_transponder(tn.param.frequency);
			copy_transponder(t, &tn);
		}*/

		section_length -= descriptors_loop_len + 6;
		buf += descriptors_loop_len + 6;
	};
}


void Mpeg2DataParser::parse_sdt (const unsigned char *buf, int section_length, int transport_stream_id)
{
	buf += 3;	       /*  skip original network id + reserved field */

	while (section_length > 4)
	{
		int service_id = (buf[0] << 8) | buf[1];
		int remainingLength = ((buf[3] & 0x0f) << 8) | buf[4];
		struct service *s;

		if (section_length < remainingLength || !remainingLength)
		{
			if (verbose) printf("  section too short: service_id == 0x%02x, section_length == %i, "
								"remainingLength == %i\n",
								service_id, section_length,
								remainingLength);
			return;
		}

		s = find_service(current_tp, service_id);
		if (!s)
			/* maybe PAT has not yet been parsed... */
			s = alloc_service(current_tp, service_id);

		s->running = (enum running_mode)((buf[3] >> 5) & 0x7);
		s->scrambled = (buf[3] >> 4) & 1;
		s->transport_stream_id = transport_stream_id;

		parse_descriptorsSDT (buf + 5, remainingLength - 5, s);

		section_length -= remainingLength + 5;
		buf += remainingLength + 5;
	};
}

/**
 *   returns 0 when more sections are expected
 *	   1 when all sections are read on this pid
 *	   -1 on invalid table id
 */
int Mpeg2DataParser::parse_section (struct section_buf *s)
{
	const unsigned char *buf = s->buf;
	int table_id;
	int section_length;
	int table_id_ext;
	int section_version_number;
	int section_number;
	int last_section_number;
	int i;

	table_id = buf[0];

	SECTION *pSection = (SECTION*)buf;
	LONG_SECTION *pLong = (LONG_SECTION*)buf;
    MPEG_HEADER_BITS *pHeader = (MPEG_HEADER_BITS*)&pSection->Header.W;
    MPEG_HEADER_VERSION_BITS *pVersion = (MPEG_HEADER_VERSION_BITS*)&pLong->Version.B; 

	if (s->table_id != table_id)
		return -1;

	section_length = (((buf[2] & 0x0f) << 8) | buf[1]) + 3;	//the mpeg2 field doesn't include the first 3 bytes of data
	section_length -= 4; //let's ignore the CRC at he end

	table_id_ext = (buf[4] << 8) | buf[3];
	section_version_number = (buf[5] >> 1) & 0x1f;
	section_number = buf[6];
	last_section_number = buf[7];

	if (s->segmented && s->table_id_ext != -1 && s->table_id_ext != table_id_ext) {
		/* find or allocate actual section_buf matching table_id_ext */
		while (s->next_seg) {
			s = s->next_seg;
			if (s->table_id_ext == table_id_ext)
				break;
		}
		if (s->table_id_ext != table_id_ext) {
			//assert(s->next_seg == NULL);
			s->next_seg = (struct section_buf *)calloc(1, sizeof(struct section_buf));
			s->next_seg->segmented = s->segmented;
			s->next_seg->run_once = s->run_once;
			s->next_seg->timeout = s->timeout;
			s = s->next_seg;
			s->table_id = table_id;
			s->table_id_ext = table_id_ext;
			s->section_version_number = section_version_number;
		}
	}

	if (s->section_version_number != section_version_number ||
			s->table_id_ext != table_id_ext) {
		struct section_buf *next_seg = s->next_seg;

		if (s->section_version_number != -1 && s->table_id_ext != -1)
			if (verbose) printf("section version_number or table_id_ext changed "
								"%d -> %d / %04x -> %04x\n",
								s->section_version_number, section_version_number,
								s->table_id_ext, table_id_ext);
		s->table_id_ext = table_id_ext;
		s->section_version_number = section_version_number;
		s->sectionfilter_done = 0;
		memset (s->section_done, 0, sizeof(s->section_done));
		s->next_seg = next_seg;
	}

	buf += 8;
	section_length -= 8;

	if (!get_bit(s->section_done, section_number)) {
		set_bit (s->section_done, section_number);

		if (verbose) printf("pid 0x%02x tid 0x%02x table_id_ext 0x%04x, "
							"%i/%i (version %i)\n",
							s->pid, table_id, table_id_ext, section_number,
							last_section_number, section_version_number);

		switch (table_id) {
		case 0x00:
			if (verbose) printf("PAT\n");
			parse_pat (buf, section_length, table_id_ext);
			break;

		case 0x02:
			if (verbose) printf("PMT 0x%04x for service 0x%04x\n", s->pid, table_id_ext);
			parse_pmt (buf, section_length, table_id_ext);
			break;

		case 0x41:
			if (verbose) printf("////////////////////////////////////////////// NIT other\n");
		case 0x40:
			if (verbose) printf("NIT (%s TS)\n", table_id == 0x40 ? "actual":"other");
			parse_nit (buf, section_length, table_id_ext);
			break;

		case 0x42:
		case 0x46:
			if (verbose) printf("SDT (%s TS)\n", table_id == 0x42 ? "actual":"other");
			parse_sdt (buf, section_length, table_id_ext);
			break;

		default:
			;
		};

		for (i = 0; i <= last_section_number; i++)
			if (get_bit (s->section_done, i) == 0)
				break;

		if (i > last_section_number)
			s->sectionfilter_done = 1;
	}

	if (s->segmented) {
		/* always wait for timeout; this is because we don't now how
		 * many segments there are
		 */
		return 0;
	}
	else if (s->sectionfilter_done)
		return 1;

	return 0;
}

int Mpeg2DataParser::read_sections (struct section_buf *s)
{
	int section_length;
	//int count;

	if (s->sectionfilter_done && !s->segmented)
		return 1;

	/* the section filter API guarantess that we get one full section
	 * per read(), provided that the buffer is large enough (it is)
	 */
//	if (((count = read (s->fd, s->buf, sizeof(s->buf))) < 0) && errno == EOVERFLOW)
//		count = read (s->fd, s->buf, sizeof(s->buf));
//	if (count < 0) {
//		printf("read_sections: read error");
//		return -1;
//	}

//	if (count < 4)
//		return -1;

	section_length = ((s->buf[1] & 0x0f) << 8) | s->buf[2];

//	if (count != section_length + 3)
//		return -1;

	if (parse_section(s) == 1)
		return 1;

	return 0;
}

void Mpeg2DataParser::PrintByteArray(const BYTE *pData, long cbSize)
{
	if (verbose)
	{
		char str[9];
		memset((char*)&str, 32, 8);
		str[8] = '\0';
		for (int iter = 0; iter < cbSize; iter++)
		{
			int pos = iter % 8;

			if (pos == 0)
				wprintf(L"\t");

			wprintf(L"0x%.2x ", pData[iter]);
			if ((pData[iter] >= 32) && (pData[iter] <= 126))
				str[pos] = pData[iter];

			if (pos == 7)
			{
				wprintf(L"  %S\n", (char*)&str);
				memset((char*)&str, 32, 8);
			}
		} 
		int missing = 8 - (((iter-1)%8)+1);
		if (missing != 0)
		{
			for (int i=0 ; i< missing ; i++ )
				wprintf(L"     ");
			wprintf(L"  %S\n", (char*)&str);
		}
	}
}

void Mpeg2DataParser::PrintWordArray(const BYTE *pData, long cbSize)
{ 
	if (verbose)
	{
		int iter = 0;
		while (iter < cbSize) 
		{
			if (iter % 16 == 0)
				wprintf(L"\t");

			if (iter%2 == 0)
				wprintf(L"0x");

			printf(_T("%.2x"), pData[iter]);

			if (iter%2 == 1)
				wprintf(L" ");

			if (iter % 16 == 15)
				wprintf(L"\n");

			iter++;
		} 
		if (iter % 16 != 0)
			wprintf(L"\n");
	}
}

void Mpeg2DataParser::PaddingForNumber(long number, long totalWidth)
{
	long len = 0;
	if (number != 0)
		len = log10(((number > 0) ? number : -1 * number));
	if (number < 0)
		len++;
	len++;

	while ( len < totalWidth )
	{
		printf(" ");
		len++;
	}
}

