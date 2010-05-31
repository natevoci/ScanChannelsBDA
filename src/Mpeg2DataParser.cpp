/**
 *	Mpeg2DataParser.cpp
 *	Copyright (C) 2004 Nate
 *  Copyright (C) 2004 JoeyBloggs
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

#include <process.h>
#include <math.h>

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
	current_tp = NULL;

	//verbose = FALSE;
	m_bThreadStarted = FALSE;
	m_bActivity = FALSE;
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
	m_piIMpeg2Data.Release(); 

	if (pBDASecTab != NULL) 
	{
		HRESULT hr = pBDASecTab->QueryInterface(__uuidof(IMpeg2Data), reinterpret_cast<void**>(&m_piIMpeg2Data));
	}
}

void Mpeg2DataParser::ReleaseFilter()
{
	m_piIMpeg2Data.Release();
}

//////////////////////////////////////////////////////////////////////
// Event stuff
//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::Reset()
{
	current_tp = NULL;

	struct section_buf *s;
	while (waiting_filters.size())
	{
		s = waiting_filters.back();
		waiting_filters.pop_back();
		free (s);
	}

	struct transponder *t;
	while (transponders.size())
	{
		t = transponders.back();
		transponders.pop_back();
		DeleteTransponder(t);
	}

	m_bThreadStarted = FALSE;
	m_bActivity = FALSE;
	ResetEvent(m_hScanningDoneEvent);
}

void Mpeg2DataParser::DeleteTransponder(struct transponder *t)
{
	struct service *s;
	while (t->services.size())
	{
		s = t->services.back();
		t->services.pop_back();
		DeleteService(s);
	}
	if (t->other_f)
		free(t->other_f);
	t->other_f = NULL;
	free(t);
}

void Mpeg2DataParser::DeleteService(struct service *s)
{
	if (s->provider_name)
		free(s->provider_name);
	s->provider_name = NULL;
	if (s->service_name)
		free(s->service_name);
	s->service_name = NULL;

	free(s);
}

DWORD Mpeg2DataParser::WaitForScanToFinish()
{
	DWORD result;
	do
	{
		result = WaitForSingleObject(m_hScanningDoneEvent, 15000);
	} while ((result == WAIT_TIMEOUT) && (m_bActivity));
	return result;
}

//////////////////////////////////////////////////////////////////////
// Method just to kick off a separate thread
//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::StartMpeg2DataScan()
{
	//Make sure we only start scanning the first time the ServiceChanged event calls us.
	if (m_bThreadStarted)
		return;
	m_bThreadStarted = TRUE;

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

	m_bActivity = TRUE;
	try
	{
		if (m_piIMpeg2Data != NULL) 
		{
			struct section_buf *s0;

			s0 = (struct section_buf *)malloc(sizeof(struct section_buf));
			SetupFilter(s0, 0x00, 0x00, 1, 0, 5);  // PAT
			AddFilter(s0);

			s0 = (struct section_buf *)malloc(sizeof(struct section_buf));
			SetupFilter(s0, 0x10, 0x40, 1, 0, 15); // NIT
			AddFilter(s0);

			s0 = (struct section_buf *)malloc(sizeof(struct section_buf));
			SetupFilter(s0, 0x11, 0x42, 1, 0, 5);  // SDT
			AddFilter(s0);

			ReadFilters();
		}
	}
	catch(...)
	{
		output.showf("# Unhandled exception in Mpeg2DataParser::StartMpeg2DataScanThread()\n");
	}

	m_bActivity = FALSE;
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
}

void Mpeg2DataParser::AddFilter (struct section_buf *s)
{
	waiting_filters.push_back(s);
}

void Mpeg2DataParser::ReadFilters(void)
{
	struct section_buf *s;

	while (waiting_filters.size())
	{
		s = waiting_filters.front();
		ReadSection(s);
		waiting_filters.erase(waiting_filters.begin());
		free (s);
	}
}





//////////////////////////////////////////////////////////////////////
void Mpeg2DataParser::ReadSection(struct section_buf *s)
{
	PID pid = s->pid;
	TID tid = s->table_id;
	DWORD dwTimeout = (DWORD)(s->timeout * 1000);
	if (m_piIMpeg2Data != NULL)
	{
		HRESULT hr;
		ISectionList* piSectionList = NULL;

		if (SUCCEEDED(hr = m_piIMpeg2Data->GetTable(pid, tid, NULL, dwTimeout, &piSectionList)))
		{
			WORD cSections;
			if (SUCCEEDED(hr = piSectionList->GetNumberOfSections(&cSections)))
			{
				verbose.showf(_T("\n%d section(s) found for pid=%.4x, tid=%.2x\n"), cSections, pid, tid);
			
				verbose.showf("pid \n");

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
				output.showf(_T("Error 0x%x getting number of sections\n"), hr); 
			}
			
			piSectionList->Release();
		}
		else
		{
			output.showf(_T("Timeout getting table (pid=%.4x, tid=%.2x)\n"), pid, tid);
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
	s->service_id = service_id;
	tp->services.push_back(s);
	return s;
}

struct service *Mpeg2DataParser::find_service(struct transponder *tp, int service_id)
{
	struct service *s;

	vector<service *>::iterator it;
	for ( it = tp->services.begin() ; it != tp->services.end() ; it++ )
	{
		s = *it;
		if (s->service_id == service_id)
			return s;
	}
	return NULL;
}

struct transponder *Mpeg2DataParser::alloc_transponder(int transport_stream_id)
{
	struct transponder *tp = (struct transponder *)calloc(1, sizeof(*tp));

	tp->transport_stream_id = transport_stream_id;
	transponders.push_back(tp);
	return tp;
}

struct transponder *Mpeg2DataParser::find_transponder(int transport_stream_id)
{
	struct transponder *tp;

	vector<transponder *>::iterator it;
	for ( it = transponders.begin() ; it != transponders.end() ; it++ )
	{
		tp = *it;
		if (tp->transport_stream_id == transport_stream_id)
			return tp;
	}
	return NULL;
}

void Mpeg2DataParser::parse_iso639_language_descriptor (const unsigned char *buf, struct service *s)
{
	unsigned char len = buf [1];

	buf += 2;

	if (len >= 4)
	{
		verbose.showf("    LANG=%.3s %d\n", buf, buf[3]);
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

void Mpeg2DataParser::parse_network_name_descriptor (const unsigned char *buf, struct transponder *tp)
{
	if (tp->network_name)
		free (tp->network_name);

	unsigned char len = buf [1];

	tp->network_name = (unsigned char *)malloc (len + 1);
	memcpy (tp->network_name, buf+2, len);
	tp->network_name[len] = '\0';

	verbose.showf("    Network Name '%.*s'\n", len, buf + 2);
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
		verbose.showf("    terrestrial_delivery_system_descriptor outside transport stream definition (ignored)\n");
		return;
	}
	//o = &t->param.u.ofdm;
	//t->type = FE_OFDM;


	t->frequency = (buf[2] << 24) | (buf[3] << 16);
	t->frequency |= (buf[4] << 8) | buf[5];
	t->frequency /= 100;
	verbose.showf("    Frequency %i", t->frequency);


	t->bandwidth = 8 - ((buf[6] >> 5) & 0x3);
	verbose.showf("    Bandwidth %i", t->bandwidth);


	//o->constellation = m_tab[(buf[7] >> 6) & 0x3];
	switch ((buf[7] >> 6) & 0x3)
	{
	case 0:
		verbose.showf("    QPSK\n");
		break;
	case 1:
		verbose.showf("    QAM_16\n");
		break;
	case 2:
		verbose.showf("    QAM_64\n");
		break;
	case 3:
		verbose.showf("    QAM_AUTO\n");
		break;
	}

	//o->hierarchy_information = HIERARCHY_NONE + ((buf[7] >> 3) & 0x3);

	//DWORD coderateHP = buf[7] & 0x7;
	switch (buf[7] & 0x7)
	{
	case 0:
		verbose.showf("    HP - FEC_1_2");
		break;
	case 1:
		verbose.showf("    HP - FEC_2_3");
		break;
	case 2:
		verbose.showf("    HP - FEC_3_4");
		break;
	case 3:
		verbose.showf("    HP - FEC_5_6");
		break;
	case 4:
		verbose.showf("    HP - FEC_7_8");
		break;
	default:
		verbose.showf("    HP - FEC_AUTO");
		break;
	}

	//if (((buf[8] >> 5) & 0x7) > 4)
	//DWORD coderateLP = (buf[8] >> 5) & 0x7;
	switch ((buf[8] >> 5) & 0x7)
	{
	case 0:
		verbose.showf("    LP - FEC_1_2");
		break;
	case 1:
		verbose.showf("    LP - FEC_2_3");
		break;
	case 2:
		verbose.showf("    LP - FEC_3_4");
		break;
	case 3:
		verbose.showf("    LP - FEC_5_6");
		break;
	case 4:
		verbose.showf("    LP - FEC_7_8");
		break;
	default:
		verbose.showf("    LP - FEC_AUTO");
		break;
	}

	//o->guard_interval = GUARD_INTERVAL_1_32 + ((buf[8] >> 3) & 0x3);

	//o->transmission_mode = (buf[8] & 0x2) ?
	//		       TRANSMISSION_MODE_8K :
	//		       TRANSMISSION_MODE_2K;
	switch (buf[8] & 0x2)
	{
	case 0:
		verbose.showf("    Transmission mode 2K\n");
		break;
	default:
		verbose.showf("    Transmission mode 8K\n");
		break;
	}

	t->other_frequency_flag = (buf[8] & 0x01);
	if (t->other_frequency_flag)
		verbose.showf("    Other frequency flags set\n");

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

void Mpeg2DataParser::parse_frequency_list_descriptor (const unsigned char *buf, struct transponder *t)
{
	t->n_other_f = (buf[1] - 1) / 4;
	if (t->n_other_f < 1 || (buf[2] & 0x03) != 3)
	{
		t->n_other_f = 0;
		return;
	}

	if (t->other_f)
		free(t->other_f);
	t->other_f = (__int32*)malloc(sizeof(int)*t->n_other_f);

	buf += 3;
	for (int i = 0; i < t->n_other_f; i++)
	{
		t->other_f[i] = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
		t->other_f[i] /= 100;
		verbose.showf("    Alternate Frequency %i\n", t->other_f[i]);
		buf += 4;
	}
}

void Mpeg2DataParser::parse_terrestrial_uk_channel_number (const unsigned char *buf, struct transponder *t)
{
	int i, n, channel_num, service_id;
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
		verbose.showf("    Service ID 0x%x has channel number %d\n", service_id, channel_num);

		//Might need to loop here for other transponders
		s = find_service(t, service_id);
		if (!s)
			alloc_service(t, service_id);

		if (s)
			s->channel_num = channel_num;
		
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

	verbose.showf(  "  0x%04x 0x%04x: pmt_pid 0x%04x %s -- %s (%s%s)\n",
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
			verbose.showf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
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

void Mpeg2DataParser::parse_descriptorsPMT(const unsigned char *buf, int remaining_length, struct service *s)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			verbose.showf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x0a:
				parse_iso639_language_descriptor (buf, s);
				break;

			default:
				verbose.showf("    skip descriptor 0x%02x\n", descriptor_tag);
				print_unknown_descriptor(buf, descriptor_len);
		};

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
}

void Mpeg2DataParser::parse_descriptorsNIT(const unsigned char *buf, int remaining_length, struct transponder *tp)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			verbose.showf("descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x40:
				verbose.showf("  Found a network name descriptor\n");
				parse_network_name_descriptor (buf, tp);
				break;

			case 0x43:
				verbose.showf("  Found a satellite delivery system descriptor\n");
				//parse_satellite_delivery_system_descriptor (buf, tp);
				print_unknown_descriptor(buf, descriptor_len);
				break;

			case 0x44:
				verbose.showf("  Found a cable delivery system descriptor\n");
				//parse_cable_delivery_system_descriptor (buf, tp);
				print_unknown_descriptor(buf, descriptor_len);
				break;

			case 0x5a:
				verbose.showf("  Found a terrestrial delivery system descriptor\n");
				parse_terrestrial_delivery_system_descriptor (buf, tp);
				break;

			case 0x62:
				verbose.showf("  Found a frequency list descriptor\n");
				parse_frequency_list_descriptor (buf, tp);
				break;

			case 0x83:
				/* 0x83 is in the privately defined range of descriptor tags,
				 * so we parse this only if the user says so to avoid
				 * problems when 0x83 is something entirely different... */
				//if (vdr_dump_channum)
				verbose.showf("  Found a terrestrial uk channel number\n");
				parse_terrestrial_uk_channel_number (buf, tp);
				break;

			default:
				verbose.showf("  skip descriptor 0x%02x\n", descriptor_tag);
				print_unknown_descriptor(buf, descriptor_len);
				
		};

		buf += descriptor_len;
		remaining_length -= descriptor_len;
	}
}

void Mpeg2DataParser::parse_descriptorsSDT(const unsigned char *buf, int remaining_length, struct service *s)
{
	while (remaining_length > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			verbose.showf("  descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		switch (descriptor_tag)
		{
			case 0x48:
				parse_service_descriptor (buf, s);
				break;

/*			case 0x53:
				parse_ca_identifier_descriptor (buf, data);
				break;
*/
			default:
				verbose.showf("  skip descriptor 0x%02x\n", descriptor_tag);
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
			if (!current_tp)
			{
				current_tp = find_transponder(transport_stream_id);	//i'm not expecting this to return anything, but just in case
				if (!current_tp)
					current_tp = alloc_transponder(transport_stream_id);
			}

			verbose.showf("  Found service id 0x%04x with PMT 0x%04x\n", service_id, pmtPID);
			/* SDT might have been parsed first... */
			s = find_service(current_tp, service_id);
			if (!s)
				s = alloc_service(current_tp, service_id);
			else
				verbose.showf("Existing service object found\n");

			s->pmt_pid = pmtPID;
			if (!s->pmt_pid)
			{
				verbose.showf("Skipping adding filter. pmt pid is 0x00\n");
			}
			else if (s->priv)
			{
				verbose.showf("Skipping adding filter.\n");
			}
			else
			{
				s->priv = (struct section_buf *)malloc(sizeof(struct section_buf));
				SetupFilter(s->priv, s->pmt_pid, 0x02, 1, 0, 5);

				AddFilter (s->priv);
				s->priv = NULL;
			}
		}
		else
		{
			verbose.showf("Skipping nit pid entry with service_id 0x00\n");
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
		verbose.showf("PMT for serivce_id 0x%04x was not in PAT\n", service_id);
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
			verbose.showf("  VIDEO     : PID 0x%04x\n", elementary_pid);
			if (s->video_pid == 0)
				s->video_pid = elementary_pid;
			break;
		case 0x03:
		case 0x04:
			verbose.showf("  AUDIO     : PID 0x%04x\n", elementary_pid);
			if (s->audio_num < AUDIO_CHAN_MAX) {
				s->audio_pid[s->audio_num] = elementary_pid;
				parse_descriptorsPMT (buf, ES_info_len, s);
				s->audio_num++;
			}
			else
				verbose.showf("more than %i audio channels, truncating\n", AUDIO_CHAN_MAX);
			break;
		case 0x06:
			if (find_descriptor(0x56, buf, ES_info_len, NULL, NULL)) {
				verbose.showf("  TELETEXT  : PID 0x%04x\n", elementary_pid);
				s->teletext_pid = elementary_pid;
				break;
			}
			else if (find_descriptor(0x59, buf, ES_info_len, NULL, NULL)) {
				/* Note: The subtitling descriptor can also signal
				 * teletext subtitling, but then the teletext descriptor
				 * will also be present; so we can be quite confident
				 * that we catch DVB subtitling streams only here, w/o
				 * parsing the descriptor. */
				verbose.showf("  SUBTITLING: PID 0x%04x\n", elementary_pid);
				s->subtitling_pid = elementary_pid;
				break;
			}
			else if (find_descriptor(0x6a, buf, ES_info_len, NULL, NULL)) {
				verbose.showf("  AC3       : PID 0x%04x\n", elementary_pid);
				s->ac3_pid = elementary_pid;
				parse_descriptorsPMT(buf, ES_info_len, s);
				break;
			}
			/* fall through */
		default:
			verbose.showf("  OTHER     : PID 0x%04x TYPE 0x%02x\n", elementary_pid, streamType);
		};

		buf += ES_info_len;
		section_length -= ES_info_len;
	};


    tmp = msg_buf;
    tmp += sprintf(tmp, "0x%04x (%.4s)", s->audio_pid[0], s->audio_lang[0]);

	if (s->audio_num > AUDIO_CHAN_MAX) {
		verbose.showf("more than %i audio channels: %i, truncating to %i\n",
							AUDIO_CHAN_MAX, s->audio_num, AUDIO_CHAN_MAX);
		s->audio_num = AUDIO_CHAN_MAX;
	}

    for (i=1; i<s->audio_num; i++)
            tmp += sprintf(tmp, ", 0x%04x (%.4s)", s->audio_pid[i], s->audio_lang[i]);

    verbose.showf  ("0x%04x 0x%04x: %s -- %s, pmt_pid 0x%04x, vpid 0x%04x, apid %s\n",
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
		verbose.showf("section too short: network_id == 0x%04x, section_length == %i, "
							"descriptors_loop_len == %i\n",
							network_id, section_length, descriptors_loop_len);
		return;
	}

	parse_descriptorsNIT (buf + 2, descriptors_loop_len, current_tp);

	section_length -= descriptors_loop_len + 4;
	buf += descriptors_loop_len + 4;

	while (section_length > 6) {
		int transport_stream_id = (buf[0] << 8) | buf[1];
		struct transponder *t;

		t = find_transponder(transport_stream_id);
		if (!t)
			t = alloc_transponder(transport_stream_id);

		t->network_id = network_id;
		t->original_network_id = (buf[2] << 8) | buf[3];

		descriptors_loop_len = ((buf[4] & 0x0f) << 8) | buf[5];

		if (section_length < descriptors_loop_len + 4)
		{
			verbose.showf("section too short: transport_stream_id == 0x%04x, "
								"section_length == %i, descriptors_loop_len == %i\n",
								t->transport_stream_id, section_length,
								descriptors_loop_len);
			break;
		}

		verbose.showf("  transport_stream_id 0x%04x\n", t->transport_stream_id);


		parse_descriptorsNIT (buf + 6, descriptors_loop_len, t);

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

	struct transponder *tp;
	tp = find_transponder(transport_stream_id);	//i'm not expecting this to return anything, but just in case
	if (!tp)
		tp = alloc_transponder(transport_stream_id);

	while (section_length > 4)
	{
		int service_id = (buf[0] << 8) | buf[1];
		int remainingLength = ((buf[3] & 0x0f) << 8) | buf[4];
		struct service *s;

		if (section_length < remainingLength || !remainingLength)
		{
			verbose.showf("  section too short: service_id == 0x%02x, section_length == %i, "
								"remainingLength == %i\n",
								service_id, section_length,
								remainingLength);
			return;
		}

		s = find_service(tp, service_id);
		if (!s)
			/* maybe PAT has not yet been parsed... */
			s = alloc_service(tp, service_id);

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
			verbose.showf("section version_number or table_id_ext changed "
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

		verbose.showf("pid 0x%02x tid 0x%02x table_id_ext 0x%04x, "
							"%i/%i (version %i)\n",
							s->pid, table_id, table_id_ext, section_number,
							last_section_number, section_version_number);

		switch (table_id)
		{
		case 0x00:
			verbose.showf("PAT\n");
			parse_pat (buf, section_length, table_id_ext);
			break;

		case 0x02:
			verbose.showf("PMT 0x%04x for service 0x%04x\n", s->pid, table_id_ext);
			parse_pmt (buf, section_length, table_id_ext);
			break;

		case 0x41:
			verbose.showf("////////////////////////////////////////////// NIT other\n");
		case 0x40:
			verbose.showf("NIT (%s TS)\n", table_id == 0x40 ? "actual":"other");
			parse_nit (buf, section_length, table_id_ext);
			break;

		case 0x42:
		case 0x46:
			verbose.showf("SDT (%s TS)\n", table_id == 0x42 ? "actual":"other");
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

void Mpeg2DataParser::PrintByteArray(const BYTE *pData, long cbSize)
{
	char str[9];
	memset((char*)&str, 32, 8);
	str[8] = '\0';
	int iter = 0;
	for (iter = 0; iter < cbSize; iter++)
	{
		int pos = iter % 8;

		if (pos == 0)
			verbose.showf("\t");

		verbose.showf("0x%.2x ", pData[iter]);
		if ((pData[iter] >= 32) && (pData[iter] <= 126))
			str[pos] = pData[iter];

		if (pos == 7)
		{
			verbose.showf("  %S\n", (char*)&str);
			memset((char*)&str, 32, 8);
		}
	} 
	int missing = 8 - (((iter-1)%8)+1);
	if (missing != 0)
	{
		for (int i=0 ; i< missing ; i++ )
			verbose.showf("     ");
		verbose.showf("  %S\n", (char*)&str);
	}
}

void Mpeg2DataParser::PrintWordArray(const BYTE *pData, long cbSize)
{ 
	int iter = 0;
	while (iter < cbSize) 
	{
		if (iter % 16 == 0)
			verbose.showf("\t");

		if (iter%2 == 0)
			verbose.showf("0x");

		verbose.showf(_T("%.2x"), pData[iter]);

		if (iter%2 == 1)
			verbose.showf(" ");

		if (iter % 16 == 15)
			verbose.showf("\n");

		iter++;
	} 
	if (iter % 16 != 0)
		verbose.showf("\n");
}

void Mpeg2DataParser::PaddingForNumber(long number, long totalWidth)
{
	long len = 0;
	if (number != 0)
		len = (long)log10((double)((number > 0) ? number : -1 * number));
	if (number < 0)
		len++;
	len++;

	while ( len < totalWidth )
	{
		output.showf(" ");
		len++;
	}
}

void Mpeg2DataParser::PrintDigitalWatchChannelsIni()
{
	struct service *s;

	output.showf("\nNetwork_%i(\"%s\", %i, %i, 1)\n", m_networkNumber, current_tp->network_name, current_tp->frequency, current_tp->bandwidth);

	int i;
	if (current_tp->n_other_f > 0)
		output.showf("#Alternate %s\n", ((current_tp->n_other_f > 1) ? "frequencies" : "frequency"));
	for ( i=0 ; i<current_tp->n_other_f ; i++ )
	{
		output.showf("#Network_%i(\"%s\", %i, %i, 1)\n", m_networkNumber, current_tp->network_name, current_tp->other_f[i], current_tp->bandwidth);
	}

	i=1;
	vector<service *>::iterator it;
	for ( it = current_tp->services.begin() ; it != current_tp->services.end() ; it++ )
	{
		s = *it;

		for (int audio = 0 ; audio <= s->audio_num ; audio++ )
		{
			if (((audio < s->audio_num) && (s->audio_pid[audio])) || (s->ac3_pid))
			{
				output.showf("  Program_");
				if (i < 10) output.showf(" ");
				output.showf("%i(\"%s", i, s->service_name);

				int len = strlen((char *)s->service_name);
				if ((audio > 0) && (audio == s->audio_num))
				{
					output.showf(" AC3");
					len += 4;
				}
				output.showf("\"");
				
				while ( len < 20 ) { output.showf(" "); len++; }
				output.showf(", ");

				PaddingForNumber(s->service_id, 4);
				output.showf("%i, ",	s->service_id);

				PaddingForNumber(s->video_pid, 4);
				output.showf("%i, ", s->video_pid);

				if (audio < s->audio_num)
				{
					PaddingForNumber(s->audio_pid[audio], 5);
					output.showf("%i, ", s->audio_pid[audio]);
				}
				else
				{
					PaddingForNumber(s->ac3_pid, 4);
					output.showf("A%i, ", s->ac3_pid);
				}
				
				PaddingForNumber(s->pmt_pid, 4);
				output.showf("%i)    #", s->pmt_pid);

				if (audio == 0)
				{
					if (s->channel_num)
						output.showf(" LCN=%i", s->channel_num);

					if (s->teletext_pid)
						output.showf(" Teletext=%i", s->teletext_pid);
				}

				output.showf("\n");

				i++;
			}
		}
	}
	output.showf("\n");
}

