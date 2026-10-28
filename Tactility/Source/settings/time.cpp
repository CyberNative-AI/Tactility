#include <service/paths.h>

#include <Tactility/settings/RadioSettings.h>
#include <Tactility/settings/Time.h>
#include <Tactility/settings/SystemSettings.h>

#include <tactility/paths.h>
#include <tactility/preferences.h>
#include <tactility/log.h>
#include <tactility/system_event.h>

#include <lvgl/widgets/statusbar.h>

#include <generated_assets.h>

#include <string_view>

#ifdef ESP_PLATFORM
#include <ctime>
#endif

namespace tt::settings {

constexpr auto* TAG = "Time";

constexpr auto* TIMEZONE_PREFERENCES_KEY_NAME = "tz_name";
constexpr auto* TIMEZONE_PREFERENCES_KEY_CODE = "tz_code";
constexpr auto* TIMEZONE_PREFERENCES_KEY_TIME24 = "tz_time24";

namespace {

// Same "time" namespace/file that Ntp.cpp's storeTimeInNvs()/setTimeFromNvs() use for
// "syncTime" - matches the shared NVS namespace this used to be.
bool getPreferencesPath(std::string& outPath) {
    char root[128];
    // Not really a service, but this is the best way of organising it for now
    if (paths_get_data_path(root, sizeof(root)) != ERROR_NONE) {
        return false;
    }
    outPath = std::string(root) + "/settings/time.properties";
    return true;
}

void saveTimeZoneCode(const std::string& code) {
    std::string path;
    if (getPreferencesPath(path)) {
        Preferences* preferences = preferences_open(path.c_str());
        if (preferences != nullptr) {
            preferences_put_string(preferences, TIMEZONE_PREFERENCES_KEY_CODE, code.c_str());
            preferences_close(preferences);
        }
    }
}

} // namespace

bool findTimeZoneCode(std::string_view csv, const std::string& name, std::string& outCode) {
    // Lines look like "Europe/Amsterdam","CET-1CEST,M3.5.0,M10.5.0/3"
    const std::string prefix = "\"" + name + "\",\"";
    size_t start = 0;
    while (start < csv.size()) {
        size_t end = csv.find('\n', start);
        if (end == std::string_view::npos) {
            end = csv.size();
        }
        auto line = csv.substr(start, end - start);
        if (line.ends_with('\r')) {
            line.remove_suffix(1); // CRLF line ending
        }
        if (line.starts_with(prefix) && line.size() > prefix.size() && line.ends_with('"')) {
            outCode = line.substr(prefix.size(), line.size() - prefix.size() - 1);
            return true;
        }
        start = end + 1;
    }
    return false;
}

bool findTimeZoneCode(const std::string& name, std::string& outCode) {
    const std::string_view csv(reinterpret_cast<const char*>(asset_timezones_csv), asset_timezones_csv_size);
    return findTimeZoneCode(csv, name, outCode);
}

void initTimeZone() {
    auto code = getTimeZoneCode();
    // The saved code is a copy of the timezones.csv row from when the time zone was picked. When a
    // country changes its rules, a firmware update ships a new row, so use that for the same name.
    std::string bundled_code;
    if (hasTimeZone() && findTimeZoneCode(getTimeZoneName(), bundled_code) && bundled_code != code) {
        LOG_I(TAG, "Updating the time zone code from %s to %s", code.c_str(), bundled_code.c_str());
        saveTimeZoneCode(bundled_code);
        code = bundled_code;
    }
#ifdef ESP_PLATFORM
    if (!code.empty()) {
        setenv("TZ", code.c_str(), 1);
        tzset();
    }
#endif
}

void setTimeZone(const std::string& name, const std::string& code) {
    std::string path;
    if (getPreferencesPath(path)) {
        Preferences* preferences = preferences_open(path.c_str());
        if (preferences != nullptr) {
            preferences_put_string(preferences, TIMEZONE_PREFERENCES_KEY_NAME, name.c_str());
            preferences_put_string(preferences, TIMEZONE_PREFERENCES_KEY_CODE, code.c_str());
            preferences_close(preferences);
        }
    }

#ifdef ESP_PLATFORM
    setenv("TZ", code.c_str(), 1);
    tzset();
#endif

    // The time zone tells where the device is, and so which radio band it may use
    if (radio::hasRadio()) {
        const RadioRegion region = radio::findRegionForTimeZone(name);
        if (region != RADIO_REGION_UNSET && !radio::saveRegion(region)) {
            LOG_E(TAG, "Failed to save the radio region");
        }
    }

    system_event_emit(KERNEL_EVENT_TIME_CHANGED, nullptr, 0);
}

std::string getTimeZoneName() {
    std::string path;
    if (getPreferencesPath(path)) {
        Preferences* preferences = preferences_open(path.c_str());
        if (preferences != nullptr) {
            char buffer[64];
            error_t error = preferences_opt_string(preferences, TIMEZONE_PREFERENCES_KEY_NAME, buffer, sizeof(buffer));
            preferences_close(preferences);
            if (error == ERROR_NONE) {
                return buffer;
            }
        }
    }
    return "Europe/Amsterdam";
}

bool hasTimeZone() {
    std::string path;
    if (!getPreferencesPath(path)) {
        return false;
    }
    Preferences* preferences = preferences_open(path.c_str());
    if (preferences == nullptr) {
        return false;
    }
    bool has = preferences_has_string(preferences, TIMEZONE_PREFERENCES_KEY_NAME);
    preferences_close(preferences);
    return has;
}

std::string getTimeZoneCode() {
    std::string path;
    if (getPreferencesPath(path)) {
        Preferences* preferences = preferences_open(path.c_str());
        if (preferences != nullptr) {
            char buffer[64];
            error_t error = preferences_opt_string(preferences, TIMEZONE_PREFERENCES_KEY_CODE, buffer, sizeof(buffer));
            preferences_close(preferences);
            if (error == ERROR_NONE) {
                return buffer;
            }
        }
    }
    return "CET-1CEST,M3.5.0,M10.5.0/3";  // Default: Europe/Amsterdam
}

bool isTimeFormat24Hour() {
    SystemSettings properties;
    if (!loadSystemSettings(properties)) {
        return true;
    } else {
        return properties.timeFormat24h;
    }
}

void setTimeFormat24Hour(bool show24Hour) {
    SystemSettings properties;
    if (!loadSystemSettings(properties)) {
        return;
    }

    properties.timeFormat24h = show24Hour;
    saveSystemSettings(properties);
    lvgl_statusbar_set_time_format_24h(show24Hour);
}

}
