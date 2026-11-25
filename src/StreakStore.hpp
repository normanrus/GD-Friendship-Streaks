#pragma once
// Owns the local mirror of the server state: account, paired friends, cached
// streaks and the queue of message events that still need to be reported.

#include <Geode/Geode.hpp>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "StreakApi.hpp"
#include "StreakModel.hpp"

namespace fstreak {

class StreakStore {
public:
    static StreakStore& get();

    void load();
    void save();

    Account const& account() const { return m_account; }
    bool hasAccount() const { return m_account.valid(); }
    bool configured() const { return StreakApi::configured(); }
    std::vector<PairState> const& pairs() const { return m_pairs; }

    /// Streak of a specific friend, by GD account ID.
    std::optional<PairState> pairForFriend(int friendAccountId) const;

    /// Called once the main menu exists: registers the account if needed and syncs.
    void onMenuOpened();

    void ensureAccount(std::function<void(bool)> callback = {});
    void refresh(std::function<void(bool)> callback = {});

    /// Reports that the player sent a message to the paired friend today.
    void reportMessageSent(int friendAccountId);
    void reportSentForPair(std::string const& pairId);
    void flushPending();

    void createInvite(std::function<void(std::string code, std::string error)> callback);
    void redeem(std::string code, std::function<void(bool ok, std::string error)> callback);
    void unlink(std::string const& pairId, std::function<void(bool ok)> callback);

    /// Registers a change listener; returns the id needed to remove it again.
    int addListener(std::function<void()> callback);
    void removeListener(int id);
    void notifyListeners();

private:
    void applyServerPairs(std::vector<PairState> const& fresh);
    void announce(PairState& pair, std::optional<PairState> const& previous);
    void enqueue(std::string const& pairId, std::string const& kind);
    void sendNextPending();

    Account m_account;
    std::vector<PairState> m_pairs;
    std::vector<std::pair<std::string, std::string>> m_pending;
    std::map<int, std::function<void()>> m_listeners;
    int m_nextListenerId = 1;
    bool m_loaded = false;
    bool m_refreshing = false;
    bool m_flushing = false;
    bool m_registering = false;
};

} // namespace fstreak
