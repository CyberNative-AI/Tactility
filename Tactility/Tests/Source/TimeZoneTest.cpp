#include "doctest.h"

#include <Tactility/settings/Time.h>
#include <Tactility/settings/TimePrivate.h>

#include <tactility/paths.h>
#include <tactility/preferences.h>

#include <string>

using namespace tt;

namespace {

/** Writes the time zone like an older firmware did, without the radio region and event of setTimeZone() */
void saveTimeZone(const char* name, const char* code) {
    char root[128];
    REQUIRE(paths_get_data_path(root, sizeof(root)) == ERROR_NONE);
    const std::string path = std::string(root) + "/settings/time.properties";
    Preferences* preferences = preferences_open(path.c_str());
    REQUIRE(preferences != nullptr);
    preferences_put_string(preferences, "tz_name", name);
    preferences_put_string(preferences, "tz_code", code);
    preferences_close(preferences);
}

} // namespace

TEST_CASE("findTimeZoneCode finds the bundled code of a time zone") {
    std::string code;
    CHECK(settings::findTimeZoneCode("Europe/Amsterdam", code));
    CHECK_EQ(code, "CET-1CEST,M3.5.0,M10.5.0/3");
    CHECK(settings::findTimeZoneCode("Africa/Abidjan", code)); // first row
    CHECK_EQ(code, "GMT0");
    CHECK(settings::findTimeZoneCode("Etc/Zulu", code)); // last row
    CHECK_EQ(code, "UTC0");
}

TEST_CASE("findTimeZoneCode only matches whole names") {
    std::string code = "unchanged";
    CHECK_FALSE(settings::findTimeZoneCode("Europe/Amster", code));
    CHECK_FALSE(settings::findTimeZoneCode("Amsterdam", code));
    CHECK_FALSE(settings::findTimeZoneCode("", code));
    CHECK_FALSE(settings::findTimeZoneCode("Mars/Olympus_Mons", code));
    CHECK_EQ(code, "unchanged");
    CHECK(settings::findTimeZoneCode("Etc/GMT+1", code));
    CHECK_EQ(code, "<-01>1");
}

TEST_CASE("initTimeZone replaces an outdated saved code with the bundled one") {
    saveTimeZone("America/Winnipeg", "CST6CDT,M3.2.0,M11.1.0");
    settings::initTimeZone();
    std::string bundled;
    REQUIRE(settings::findTimeZoneCode("America/Winnipeg", bundled));
    CHECK_EQ(bundled, "EST5"); // CST all year from 1 November 2026 (IANA tz 2026e)
    CHECK_EQ(settings::getTimeZoneCode(), bundled);
    CHECK_EQ(settings::getTimeZoneName(), "America/Winnipeg");
}

TEST_CASE("initTimeZone keeps a saved code that is current or not in the list") {
    saveTimeZone("Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3");
    settings::initTimeZone();
    CHECK_EQ(settings::getTimeZoneCode(), "CET-1CEST,M3.5.0,M10.5.0/3");

    saveTimeZone("Custom/Zone", "<+0530>-5:30");
    settings::initTimeZone();
    CHECK_EQ(settings::getTimeZoneCode(), "<+0530>-5:30");
    CHECK_EQ(settings::getTimeZoneName(), "Custom/Zone");
}
