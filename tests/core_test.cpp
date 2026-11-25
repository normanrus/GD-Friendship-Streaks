// Host tests for the streak helpers. Build and run with:
//   g++ -std=c++20 -O2 -Wall -Wextra -Werror tests/core_test.cpp -o core_tests && ./core_tests

#include "../src/core/FriendStreakCore.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace fstreak;

static void check(bool condition, std::string const& what) {
    if (!condition) {
        std::cerr << "FAILED: " << what << "\n";
        std::exit(1);
    }
}

int main() {
    check(utcDay(0) == "1970-01-01", "epoch is 1970-01-01");
    check(utcDay(1700000000) == "2023-11-14", "1700000000 is 2023-11-14");

    check(dayOrdinal("1970-01-01").value() == 0, "epoch ordinal");
    check(dayOrdinal("2026-01-01").value() == 20454, "2026-01-01 ordinal");
    check(dayOrdinal("2024-02-29").value() == 19782, "leap day ordinal");
    check(dayOrdinal("1969-12-31").value() == -1, "day before epoch");
    check(!dayOrdinal("2026-13-01"), "month 13 is rejected");
    check(!dayOrdinal("2026-01-32"), "day 32 is rejected");
    check(!dayOrdinal("2026-1-01"), "short month is rejected");
    check(!dayOrdinal(""), "empty day is rejected");

    for (std::string day : {"1970-01-01", "2000-02-29", "2026-11-24", "1999-12-31"}) {
        auto ordinal = dayOrdinal(day);
        check(ordinal.has_value(), "round trip parse " + day);
        check(dayFromOrdinal(*ordinal) == day, "round trip format " + day);
    }

    check(shiftDay("2026-12-31", 1) == "2027-01-01", "year rollover");
    check(shiftDay("2026-03-01", -1) == "2026-02-28", "month rollback");
    check(shiftDay("2024-03-01", -1) == "2024-02-29", "leap rollback");
    check(shiftDay("garbage", 1).empty(), "shift of garbage is empty");

    check(secondsUntilUtcMidnight(0) == 86400, "midnight has a full day left");
    check(secondsUntilUtcMidnight(86399) == 1, "one second before midnight");
    check(secondsUntilUtcMidnight(3600) == 82800, "one hour after midnight");

    check(formatDayShort("2026-01-05") == "Jan 5", "short date");
    check(formatDayShort("bad") == "?", "short date of garbage");

    // 2026-01-01 is a Thursday.
    check(weekdayLetter("2026-01-01") == 'T', "thursday letter");
    check(weekdayLetter("2026-01-05") == 'M', "monday letter");
    check(weekdayLetter("2026-01-04") == 'S', "sunday letter");

    check(formatCountdown(0) == "0s", "zero countdown");
    check(formatCountdown(59) == "59s", "seconds countdown");
    check(formatCountdown(60) == "1m", "minute countdown");
    check(formatCountdown(3600 + 12 * 60) == "1h 12m", "hour countdown");
    check(formatCountdown(-5) == "0s", "negative countdown clamps");

    check(isMilestone(7) && isMilestone(14) && isMilestone(30) && isMilestone(50) && isMilestone(100),
          "milestones");
    check(!isMilestone(6) && !isMilestone(8) && !isMilestone(99), "non milestones");
    check(isMilestone(200), "multiples of 100 after 100");

    check(nextMilestone(0) == 7, "next milestone from 0");
    check(nextMilestone(6) == 7, "next milestone from 6");
    check(nextMilestone(7) == 14, "next milestone from 7");
    check(nextMilestone(99) == 100, "next milestone from 99");
    check(nextMilestone(100) == 200, "next milestone from 100");
    check(nextMilestone(250) == 300, "next milestone from 250");

    check(todayStatus(true, true) == TodayStatus::Done, "done");
    check(todayStatus(true, false) == TodayStatus::WaitingForFriend, "waiting for friend");
    check(todayStatus(false, true) == TodayStatus::WaitingForYou, "waiting for you");
    check(todayStatus(false, false) == TodayStatus::Nobody, "nobody");

    check(milestoneMessage(7) == "One week of daily messages!", "milestone message 7");
    check(milestoneMessage(15) == "15 days in a row!", "generic milestone message");

    std::cout << "All friendship streak core tests passed.\n";
    return 0;
}
