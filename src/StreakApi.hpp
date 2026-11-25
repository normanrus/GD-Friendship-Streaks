#pragma once
// Thin HTTP layer over the friendship streak server. All calls are async and
// deliver their result on the main thread.

#include <Geode/Geode.hpp>
#include <functional>
#include <optional>
#include <string>

#include "StreakModel.hpp"

namespace fstreak {

struct ApiResult {
    bool ok = false;
    int status = 0;
    /// Server error code ("bad_token", "invite_not_found", ...) or "http_<code>".
    std::string error;
    matjson::Value body = matjson::Value::object();
};

class StreakApi {
public:
    /// Server base URL from settings, without a trailing slash.
    static std::string baseUrl();
    static bool configured();

    static void request(
        std::string const& method, std::string const& path, std::string const& token,
        std::optional<matjson::Value> body, std::function<void(ApiResult)> callback
    );

    static void registerAccount(
        int accountId, std::string const& username, std::function<void(std::optional<Account>)> callback
    );
    static void listPairs(Account const& account, std::function<void(ApiResult)> callback);
    static void createInvite(Account const& account, std::function<void(ApiResult)> callback);
    static void redeem(Account const& account, std::string const& code, std::function<void(ApiResult)> callback);
    static void sendEvent(
        Account const& account, std::string const& pairId, std::string const& kind,
        std::function<void(ApiResult)> callback
    );
    static void unlink(Account const& account, std::string const& pairId, std::function<void(ApiResult)> callback);
};

} // namespace fstreak
