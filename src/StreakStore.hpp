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

    /// GD account currently signed in to the game, independent of the server identity.
    static int signedInAccountId();
    static std::string signedInUsername();

    Account const& account() const { return m_account; }
    /// Why the last registration failed: "no_account", "no_server", "http_500", ...
    std::string const& accountError() const { return m_accountError; }
    bool hasAccount() const { return m_account.valid(); }
    bool configured() const { return StreakApi::configured(); }
    std::vector<PairState> const& pairs() const { return m_pairs; }

    /// Streak of a specific friend, by GD account ID.
    std::optional<PairState> pairForFriend(int friendAccountId) const;

    /// Called once the main menu exists: registers the account if needed and syncs.
    void onMenuOpened();

    /// Drives background syncing while the game is open.
    void onTick(float seconds);
    /// A chat-like screen is visible: poll faster and sync right away.
    void enterChat();
    void leaveChat();

    /// Asks for a sync soon; bursts collapse into a single request.
    void requestRefresh(std::string const& reason = {});
    /// Syncs unless the last successful one is younger than maxAge seconds.
    void refreshIfStale(int maxAgeSeconds = 20);
    int64_t lastSyncAt() const { return m_lastSyncAt; }

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
    /// Drops the cached identity when a different GD account is signed in.
    void dropStaleAccount();
    /// Notices account switches while the game is running.
    void checkSignedInAccount();
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
    std::string m_accountError;
    std::vector<std::function<void(bool)>> m_accountWaiters;
    /// Set when a sync was asked for while another one was already running.
    bool m_refreshQueued = false;
    /// Number of chat screens currently open; polling runs faster while it is above zero.
    int m_chatDepth = 0;
    int64_t m_lastSyncAt = 0;
    int64_t m_lastAttemptAt = 0;
    int64_t m_lastRegisterAttemptAt = 0;
    float m_pollAccum = 0.f;
    float m_accountAccum = 0.f;
    float m_flushAccum = 0.f;
};

} // namespace fstreak
