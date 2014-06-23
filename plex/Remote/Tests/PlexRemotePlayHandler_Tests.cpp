#include "PlexTest.h"
#include "PlexRemotePlayHandler.h"

class PlexRemotePlayHandlerOverridden : public CPlexRemotePlayHandler
{
public:
  virtual bool getContainer(const CURL &dirURL, CFileItemList &list)
  {
   return true;
  }
};

class PlexRemotePlayHandlerTests : public ::testing::Test
{
public:
  void SetUp()
  {
    handler = new PlexRemotePlayHandlerOverridden;
  }

  void TearDown()
  {
    delete handler;
  }

  CPlexRemotePlayHandler* handler;
};

TEST_F(PlexRemotePlayHandlerTests, getKeyAndContainer_basic)
{
  ArgMap map;
  map["key"] = "key";
  map["containerKey"] = "containerKey";

  std::string key, container;
  EXPECT_TRUE(handler->getKeyAndContainerUrl(map, key, container));
  EXPECT_STREQ(key.c_str(), "key");
  EXPECT_STREQ(container.c_str(), "containerKey");
}

TEST_F(PlexRemotePlayHandlerTests, getKeyAndContainer_noCKey)
{
  ArgMap map;
  map["key"] = "key";

  std::string key, container;
  EXPECT_TRUE(handler->getKeyAndContainerUrl(map, key, container));
  EXPECT_TRUE(container.empty());
}

TEST_F(PlexRemotePlayHandlerTests, getKeyAndContainer_noKey)
{
  ArgMap map;
  std::string key, container;
  EXPECT_FALSE(handler->getKeyAndContainerUrl(map, key, container));
}

TEST_F(PlexRemotePlayHandlerTests, getKeyAndContainer_ios31hack_basic)
{
  ArgMap map;
  map["key"] = "http://www.youtube.com/key?option=1";
  std::string key, container;
  EXPECT_TRUE(handler->getKeyAndContainerUrl(map, key, container));
  EXPECT_STREQ(key.c_str(), "/key?option=1");
}

TEST_F(PlexRemotePlayHandlerTests, getKeyAndContainer_ios31hack_sameContainer)
{
  ArgMap map;
  map["key"] = "http://www.youtube.com/key?option=1";
  map["containerKey"] = "http://www.youtube.com/key?option=1";
  std::string key, container;
  EXPECT_TRUE(handler->getKeyAndContainerUrl(map, key, container));
  EXPECT_STREQ(key.c_str(), "/key?option=1");
  EXPECT_STREQ(container.c_str(), "/key?option=1");
}

TEST_F(PlexRemotePlayHandlerTests, getItemFromContainer_basic)
{
  CFileItemPtr item = CFileItemPtr(new CFileItem);
  CFileItemList list;

  item->SetProperty("unprocessed_key", "key");
  list.Add(item);

  int i;
  item = handler->getItemFromContainer("key", list, i);
  EXPECT_TRUE(item);
  EXPECT_EQ(i, 0);
  EXPECT_STREQ(item->GetProperty("unprocessed_key").c_str(), "key");
}

TEST_F(PlexRemotePlayHandlerTests, getItemFromContainer_encodedKey)
{
  CFileItemPtr item;
  CFileItemList list;

  item = CFileItemPtr(new CFileItem);
  item->SetProperty("unprocessed_key", "encoded%20key%21"); // encoded key!
  list.Add(item);

  int i;
  item = handler->getItemFromContainer("encoded key!", list, i);
  EXPECT_TRUE(item);
  EXPECT_EQ(i, 0);
  EXPECT_STREQ(item->GetProperty("unprocessed_key").c_str(), "encoded%20key%21");
}

