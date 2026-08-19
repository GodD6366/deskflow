/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2014 - 2016 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfigTests.h"

#include "server/Config.h"

#include <sstream>

class OnlySystemFilter : public InputFilter::Condition
{
public:
  Condition *clone() const override
  {
    return new OnlySystemFilter();
  }
  std::string format() const override
  {
    return "";
  }

  InputFilter::FilterStatus match(const Event &ev) override
  {
    return ev.getType() == EventTypes::System ? InputFilter::FilterStatus::Activate
                                              : InputFilter::FilterStatus::NoMatch;
  }
};

using namespace deskflow::server;

void ServerConfigTests::equalityCheck()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(a != b);

  QVERIFY(b.addScreen("screenB"));
  QVERIFY(a != b);

  QVERIFY(a.addScreen("screenB"));
  QVERIFY(a.addScreen("screenC"));
  QVERIFY(a.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(a.connect("screenB", Direction::Left, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(b.addScreen("screenC"));
  QVERIFY(b.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(b.connect("screenB", Direction::Left, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(a.addOption("screenA", kOptionClipboardSharing, 1));
  QVERIFY(b.addOption("screenA", kOptionClipboardSharing, 1));
  QVERIFY(a.addOption(std::string(), kOptionClipboardSharing, 1));
  QVERIFY(b.addOption(std::string(), kOptionClipboardSharing, 1));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  b.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a.addAlias("screenA", "aliasA"));
  QVERIFY(b.addAlias("screenA", "aliasA"));
  /* TODO Fix linking to the proper libs
  NetworkAddress addr1("localhost", 8080);
  addr1.resolve();
  NetworkAddress addr2("localhost", 8080);
  addr2.resolve();
  a.setDeskflowAddress(addr1);
  b.setDeskflowAddress(addr2);
  */
  QVERIFY(a == b);
}

void ServerConfigTests::equalityCheck_diff_options()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addScreen("screenA"));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(a.addOption("screenA", kOptionClipboardSharing, 0));
  QVERIFY(b.addOption("screenA", kOptionClipboardSharing, 1));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_alias()
{
  Config a(nullptr);
  Config b(nullptr);

  QVERIFY(a.addScreen("screenA"));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(b.addAlias("screenA", "aliasA"));
  QVERIFY(a != b);

  QVERIFY(a.addAlias("screenA", "aliasA"));
  QVERIFY(b.addAlias("screenA", "aliasB"));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_filters()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(b.addScreen("screenA"));

  a.getInputFilter()->addFilterRule(InputFilter::Rule{new OnlySystemFilter()});
  QVERIFY(a != b);
}

// TODO FIX
/*
void ServerConfigTests::equalityCheck_diff_address()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(b.addScreen("screenA"));
  a.setDeskflowAddress(NetworkAddress(8000));
  b.setDeskflowAddress(NetworkAddress(9000));
  QVERIFY(a != b);
}
*/

void ServerConfigTests::equalityCheck_diff_neighbours1()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(a.addScreen("screenB"));
  QVERIFY(a.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(b.addScreen("screenB"));
  QVERIFY(a != b);
  QVERIFY(b != a);
}

void ServerConfigTests::equalityCheck_diff_neighbours2()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(a.addScreen("screenB"));
  QVERIFY(a.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(b.addScreen("screenB"));
  QVERIFY(b.connect("screenA", Direction::Bottom, 0.0f, 0.25f, "screenB", 0.25f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::equalityCheck_diff_neighbours3()
{
  Config a(nullptr);
  Config b(nullptr);
  QVERIFY(a.addScreen("screenA"));
  QVERIFY(a.addScreen("screenB"));
  QVERIFY(a.addScreen("screenC"));
  QVERIFY(a.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenB", 0.5f, 1.0f));
  QVERIFY(b.addScreen("screenA"));
  QVERIFY(b.addScreen("screenB"));
  QVERIFY(b.addScreen("screenC"));
  QVERIFY(b.connect("screenA", Direction::Bottom, 0.0f, 0.5f, "screenC", 0.5f, 1.0f));
  QVERIFY(a != b);
}

void ServerConfigTests::displayLayoutReadAndEquality()
{
  std::istringstream input(R"(section: screens
computerA:
computerB:
end
section: displays
computerA:
position = -1920,120
display = 0,0,1920,1080
display = 1920,-240,2560,1440
computerB:
position = 2560,0
display = 0,0,3840,2160
end
)");

  Config config(nullptr);
  input >> config;

  QVERIFY(config.getLayoutPosition("computerA") == Config::LayoutPosition(-1920, 120));
  QCOMPARE(config.getDisplays("computerA").size(), size_t(2));
  QVERIFY(config.getDisplays("computerA").at(1) == Config::ScreenRect({1920, -240, 2560, 1440}));

  Config different(nullptr);
  QVERIFY(different.addScreen("computerA"));
  QVERIFY(different.addScreen("computerB"));
  QVERIFY(config != different);

  QVERIFY(different.setLayoutPosition("computerA", {-1920, 120}));
  QVERIFY(different.addDisplay("computerA", {0, 0, 1920, 1080}));
  QVERIFY(different.addDisplay("computerA", {1920, -240, 2560, 1440}));
  QVERIFY(different.setLayoutPosition("computerB", {2560, 0}));
  QVERIFY(different.addDisplay("computerB", {0, 0, 3840, 2160}));
  QVERIFY(config == different);
}

void ServerConfigTests::displayLayoutSerialization()
{
  Config original(nullptr);
  QVERIFY(original.addScreen("computerA"));
  QVERIFY(original.addScreen("computerB"));
  QVERIFY(original.setLayoutPosition("computerA", {-1920, 120}));
  QVERIFY(original.addDisplay("computerA", {0, 0, 1920, 1080}));
  QVERIFY(original.addDisplay("computerA", {1920, -240, 2560, 1440}));
  QVERIFY(original.setLayoutPosition("computerB", {2560, 0}));
  QVERIFY(original.addDisplay("computerB", {0, 0, 3840, 2160}));

  std::stringstream serialized;
  serialized << original;
  const auto output = QString::fromStdString(serialized.str());
  QVERIFY(output.contains("section: displays"));
  QVERIFY(output.contains("position = -1920,120"));
  QVERIFY(output.contains("display = 1920,-240,2560,1440"));
  QVERIFY(output.contains("position = 2560,0"));
  QVERIFY(output.contains("display = 0,0,3840,2160"));
}

QTEST_MAIN(ServerConfigTests)
