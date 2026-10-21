#pragma once

#include <string>

namespace tt::settings {

/** Applies the saved time zone, first updating its code to the bundled timezones.csv row for its name */
void initTimeZone();

/**
 * Find the code of a time zone in the bundled timezones.csv
 * @param[in] name the time zone name, e.g. "Europe/Amsterdam"
 * @param[out] outCode the code, set only when found
 * @return true when found
 */
bool findTimeZoneCode(const std::string& name, std::string& outCode);

} // namespace tt::settings
