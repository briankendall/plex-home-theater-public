#ifndef PLEXPLAYLISTPLAYER_H
#define PLEXPLAYLISTPLAYER_H

#include "playlists/PlayList.h"
#include "PlayListPlayer.h"

namespace PLAYLIST
{
  class CPlexPlayListPlayer : public CPlayListPlayer
  {
  };
}

extern PLAYLIST::CPlexPlayListPlayer g_playlistPlayer;

#endif // PLEXPLAYLISTPLAYER_H
