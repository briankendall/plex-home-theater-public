#include "PlexServerManager.h"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <vector>
#include "utils/log.h"
#include "GUIMessage.h"
#include "GUIWindowManager.h"
#include "plex/PlexTypes.h"
#include "Client/PlexConnection.h"
#include "PlexServerDataLoader.h"
#include "File.h"

#include "Stopwatch.h"
#include "settings/GUISettings.h"

#include "PlexApplication.h"
#include "PlexServerCacheDatabase.h"

using namespace std;

void
CPlexServerReachabilityThread::Process()
{
  if (!g_plexApplication.serverManager)
    return;

  if (m_server)
  {
    bool success = m_server->UpdateReachability();
    if (g_plexApplication.serverManager && !m_bStop)
      g_plexApplication.serverManager->ServerReachabilityDone(m_server, success);
  }
}

CPlexServerManager::CPlexServerManager() : m_stopped(false)
{
  CPlexConnectionPtr conn;
  
  _myPlexServer = CPlexServerPtr(new CPlexServer("myplex", "myPlex", true));
  conn = CPlexConnectionPtr(new CMyPlexConnection);
  _myPlexServer->AddConnection(conn);
  _myPlexServer->SetActiveConnection(conn);

  _nodeServer = CPlexServerPtr(new CPlexServer("node", "plexNode", true));
  conn = CPlexConnectionPtr(new CPlexConnection(CPlexConnection::CONNECTION_MYPLEX, "node.plexapp.com", 32400));
  _nodeServer->AddConnection(conn);
  _nodeServer->SetActiveConnection(conn);

  _localServer = CPlexServerPtr(new CPlexServer("local", PlexUtils::GetHostName(), true));
  conn = CPlexConnectionPtr(new CPlexConnection(CPlexConnection::CONNECTION_MANUAL, "127.0.0.1", 32400));
  _localServer->AddConnection(conn);
  _localServer->SetActiveConnection(conn);

}

CPlexServerPtr CPlexServerManager::FindByHostAndPort(const CStdString &host, int port)
{
  CSingleLock lk(m_serverManagerLock);

  BOOST_FOREACH(PlexServerPair p, m_serverMap)
  {
    vector<CPlexConnectionPtr> connections;
    p.second->GetConnections(connections);
    BOOST_FOREACH(CPlexConnectionPtr conn, connections)
    {
      if (conn->GetAddress().GetHostName().Equals(host) &&
          conn->GetAddress().GetPort() == port)
        return p.second;
    }
  }
  return CPlexServerPtr();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CPlexServerPtr CPlexServerManager::FindFromItem(CFileItemPtr item)
{
  if (!item)
    return CPlexServerPtr();

  CStdString uuid = item->GetProperty("plexserver").asString();
  if (uuid.empty())
    return CPlexServerPtr();

  return FindByUUID(uuid);
}

CPlexServerPtr
CPlexServerManager::FindByUUID(const CStdString &uuid)
{
  if (m_stopped)
    return CPlexServerPtr();

  CSingleLock lk(m_serverManagerLock);

  if (uuid.Equals("myplex"))
    return _myPlexServer;

  if (uuid.Equals("local"))
    return _localServer;
  
  if (uuid.Equals("node") || uuid.Equals("com.plexapp.node"))
    return _nodeServer;

  if (uuid.Equals("best"))
    return m_bestServer;

  if (m_serverMap.find(uuid) != m_serverMap.end())
  {
    return m_serverMap.find(uuid)->second;
  }
  return CPlexServerPtr();
}

PlexServerList
CPlexServerManager::GetAllServers(CPlexServerOwnedModifier modifier) const
{
  CSingleLock lk(m_serverManagerLock);

  PlexServerList ret;

  BOOST_FOREACH(PlexServerPair p, m_serverMap)
  {
    if (modifier == SERVER_OWNED && p.second->GetOwned())
      ret.push_back(p.second);
    else if (modifier == SERVER_SHARED && !p.second->GetOwned())
      ret.push_back(p.second);
    else if (modifier == SERVER_ALL)
      ret.push_back(p.second);
  }

  return ret;
}

bool CPlexServerManager::HasAnyServerWithActiveConnection() const
{
  CSingleLock lk(m_serverManagerLock);
  BOOST_FOREACH(PlexServerPair p, m_serverMap)
  {
    if (p.second->GetActiveConnection())
      return true;
  }

  return false;
}

void
CPlexServerManager::MarkServersAsRefreshing()
{
  BOOST_FOREACH(PlexServerPair p, m_serverMap)
    p.second->MarkAsRefreshing();
}

void
CPlexServerManager::UpdateFromConnectionType(PlexServerList servers, int connectionType)
{
  if (m_stopped) return;
  
  CSingleLock lk(m_serverManagerLock);

  MarkServersAsRefreshing();
  BOOST_FOREACH(CPlexServerPtr p, servers)
    MergeServer(p);

  ServerRefreshComplete(connectionType);
  UpdateReachability();
  save();
}

void
CPlexServerManager::UpdateFromDiscovery(CPlexServerPtr server)
{
  if (m_stopped) return;
  
  CSingleLock lk(m_serverManagerLock);

  CPlexServerPtr mergedServer = MergeServer(server);
  NotifyAboutServer(mergedServer);
  SetBestServer(mergedServer, false);
}

CPlexServerPtr
CPlexServerManager::MergeServer(CPlexServerPtr server)
{
  CSingleLock lk(m_serverManagerLock);

  if (m_serverMap.find(server->GetUUID()) != m_serverMap.end())
  {
    CPlexServerPtr existingServer = m_serverMap.find(server->GetUUID())->second;
    existingServer->Merge(server);
    CLog::Log(LOGDEBUG, "CPlexServerManager::MergeServer Merged %s with %d connection, now we have %d total connections.",
              server->GetName().c_str(), server->GetNumConnections(),
              existingServer->GetNumConnections());
    return existingServer;
  }
  else
  {
    m_serverMap[server->GetUUID()] = server;
    CLog::Log(LOGDEBUG, "CPlexServerManager::MergeServer Added a new server %s with %d connections", server->GetName().c_str(), server->GetNumConnections());
    return server;
  }
}

void
CPlexServerManager::ServerRefreshComplete(int connectionType)
{
  vector<CStdString> serversToRemove;

  BOOST_FOREACH(PlexServerPair p, m_serverMap)
  {
    if (!p.second->MarkUpdateFinished(connectionType))
      serversToRemove.push_back(p.first);
  }

  BOOST_FOREACH(CStdString uuid, serversToRemove)
  {
    CLog::Log(LOGDEBUG, "CPlexServerManager::ServerRefreshComplete removing server %s", uuid.c_str());
    NotifyAboutServer(m_serverMap.find(uuid)->second, false);
    m_serverMap.erase(uuid);
  }
}

void
CPlexServerManager::UpdateReachability(bool force)
{
  CSingleLock lk(m_serverManagerLock);

  CLog::Log(LOGDEBUG, "CPlexServerManager::UpdateReachability Updating reachability (force=%s)", force ? "YES" : "NO");
  if (m_reachabilityThreads.size() > 0)
  {
    CLog::Log(LOGWARNING, "CPlexServerManager::UpdateReachability WOW, BAD we are already running reachability tests. We can't do it twice, mmmkey?");
    return;
  }

  m_updateRechabilityForced = force;
  m_reachabilityTestEvent.Reset();

  BOOST_FOREACH(PlexServerPair p, m_serverMap)
  {
    if (!p.second->GetActiveConnection() || force)
      m_reachabilityThreads[p.second->GetUUID()] = new CPlexServerReachabilityThread(shared_from_this(), p.second);
  }
}

void
CPlexServerManager::SetBestServer(CPlexServerPtr server, bool force)
{
  if (!server)
    return;
  
  CSingleLock lk(m_serverManagerLock);
  if (!m_bestServer || force || m_bestServer == server)
  {
    CLog::Log(LOGDEBUG, "CPlexServerManager::SetBestServer bestServer updated to %s", server->toString().c_str());
    m_bestServer = server;

    CGUIMessage msg(GUI_MSG_PLEX_BEST_SERVER_UPDATED, 0, 0);
    msg.SetStringParam(server->GetUUID());
    g_windowManager.SendThreadMessage(msg);
  }
}

void
CPlexServerManager::ClearBestServer()
{
  CLog::Log(LOGDEBUG, "CPlexServerManager::ClearBestServer clearing %s", m_bestServer->toString().c_str());
  m_bestServer.reset();
}

void CPlexServerManager::ServerReachabilityDone(CPlexServerPtr server, bool success)
{
  int reachThreads = 0;

  {
    CSingleLock lk(m_serverManagerLock);
    if (m_reachabilityThreads.find(server->GetUUID()) != m_reachabilityThreads.end())
      m_reachabilityThreads.erase(server->GetUUID());

    reachThreads = m_reachabilityThreads.size();
  }
  
  if (success)
  {
    if (server->GetOwned() &&
        (server->GetServerClass().empty() || !server->GetServerClass().Equals(PLEX_SERVER_CLASS_SECONDARY)))
      SetBestServer(server, false);
    NotifyAboutServer(server);
  }
  else
  {
    if (m_bestServer == server)
      ClearBestServer();
    
    NotifyAboutServer(server, false);
  }

  if (reachThreads == 0)
  {
    CLog::Log(LOGINFO, "CPlexServerManager::ServerRechabilityDone All servers have done their thing. have a nice day now.");
    m_reachabilityTestEvent.Set();

    if (!m_bestServer && !m_updateRechabilityForced)
      UpdateReachability(true);
  }
  else
  {
    CLog::Log(LOGINFO, "CPlexServerManager::ServerRechabilityDone still %d server checking reachability", reachThreads);
  }
}

void
CPlexServerManager::NotifyAboutServer(CPlexServerPtr server, bool added)
{
  CGUIMessage msg(GUI_MSG_PLEX_SERVER_NOTIFICATION, 0, 0, added ? 1 : 0);
  msg.SetStringParam(server->GetUUID());
  g_windowManager.SendThreadMessage(msg);

  if (added)
    g_plexApplication.dataLoader->LoadDataFromServer(server);
  else
    g_plexApplication.dataLoader->RemoveServer(server);
}

void CPlexServerManager::save()
{
  CSingleLock lk(m_serverManagerLock);
  CPlexServerCacheDatabase db;
  if (db.Open())
  {
    db.cacheServers();
    db.Close();
  }
}

void CPlexServerManager::load()
{
  CSingleLock lk(m_serverManagerLock);

  CPlexServerCacheDatabase db;
  if (db.Open())
  {
    PlexServerList list;
    if (db.getCachedServers(list))
    {
      BOOST_FOREACH(CPlexServerPtr server, list)
      {
        CLog::Log(LOGDEBUG, "CPlexServerManager::load found cached server %s", server->GetName().c_str());
        m_serverMap[server->GetUUID()] = server;
      }
    }
    db.Close();
  }
  lk.unlock();

  m_manualServerManager.checkManualServersAsync();
}

////////////////////////////////////////////////////////////////////////////////////////
void CPlexServerManager::Stop()
{
  m_stopped = true;
  save();

  CLog::Log(LOGDEBUG, "CPlexServerManager::Stop asked to stop...");

  CSingleLock lk(m_serverManagerLock);

  if (IsRunningReachabilityTests())
  {
    CLog::Log(LOGDEBUG, "CPlexServerManager::Stop still running reachability tests...");
    std::pair<std::string, CPlexServerReachabilityThread*> p;
    BOOST_FOREACH(p, m_reachabilityThreads)
    {
      CLog::Log(LOGDEBUG, "CPlexServerManager::Stop canceling reachtests for server %s", p.second->m_server->GetName().c_str());
      p.second->m_server->CancelReachabilityTests();
    }

    lk.unlock();

    if (!m_reachabilityTestEvent.WaitMSec(10 * 1000))
    {
      CLog::Log(LOGWARNING, "CPlexServerManager::Stop waited 10 seconds for the reachability stuff to finish, will just kill and move on.");
      return;
    }
  }
}
