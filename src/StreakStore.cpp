#include "StreakStore.hpp"

#include "Notify.hpp"

#include <algorithm>
#include <ctime>

using namespace geode::prelude;

namespace fstreak {

namespace {
char const* kAccountKey = "account";
char const* kPairsKey = "pairs";
char const* kPendingKey = "pending";
} // namespace

StreakStore& StreakStore::get() {
    static StreakStore instance;
    return instance;
}

void StreakStore::load() {
    if (m_loaded) return;
    m_loaded = true;

    auto* mod = Mod::get();
    if (auto account = Account::fromJson(mod->getSavedValue<matjson::Value>(kAccountKey))) {
        m_account = *account;
    }

    if (auto saved = mod->getSavedValue<matjson::Value>(kPairsKey); saved.isArray()) {
        for (auto const& item : saved.asArray().unwrap()) {
            if (auto pair = PairState::fromJson(item)) {
                m_pairs.push_back(std::move(*pair));
            }
        }
    }

    if (auto saved = mod->getSavedValue<matjson::Value>(kPendingKey); saved.isArray()) {
        for (auto const& item : saved.asArray().unwrap()) {
            auto pairId = jsonString(item, "pairId");
            auto kind = jsonString(item, "kind");
            if (!pairId.empty() && !kind.empty()) {
                m_pending.emplace_back(pairId, kind);
            }
        }
    }
}

void StreakStore::save() {
    auto* mod = Mod::get();
    mod->setSavedValue(kAccountKey, m_account.valid() ? m_account.toJson() : matjson::Value());
    auto list = matjson::Value::array();
    for (auto const& pair : m_pairs) {
        list.push(pair.toJson());
    }
    mod->setSavedValue(kPairsKey, list);

    auto pending = matjson::Value::array();
    for (auto const& [pairId, kind] : m_pending) {
        auto item = matjson::Value::object();
        item["pairId"] = pairId;
        item["kind"] = kind;
        pending.push(item);
    }
    mod->setSavedValue(kPendingKey, pending);
}

std::optional<PairState> StreakStore::pairForFriend(int friendAccountId) const {
    for (auto const& pair : m_pairs) {
        if (pair.friendId == friendAccountId) return pair;
    }
    return std::nullopt;
}

void StreakStore::onMenuOpened() {
    load();
    if (!configured()) return;
    ensureAccount([this](bool ok) {
        if (!ok) return;
        flushPending();
        refresh();
    });
}

void StreakStore::ensureAccount(std::function<void(bool)> callback) {
    load();
    if (m_account.valid()) {
        if (callback) callback(true);
        return;
    }
    if (m_registering) {
        if (callback) callback(false);
        return;
    }

    auto* manager = GJAccountManager::get();
    if (!manager || manager->m_accountID <= 0) {
        // Streaks need a real GD account: that is the identity shared by both players.
        if (callback) callback(false);
        return;
    }

    m_registering = true;
    auto username = std::string(manager->m_username);
    StreakApi::registerAccount(
        manager->m_accountID, username, [this, callback](std::optional<Account> account) {
            m_registering = false;
            if (!account) {
                if (callback) callback(false);
                return;
            }
            m_account = *account;
            save();
            if (callback) callback(true);
        }
    );
}

void StreakStore::refresh(std::function<void(bool)> callback) {
    load();
    if (!configured() || !m_account.valid()) {
        if (callback) callback(false);
        return;
    }
    if (m_refreshing) {
        if (callback) callback(false);
        return;
    }
    m_refreshing = true;

    StreakApi::listPairs(m_account, [this, callback](ApiResult result) {
        m_refreshing = false;
        if (!result.ok) {
            if (result.error == "bad_token") {
                // The server rotated or lost our token: register again on next sync.
                m_account.token.clear();
                save();
            }
            if (callback) callback(false);
            return;
        }
        std::vector<PairState> fresh;
        if (auto list = result.body["pairs"].asArray(); list.isOk()) {
            for (auto const& item : list.unwrap()) {
                if (auto pair = PairState::fromServer(item)) {
                    fresh.push_back(std::move(*pair));
                }
            }
        }
        applyServerPairs(fresh);
        if (callback) callback(true);
    });
}

void StreakStore::applyServerPairs(std::vector<PairState> const& fresh) {
    std::vector<PairState> merged;
    for (auto incoming : fresh) {
        std::optional<PairState> previous;
        for (auto const& cached : m_pairs) {
            if (cached.id == incoming.id) {
                previous = cached;
                break;
            }
        }
        if (previous) {
            incoming.announcedCurrent = previous->announcedCurrent;
            incoming.remindedDay = previous->remindedDay;
        }
        incoming.fetchedAt = static_cast<int64_t>(std::time(nullptr));
        announce(incoming, previous);
        merged.push_back(std::move(incoming));
    }
    m_pairs = std::move(merged);
    save();
    notifyListeners();
}

void StreakStore::announce(PairState& pair, std::optional<PairState> const& previous) {

    if (pair.current > pair.announcedCurrent) {
        pair.announcedCurrent = pair.current;
        notify::streakIncreased(pair);
    } else if (pair.current < pair.announcedCurrent) {
        int lost = pair.announcedCurrent;
        pair.announcedCurrent = pair.current;
        if (pair.current == 0 && lost > 0) notify::streakBroken(pair, lost);
    }

    // "Your friend already wrote today": one reminder per day and pair.
    if (pair.status() == TodayStatus::WaitingForYou && pair.remindedDay != pair.todayDay) {
        pair.remindedDay = pair.todayDay;
        notify::reminder(pair);
    }
}

void StreakStore::reportMessageSent(int friendAccountId) {
    load();
    auto pair = pairForFriend(friendAccountId);
    if (!pair) return;
    enqueue(pair->id, "sent");
    flushPending();
}

void StreakStore::reportSentForPair(std::string const& pairId) {
    load();
    if (pairId.empty()) return;
    enqueue(pairId, "sent");
    flushPending();
}

void StreakStore::enqueue(std::string const& pairId, std::string const& kind) {
    auto duplicate = std::any_of(m_pending.begin(), m_pending.end(), [&](auto const& item) {
        return item.first == pairId && item.second == kind;
    });
    if (!duplicate) {
        m_pending.emplace_back(pairId, kind);
        save();
    }
}

void StreakStore::flushPending() {
    if (m_flushing || m_pending.empty() || !m_account.valid() || !configured()) return;
    m_flushing = true;
    sendNextPending();
}

void StreakStore::sendNextPending() {
    if (m_pending.empty() || !m_account.valid()) {
        m_flushing = false;
        return;
    }

    auto [pairId, kind] = m_pending.front();
    StreakApi::sendEvent(m_account, pairId, kind, [this, pairId](ApiResult result) {
        if (result.ok) {
            m_pending.erase(
                std::remove_if(
                    m_pending.begin(), m_pending.end(),
                    [&](auto const& item) { return item.first == pairId; }
                ),
                m_pending.end()
            );
            save();
            if (auto pair = PairState::fromServer(result.body)) {
                std::vector<PairState> updated;
                for (auto const& cached : m_pairs) {
                    if (cached.id == pair->id) {
                        pair->announcedCurrent = cached.announcedCurrent;
                        pair->remindedDay = cached.remindedDay;
                        pair->fetchedAt = static_cast<int64_t>(std::time(nullptr));
                        announce(*pair, cached);
                        updated.push_back(*pair);
                    } else {
                        updated.push_back(cached);
                    }
                }
                m_pairs = std::move(updated);
                save();
                notifyListeners();
            }
            sendNextPending();
        } else if (result.error == "pair_not_found") {
            // The friend unlinked: drop the event and the cached pair.
            m_pending.erase(m_pending.begin());
            save();
            sendNextPending();
        } else {
            // Network problem: keep the event queued for the next attempt.
            m_flushing = false;
        }
    });
}

void StreakStore::createInvite(std::function<void(std::string, std::string)> callback) {
    load();
    if (!m_account.valid()) {
        if (callback) callback("", "no_account");
        return;
    }
    StreakApi::createInvite(m_account, [callback](ApiResult result) {
        if (!result.ok) {
            if (callback) callback("", result.error);
            return;
        }
        if (callback) callback(jsonString(result.body, "code"), "");
    });
}

void StreakStore::redeem(std::string code, std::function<void(bool, std::string)> callback) {
    load();
    if (!m_account.valid()) {
        if (callback) callback(false, "no_account");
        return;
    }
    StreakApi::redeem(m_account, code, [this, callback](ApiResult result) {
        if (!result.ok) {
            if (callback) callback(false, result.error);
            return;
        }
        if (auto pair = PairState::fromServer(result.body)) {
            m_pairs.erase(
                std::remove_if(m_pairs.begin(), m_pairs.end(), [&](auto const& p) { return p.id == pair->id; }),
                m_pairs.end()
            );
            m_pairs.push_back(*pair);
            save();
            notifyListeners();
        }
        if (callback) callback(true, "");
        refresh();
    });
}

void StreakStore::unlink(std::string const& pairId, std::function<void(bool)> callback) {
    load();
    if (!m_account.valid()) {
        if (callback) callback(false);
        return;
    }
    StreakApi::unlink(m_account, pairId, [this, pairId, callback](ApiResult result) {
        if (result.ok || result.error == "pair_not_found") {
            m_pairs.erase(
                std::remove_if(m_pairs.begin(), m_pairs.end(), [&](auto const& p) { return p.id == pairId; }),
                m_pairs.end()
            );
            save();
            notifyListeners();
            if (callback) callback(true);
        } else if (callback) {
            callback(false);
        }
    });
}

int StreakStore::addListener(std::function<void()> callback) {
    int id = m_nextListenerId++;
    m_listeners[id] = std::move(callback);
    return id;
}

void StreakStore::removeListener(int id) {
    m_listeners.erase(id);
}

void StreakStore::notifyListeners() {
    for (auto const& [id, listener] : m_listeners) {
        if (listener) listener();
    }
}

} // namespace fstreak
