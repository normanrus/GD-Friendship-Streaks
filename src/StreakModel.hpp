#pragma once
// Wire and cache model for friendship streaks. Mirrors the JSON the server sends.

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/FriendStreakCore.hpp"

namespace fstreak {

inline std::string jsonString(matjson::Value const& value, std::string_view key, std::string fallback = {}) {
    auto result = value[key].asString();
    return result.isOk() ? result.unwrap() : fallback;
}

inline int jsonInt(matjson::Value const& value, std::string_view key, int fallback = 0) {
    auto result = value[key].asInt();
    return result.isOk() ? static_cast<int>(result.unwrap()) : fallback;
}

inline int64_t jsonInt64(matjson::Value const& value, std::string_view key, int64_t fallback = 0) {
    auto result = value[key].asInt();
    return result.isOk() ? static_cast<int64_t>(result.unwrap()) : fallback;
}

inline bool jsonBool(matjson::Value const& value, std::string_view key, bool fallback = false) {
    auto result = value[key].asBool();
    return result.isOk() ? result.unwrap() : fallback;
}

struct DayEntry {
    std::string day;
    bool mutual = false;
    bool youSent = false;
    bool friendSent = false;
};

struct PairState {
    std::string id;
    int youId = 0;
    int friendId = 0;
    std::string youName;
    std::string friendName;
    int current = 0;
    int best = 0;
    std::string startDay;
    std::string lastStreakDay;
    std::string todayDay;
    bool youSentToday = false;
    bool friendSentToday = false;
    bool mutualToday = false;
    std::vector<DayEntry> days;
    int64_t serverTime = 0;
    /// Local epoch when this snapshot arrived, used to tick the deadline offline.
    int64_t fetchedAt = 0;

    /// Highest streak value we already announced, so restarts stay quiet.
    int announcedCurrent = 0;
    /// Day we last sent a "your friend is waiting" reminder.
    std::string remindedDay;

    TodayStatus status() const {
        return todayStatus(youSentToday, friendSentToday);
    }

    matjson::Value toJson() const {
        auto root = matjson::Value::object();
        root["id"] = id;
        root["youId"] = youId;
        root["friendId"] = friendId;
        root["youName"] = youName;
        root["friendName"] = friendName;
        root["current"] = current;
        root["best"] = best;
        root["startDay"] = startDay;
        root["lastStreakDay"] = lastStreakDay;
        root["todayDay"] = todayDay;
        root["youSentToday"] = youSentToday;
        root["friendSentToday"] = friendSentToday;
        root["mutualToday"] = mutualToday;
        root["serverTime"] = serverTime;
        root["fetchedAt"] = fetchedAt;
        root["announcedCurrent"] = announcedCurrent;
        root["remindedDay"] = remindedDay;

        auto list = matjson::Value::array();
        for (auto const& entry : days) {
            auto item = matjson::Value::object();
            item["day"] = entry.day;
            item["mutual"] = entry.mutual;
            item["youSent"] = entry.youSent;
            item["friendSent"] = entry.friendSent;
            list.push(item);
        }
        root["days"] = list;
        return root;
    }

    static std::optional<PairState> fromJson(matjson::Value const& value) {
        PairState state;
        state.id = jsonString(value, "id");
        if (state.id.empty()) return std::nullopt;
        state.youId = jsonInt(value, "youId");
        state.friendId = jsonInt(value, "friendId");
        state.youName = jsonString(value, "youName");
        state.friendName = jsonString(value, "friendName");
        state.current = jsonInt(value, "current");
        state.best = jsonInt(value, "best");
        state.startDay = jsonString(value, "startDay");
        state.lastStreakDay = jsonString(value, "lastStreakDay");
        state.todayDay = jsonString(value, "todayDay");
        state.youSentToday = jsonBool(value, "youSentToday");
        state.friendSentToday = jsonBool(value, "friendSentToday");
        state.mutualToday = jsonBool(value, "mutualToday");
        state.serverTime = jsonInt64(value, "serverTime");
        state.fetchedAt = jsonInt64(value, "fetchedAt");
        state.announcedCurrent = jsonInt(value, "announcedCurrent");
        state.remindedDay = jsonString(value, "remindedDay");
        if (auto list = value["days"].asArray(); list.isOk()) {
            for (auto const& item : list.unwrap()) {
                DayEntry entry;
                entry.day = jsonString(item, "day");
                entry.mutual = jsonBool(item, "mutual");
                entry.youSent = jsonBool(item, "youSent");
                entry.friendSent = jsonBool(item, "friendSent");
                state.days.push_back(std::move(entry));
            }
        }
        return state;
    }

    /// Parses the server's pair payload, which names the two players as "you" and "friend".
    static std::optional<PairState> fromServer(matjson::Value const& value) {
        PairState state;
        state.id = jsonString(value, "id");
        if (state.id.empty()) return std::nullopt;
        auto you = value["you"];
        auto friend_ = value["friend"];
        state.youId = jsonInt(you, "accountId");
        state.youName = jsonString(you, "username");
        state.friendId = jsonInt(friend_, "accountId");
        state.friendName = jsonString(friend_, "username");
        state.current = jsonInt(value, "current");
        state.best = jsonInt(value, "best");
        state.startDay = jsonString(value, "startDay");
        state.lastStreakDay = jsonString(value, "lastStreakDay");
        state.serverTime = jsonInt64(value, "serverTime");
        auto today = value["today"];
        state.todayDay = jsonString(today, "day");
        state.youSentToday = jsonBool(today, "youSent");
        state.friendSentToday = jsonBool(today, "friendSent");
        state.mutualToday = jsonBool(today, "mutual");
        if (auto list = value["days"].asArray(); list.isOk()) {
            for (auto const& item : list.unwrap()) {
                DayEntry entry;
                entry.day = jsonString(item, "day");
                entry.mutual = jsonBool(item, "mutual");
                entry.youSent = jsonBool(item, "youSent");
                entry.friendSent = jsonBool(item, "friendSent");
                state.days.push_back(std::move(entry));
            }
        }
        return state;
    }
};

struct Account {
    int accountId = 0;
    std::string username;
    std::string token;

    bool valid() const {
        return accountId > 0 && !token.empty();
    }

    matjson::Value toJson() const {
        auto root = matjson::Value::object();
        root["accountId"] = accountId;
        root["username"] = username;
        root["token"] = token;
        return root;
    }

    static std::optional<Account> fromJson(matjson::Value const& value) {
        Account account;
        account.accountId = jsonInt(value, "accountId");
        account.username = jsonString(value, "username");
        account.token = jsonString(value, "token");
        if (!account.valid()) return std::nullopt;
        return account;
    }
};

} // namespace fstreak
