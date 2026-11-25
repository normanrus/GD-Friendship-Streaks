#include "StreakApi.hpp"

#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;
namespace web = geode::utils::web;

namespace fstreak {

std::string StreakApi::baseUrl() {
    auto raw = Mod::get()->getSettingValue<std::string>("server-url");
    while (!raw.empty() && (raw.back() == '/' || raw.back() == ' ')) raw.pop_back();
    while (!raw.empty() && raw.front() == ' ') raw.erase(raw.begin());
    return raw;
}

bool StreakApi::configured() {
    auto url = baseUrl();
    return url.starts_with("http://") || url.starts_with("https://");
}

void StreakApi::request(
    std::string const& method, std::string const& path, std::string const& token,
    std::optional<matjson::Value> body, std::function<void(ApiResult)> callback
) {
    if (!configured()) {
        ApiResult result;
        result.error = "no_server";
        if (callback) callback(std::move(result));
        return;
    }

    auto request = web::WebRequest();
#if defined(GEODE_COMP_GD_VERSION) && GEODE_COMP_GD_VERSION >= 22091
    request.timeout(asp::Duration::fromSecs(12));
#else
    request.timeout(std::chrono::seconds(12));
#endif
    request.header("Accept", "application/json");
    request.userAgent(Mod::get()->getID() + "/" + Mod::get()->getVersion().toNonVString());
    if (!token.empty()) {
        request.header("Authorization", "Bearer " + token);
    }
    if (body) {
        request.header("Content-Type", "application/json");
        request.bodyString(body->dump());
    }

    auto url = baseUrl() + path;
    geode::async::spawn(
        request.send(method, url),
        [callback = std::move(callback)](web::WebResponse response) {
            ApiResult result;
            result.status = response.code();
            if (auto parsed = response.json(); parsed.isOk()) {
                result.body = parsed.unwrap();
            }
            result.ok = response.ok() && result.status >= 200 && result.status < 300;
            if (!result.ok) {
                result.error = jsonString(result.body, "error", "http_" + std::to_string(result.status));
            }
            if (callback) callback(std::move(result));
        }
    );
}

void StreakApi::registerAccount(
    int accountId, std::string const& username, std::function<void(std::optional<Account>)> callback
) {
    auto body = matjson::Value::object();
    body["accountId"] = accountId;
    body["username"] = username;

    request("POST", "/v1/register", "", body, [callback = std::move(callback)](ApiResult result) {
        if (!result.ok) {
            if (callback) callback(std::nullopt);
            return;
        }
        Account account;
        account.accountId = jsonInt(result.body, "accountId");
        account.username = jsonString(result.body, "username");
        account.token = jsonString(result.body, "token");
        if (callback) callback(account.valid() ? std::optional<Account>(account) : std::nullopt);
    });
}

void StreakApi::listPairs(Account const& account, std::function<void(ApiResult)> callback) {
    request("GET", "/v1/pairs", account.token, std::nullopt, std::move(callback));
}

void StreakApi::createInvite(Account const& account, std::function<void(ApiResult)> callback) {
    request("POST", "/v1/pairs/invite", account.token, matjson::Value::object(), std::move(callback));
}

void StreakApi::redeem(
    Account const& account, std::string const& code, std::function<void(ApiResult)> callback
) {
    auto body = matjson::Value::object();
    body["code"] = code;
    request("POST", "/v1/pairs/redeem", account.token, body, std::move(callback));
}

void StreakApi::sendEvent(
    Account const& account, std::string const& pairId, std::string const& kind,
    std::function<void(ApiResult)> callback
) {
    auto body = matjson::Value::object();
    body["kind"] = kind;
    request("POST", "/v1/pairs/" + pairId + "/events", account.token, body, std::move(callback));
}

void StreakApi::unlink(Account const& account, std::string const& pairId, std::function<void(ApiResult)> callback) {
    auto body = matjson::Value::object();
    request("DELETE", "/v1/pairs/" + pairId, account.token, body, std::move(callback));
}

} // namespace fstreak
