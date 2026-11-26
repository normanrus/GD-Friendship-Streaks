#include "StreakStore.hpp"

#include "Notify.hpp"

#include <algorithm>
#include <ctime>

using namespace geode::prelude;

namespace fstreak {

namespace {
char const* kAccountKey = "account";

int64_t nowSeconds() {
    return static_cast<int64_t>(std::time(nullptr));
}
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
        requestRefresh("menu");
    });
}

void StreakStore::enterChat() {
    ++m_chatDepth;
    requestRefresh("chat-open");
}

void StreakStore::leaveChat() {
    if (m_chatDepth > 0) --m_chatDepth;
}

void StreakStore::onTick(float seconds) {
    load();
    if (!configured()) return;

    m_accountAccum += seconds;
    if (m_accountAccum >= 2.f) {
        m_accountAccum = 0.f;
        checkSignedInAccount();
    }

    m_flushAccum += seconds;
    if (m_flushAccum >= 20.f) {
        m_flushAccum = 0.f;
        if (m_account.valid()) flushPending();
    }

    // A request that arrived while another sync was running fires as soon as the
    // minimum gap allows it, instead of waiting for the next poll.
    if (m_refreshQueued && !m_refreshing && nowSeconds() - m_lastAttemptAt >= 3) {
        m_refreshQueued = false;
        m_lastAttemptAt = nowSeconds();
        refresh();
        return;
    }

    // Chat screens change the moment the other player writes, so they poll faster.
    float interval = m_chatDepth > 0 ? 6.f : 20.f;
    m_pollAccum += seconds;
    if (m_pollAccum >= interval) {
        m_pollAccum = 0.f;
        requestRefresh(m_chatDepth > 0 ? "chat-poll" : "poll");
    }
}

void StreakStore::requestRefresh(std::string const& reason) {
    load();
    (void)reason;
    if (!configured()) return;
    if (!m_account.valid()) {
        // Recover by itself: a registration that failed earlier can succeed now.
        if (nowSeconds() - m_lastRegisterAttemptAt < 20) return;
        ensureAccount([this](bool ok) {
            if (!ok) return;
            flushPending();
            requestRefresh("after-register");
        });
        return;
    }

    if (m_refreshing) {
        m_refreshQueued = true;
        return;
    }
    // Collapse bursts (open chat + open card + send + receive) into one request.
    if (nowSeconds() - m_lastAttemptAt < 3) {
        m_refreshQueued = true;
        return;
    }

    m_lastAttemptAt = nowSeconds();
    refresh();
}

void StreakStore::refreshIfStale(int maxAgeSeconds) {
    if (m_lastSyncAt > 0 && nowSeconds() - m_lastSyncAt < maxAgeSeconds) return;
    requestRefresh("stale");
}

void StreakStore::checkSignedInAccount() {
    int signedIn = signedInAccountId();
    if (signedIn <= 0) return;
    if (m_account.valid() && m_account.accountId == signedIn) return;
    if (m_registering) return;
    if (nowSeconds() - m_lastRegisterAttemptAt < 20) return;

    // Another GD account is signed in: the token, the pairs and the queued events
    // all belong to the previous account and must not be shown for this one.
    if (m_account.valid()) {
        dropStaleAccount();
    }
    ensureAccount([this](bool ok) {
        if (!ok) return;
        flushPending();
        requestRefresh("account-switch");
    });
}

int StreakStore::signedInAccountId() {
    auto* manager = GJAccountManager::sharedState();
    if (!manager) manager = GJAccountManager::get();
    return manager ? manager->m_accountID : 0;
}

std::string StreakStore::signedInUsername() {
    auto* manager = GJAccountManager::sharedState();
    if (!manager) manager = GJAccountManager::get();
    return manager ? std::string(manager->m_username) : std::string();
}

void StreakStore::dropStaleAccount() {
    int signedIn = signedInAccountId();
    if (signedIn <= 0 || !m_account.valid() || m_account.accountId == signedIn) return;

    // Another GD account is signed in on this install: the token, the pairs and the
    // queued events all belong to the previous account.
    m_account = Account{};
    m_pairs.clear();
    m_pending.clear();
    save();
    notifyListeners();
}

void StreakStore::ensureAccount(std::function<void(bool)> callback) {
    load();
    dropStaleAccount();
    if (m_account.valid()) {
        if (callback) callback(true);
        return;
    }
    // A registration started by the main menu may still be in flight: wait for it
    // instead of reporting a failure the player cannot act on.
    if (m_registering) {
        if (callback) m_accountWaiters.push_back(callback);
        return;
    }

    auto accountId = signedInAccountId();
    if (accountId <= 0) {
        // Streaks need a real GD account: that is the identity shared by both players.
        m_accountError = "no_account";
        if (callback) callback(false);
        return;
    }

    m_registering = true;
    m_lastRegisterAttemptAt = nowSeconds();
    m_accountError.clear();
    auto username = signedInUsername();
    StreakApi::registerAccount(
        accountId, username,
        [this, callback](std::optional<Account> account, std::string error) {
            m_registering = false;
            if (!account) {
                m_accountError = error.empty() ? "request_failed" : error;
            } else {
                m_account = *account;
                m_accountError.clear();
                save();
            }
            if (callback) callback(account.has_value());
            auto waiters = std::move(m_accountWaiters);
            m_accountWaiters.clear();
            for (auto& waiter : waiters) {
                if (waiter) waiter(account.has_value());
            }
        }
    );
}

void StreakStore::refresh(std::function<void(bool)> callback) {
    load();
    dropStaleAccount();
    if (!configured() || !m_account.valid()) {
        if (callback) callback(false);
        return;
    }
    if (m_refreshing) {
        m_refreshQueued = true;
        if (callback) callback(false);
        return;
    }
    m_refreshing = true;
    m_lastAttemptAt = nowSeconds();

    StreakApi::listPairs(m_account, [this, callback](ApiResult result) {
        m_refreshing = false;
        if (!result.ok) {
            if (result.error == "bad_token") {
                // The server rotated or lost our token: register again straight away.
                m_account.token.clear();
                save();
                m_lastRegisterAttemptAt = 0;
                ensureAccount([this](bool ok) {
                    if (ok) requestRefresh("re-register");
                });
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
        m_lastSyncAt = nowSeconds();
        m_refreshQueued = false;
        applyServerPairs(fresh);
        if (callback) callback(true);
    });
}

void StreakStore::applyServerPairs(std::vector<PairState> const& fresh) {
    std::vector<PairState> merged;
    // Polling every few seconds must not repaint the UI or rewrite save data when
    // nothing actually moved, so the comparison decides both.
    bool changed = fresh.size() != m_pairs.size();

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
            if (
                previous->current != incoming.current || previous->best != incoming.best ||
                previous->startDay != incoming.startDay || previous->lastStreakDay != incoming.lastStreakDay ||
                previous->todayDay != incoming.todayDay || previous->youSentToday != incoming.youSentToday ||
                previous->friendSentToday != incoming.friendSentToday || previous->mutualToday != incoming.mutualToday
            ) {
                changed = true;
            }
        } else {
            changed = true;
        }
        incoming.fetchedAt = static_cast<int64_t>(std::time(nullptr));
        announce(incoming, previous);
        merged.push_back(std::move(incoming));
    }

    m_pairs = std::move(merged);
    if (!changed) return;

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
    dropStaleAccount();
    if (!m_account.valid()) {
        ensureAccount([this, callback](bool ok) {
            if (!ok) {
                if (callback) callback("", m_accountError.empty() ? "no_account" : m_accountError);
                return;
            }
            createInvite(callback);
        });
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
    dropStaleAccount();
    if (!m_account.valid()) {
        ensureAccount([this, code, callback](bool ok) {
            if (!ok) {
                if (callback) callback(false, m_accountError.empty() ? "no_account" : m_accountError);
                return;
            }
            redeem(code, callback);
        });
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
