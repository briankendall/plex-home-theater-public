#pragma once
/*
 *      Copyright (C) 2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DVDDemux.h"

#ifdef _WIN32
#define __attribute__(dummy_val)
#else
#include <config.h>
#endif

#ifdef _WIN32
#pragma pack(push)
#pragma pack(1)
#endif

typedef struct
{
  char fourcc[4];
  uint32_t type;
  uint32_t channels;
  uint32_t sampleRate;
  uint32_t bitsPerSample;
  uint64_t durationMs;
} __attribute__((__packed__)) Demux_BXA_FmtHeader;

#ifdef _WIN32
#pragma pack(pop)
#endif

#include <vector>

#define BXA_PACKET_TYPE_FMT_DEMUX 1

class CDemuxStreamAudioBXA;

class CDVDDemuxBXA : public CDVDDemux
{
public:

  CDVDDemuxBXA();
  ~CDVDDemuxBXA();

  bool Open(CDVDInputStream* pInput);
  void Dispose();
  void Reset();
  void Abort();
  void Flush();
  DemuxPacket* Read();
  bool SeekTime(int time, bool backwords = false, double* startpts = NULL) { return false; }
  void SetSpeed(int iSpeed) {};
  int GetStreamLength() { return (int)m_header.durationMs; }
  CDemuxStream* GetStream(int iStreamId);
  int GetNrOfStreams();
  std::string GetFileName();
  virtual void GetStreamCodecName(int iStreamId, CStdString &strName);

  /* PLEX */
  virtual int GetStreamBitrate() { return 0; }
  /* END PLEX */

protected:
  friend class CDemuxStreamAudioBXA;
  CDVDInputStream* m_pInput;
  double m_pts;

  CDemuxStreamAudioBXA *m_stream;

  Demux_BXA_FmtHeader m_header;
};

