#include "GUIDialogPlexPlayQueue.h"
#include "FileSystem/PlexDirectory.h"
#include "PlexApplication.h"
#include "PlexPlayQueueManager.h"
#include "music/tags/MusicInfoTag.h"
#include "Application.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
CGUIDialogPlexPlayQueue::CGUIDialogPlexPlayQueue()
  : CGUIDialogSelect(WINDOW_DIALOG_PLAYER_CONTROLS, "PlayerControls.xml")
{
  m_multiSelection = false;
  m_loadType = KEEP_IN_MEMORY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool CGUIDialogPlexPlayQueue::OnMessage(CGUIMessage& message)
{
  if (message.GetMessage() == GUI_MSG_WINDOW_INIT)
    LoadPlayQueue();

  return CGUIDialogSelect::OnMessage(message);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool CGUIDialogPlexPlayQueue::OnAction(const CAction &action)
{
  if (action.GetID() == ACTION_SELECT_ITEM && GetFocusedControlID() == 3)
  {
    ItemSelected();
    return true;
  }

  return CGUIDialogSelect::OnAction(action);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CGUIDialogPlexPlayQueue::LoadPlayQueue()
{
  XFILE::CPlexDirectory dir;
  CFileItemList list;
  int currentItemId = -1;
  if (PlexUtils::IsPlayingPlaylist())
  {
    if (g_application.CurrentFileItemPtr() && g_application.CurrentFileItemPtr()->HasMusicInfoTag())
      currentItemId = g_application.CurrentFileItemPtr()->GetMusicInfoTag()->GetDatabaseId();
  }

  if (dir.GetDirectory("plexserver://playqueue/", list))
  {
    int playingItemIdx = 0;
    for (int i = 0; i < list.Size(); i++)
    {
      CFileItemPtr item = list.Get(i);
      if (item)
      {
        if (item->HasMusicInfoTag() && item->GetMusicInfoTag()->GetDatabaseId() == currentItemId)
        {
          playingItemIdx = i;
          item->Select(true);
        }
        else
        {
          item->Select(false);
        }
        Add(item.get());
      }
    }
    m_viewControl.SetSelectedItem(playingItemIdx);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CGUIDialogPlexPlayQueue::ItemSelected()
{
  int iSelected = m_viewControl.GetSelectedItem();
  if (iSelected >= 0 && iSelected < (int)m_vecList->Size())
  {
    CFileItemPtr item(m_vecList->Get(iSelected));
    if (item && item->HasMusicInfoTag())
    {
      // unselect old item
      for (int i = 0 ; i < m_vecList->Size() ; i++)
        m_vecList->Get(i)->Select(false);

      // select the new one
      item->Select(true);
      g_plexApplication.playQueueManager->playCurrentId(item->GetMusicInfoTag()->GetDatabaseId());
    }
  }
}
