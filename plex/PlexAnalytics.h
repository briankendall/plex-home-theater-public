#ifndef PLEXANALYTICS_H
#define PLEXANALYTICS_H

#include "interfaces/AnnouncementManager.h"
#include "interfaces/IAnnouncer.h"
#include "utils/UrlOptions.h"
#include "Utility/PlexTimer.h"
#include "PlexGlobalTimer.h"
#include "Stopwatch.h"

class CPlexAnalytics : public ANNOUNCEMENT::IAnnouncer, public IPlexGlobalTimeout
{
  public:
    CPlexAnalytics();
    void didUpgradeEvent(bool success, const std::string& fromVersion, const std::string& toVersion, bool delta);  
    void startLogging();
    void stopLogging();

    CStdString TimerName() const { return "analytics"; }

  private:
    void setCustomDimensions(CUrlOptions &options);
    void trackEvent(const std::string& category, const std::string& action, const std::string& label, int64_t value, const CUrlOptions &arg = CUrlOptions());
    void sendPing();
    void sendTrackingRequest(const CUrlOptions &request);    
    void sendPlaybackStop();

    // IAnnouncer interface
    void Announce(ANNOUNCEMENT::AnnouncementFlag flag, const char *sender, const char *message, const CVariant &data);

    // ITimerCallback interface
    void OnTimeout();

    CUrlOptions m_baseOptions;
    bool m_firstEvent;

    int64_t m_numberOfPlays;
    CPlexTimer m_sessionLength;

    CFileItemPtr m_currentItem;

    CStopWatch m_playStopWatch;
    int64_t m_cumulativeTimePlayed;
};

#endif // PLEXANALYTICS_H
