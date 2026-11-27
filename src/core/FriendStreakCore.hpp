#pragma once
// Pure helpers for friendship streaks: no Geode, no JSON, no UI.
// Host-testable, which is why the day math lives here instead of in the mod.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace fstreak {

inline int64_t constexpr kDaySeconds = 86400;

inline std::string zeroPad(int64_t value, int width) {
    std::string text = std::to_string(value);
    while (static_cast<int>(text.size()) < width) text.insert(text.begin(), '0');
    return text;
}

inline std::string makeDay(int64_t year, int64_t month, int64_t date) {
    if (year < 0 || year > 9999) return "?";
    return zeroPad(year, 4) + "-" + zeroPad(month, 2) + "-" + zeroPad(date, 2);
}

/// UTC calendar day of an epoch timestamp, as "YYYY-MM-DD".
inline std::string utcDay(int64_t epoch) {
    time_t raw = static_cast<time_t>(epoch);
    std::tm parts{};
#if defined(_WIN32)
    gmtime_s(&parts, &raw);
#else
    gmtime_r(&raw, &parts);
#endif
    return makeDay(parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday);
}

/// Seconds left until the next UTC midnight, i.e. how long the current streak day lasts.
inline int64_t secondsUntilUtcMidnight(int64_t epoch) {
    int64_t offset = epoch % kDaySeconds;
    if (offset < 0) offset += kDaySeconds;
    return kDaySeconds - offset;
}

/// Days since 1970-01-01 for a "YYYY-MM-DD" string, or nothing if it is malformed.
inline std::optional<int64_t> dayOrdinal(std::string const& day) {
    if (day.size() != 10 || day[4] != '-' || day[7] != '-') return std::nullopt;
    int year = 0, month = 0, date = 0;
    for (int i : {0, 1, 2, 3}) {
        if (day[i] < '0' || day[i] > '9') return std::nullopt;
        year = year * 10 + (day[i] - '0');
    }
    for (int i : {5, 6}) {
        if (day[i] < '0' || day[i] > '9') return std::nullopt;
        month = month * 10 + (day[i] - '0');
    }
    for (int i : {8, 9}) {
        if (day[i] < '0' || day[i] > '9') return std::nullopt;
        date = date * 10 + (day[i] - '0');
    }
    if (month < 1 || month > 12 || date < 1 || date > 31) return std::nullopt;

    // Howard Hinnant's days_from_civil.
    year -= month <= 2;
    int64_t const era = (year >= 0 ? year : year - 399) / 400;
    int64_t const yoe = year - era * 400;
    int64_t const doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + date - 1;
    int64_t const doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

inline std::string dayFromOrdinal(int64_t ordinal) {
    // Inverse of days_from_civil.
    ordinal += 719468;
    int64_t const era = (ordinal >= 0 ? ordinal : ordinal - 146096) / 146097;
    int64_t const doe = ordinal - era * 146097;
    int64_t const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t year = yoe + era * 400;
    int64_t const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t const mp = (5 * doy + 2) / 153;
    int64_t const date = doy - (153 * mp + 2) / 5 + 1;
    int64_t const month = mp + (mp < 10 ? 3 : -9);
    year += month <= 2;

    return makeDay(year, month, date);
}

inline std::string shiftDay(std::string const& day, int days) {
    auto ordinal = dayOrdinal(day);
    if (!ordinal) return {};
    return dayFromOrdinal(*ordinal + days);
}

inline char const* const kMonthNames[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/// "2026-01-05" -> "Jan 5"
inline std::string formatDayShort(std::string const& day) {
    auto ordinal = dayOrdinal(day);
    if (!ordinal) return "?";
    int month = (day[5] - '0') * 10 + (day[6] - '0');
    int date = (day[8] - '0') * 10 + (day[9] - '0');
    if (month < 1 || month > 12) return "?";
    return std::string(kMonthNames[month - 1]) + " " + std::to_string(date);
}

/// Single letter weekday for a day, Monday first ("M", "T", ...).
inline char weekdayLetter(std::string const& day) {
    auto ordinal = dayOrdinal(day);
    if (!ordinal) return '?';
    // 1970-01-01 was a Thursday (index 3 when Monday is 0).
    int index = static_cast<int>(((*ordinal % 7) + 7 + 3) % 7);
    static char const letters[7] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
    return letters[index];
}

/// "3h 12m" style countdown for the streak deadline.
inline std::string formatCountdown(int64_t seconds) {
    if (seconds < 0) seconds = 0;
    int64_t hours = seconds / 3600;
    int64_t minutes = (seconds % 3600) / 60;
    if (hours > 0) return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    if (minutes > 0) return std::to_string(minutes) + "m";
    return std::to_string(seconds % 60) + "s";
}

inline bool isMilestone(int streak) {
    return streak == 7 || streak == 14 || streak == 30 || streak == 50 || streak == 100 ||
           (streak > 100 && streak % 100 == 0);
}

/// Next milestone above the given streak, so the UI can show progress.
inline int nextMilestone(int streak) {
    static int const base[] = {7, 14, 30, 50, 100};
    for (int milestone : base) {
        if (streak < milestone) return milestone;
    }
    return (streak / 100 + 1) * 100;
}

inline int milestoneIndex(int streak) {
    static int const base[] = {7, 14, 30, 50, 100};
    for (int i = 0; i < 5; ++i) {
        if (streak == base[i]) return i;
    }
    return -1;
}

/// Whether the profile page of this player should carry the streak line. Your own
/// page never does: a friendship streak belongs to a pair, and your own score can
/// never be your friend.
inline bool shouldDecorateProfile(bool ownProfile, bool settingOn, int streak) {
    return !ownProfile && settingOn && streak > 0;
}

/// Text of the line under the player name on the profile page.
inline std::string profileBadgeText(int streak) {
    if (streak <= 0) return {};
    return std::to_string(streak) + " day friendship streak";
}

enum class TodayStatus {
    /// Neither side has written today yet.
    Nobody,
    /// You already wrote, your friend has not answered yet.
    WaitingForFriend,
    /// Your friend wrote, you still have to answer.
    WaitingForYou,
    /// Both of you wrote: the day counts.
    Done,
};

inline TodayStatus todayStatus(bool youSent, bool friendSent) {
    if (youSent && friendSent) return TodayStatus::Done;
    if (youSent) return TodayStatus::WaitingForFriend;
    if (friendSent) return TodayStatus::WaitingForYou;
    return TodayStatus::Nobody;
}

inline char const* todayStatusText(TodayStatus status) {
    switch (status) {
        case TodayStatus::Done: return "Counted for today";
        case TodayStatus::WaitingForFriend: return "Waiting for your friend";
        case TodayStatus::WaitingForYou: return "Your friend is waiting for you";
        default: return "Nobody wrote today";
    }
}

/// Colour stage of a streak. Both players derive it from the same day count, so it
/// needs no syncing of its own and survives a restart with the streak itself.
enum class FlameStage {
    Dead,
    Orange,
    Blue,
    Purple,
    Gold,
};

inline FlameStage flameStage(int streak) {
    if (streak <= 0) return FlameStage::Dead;
    if (streak >= 100) return FlameStage::Gold;
    if (streak >= 50) return FlameStage::Purple;
    if (streak >= 25) return FlameStage::Blue;
    return FlameStage::Orange;
}

inline char const* flameStageName(FlameStage stage) {
    switch (stage) {
        case FlameStage::Blue: return "Blue";
        case FlameStage::Purple: return "Purple";
        case FlameStage::Gold: return "Gold";
        case FlameStage::Orange: return "Orange";
        default: return "Dead";
    }
}

/// Milestone announcements, kept here so the text stays consistent everywhere.
inline std::string milestoneMessage(int streak) {
    switch (streak) {
        case 7: return "One week of daily messages!";
        case 14: return "Two weeks in a row!";
        case 30: return "A whole month of friendship!";
        case 50: return "50 days together!";
        case 100: return "100 days. That is dedication!";
        default: return std::to_string(streak) + " days in a row!";
    }
}

} // namespace fstreak
