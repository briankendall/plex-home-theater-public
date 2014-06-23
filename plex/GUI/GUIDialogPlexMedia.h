#ifndef GUIDIALOGPLEXMEDIA_H
#define GUIDIALOGPLEXMEDIA_H

#include "FileItem.h"

class CGUIDialogPlexMedia
{
public:
  static int ProcessResumeChoice(const CFileItem& file);
  static int ProcessMediaChoice(const CFileItem& file);
  int autoSelectQuality(const CFileItemPtr &item, int target);
};

#endif // GUIDIALOGPLEXMEDIA_H
