#include "StreakInfoPopup.hpp"

#include "../Notify.hpp"
#include "../StreakStore.hpp"
#include "Ui.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace fstreak {
namespace {

constexpr float W = 300.f;
constexpr float H = 244.f;

cocos2d::CCSprite* dayDot(bool alive) {
    auto dot = cocos2d::CCSprite::create(alive ? "circle-on.png"_spr : "circle-off.png"_spr);
    if (!dot) return nullptr;
    dot->setScale(12.f / dot->getContentSize().height);
    dot->setColor(alive ? cocos2d::ccColor3B{124, 214, 138} : cocos2d::ccColor3B{92, 92, 92});
    return dot;
}

CCMenuItemSpriteExtra* textButton(
    char const* text, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler handler, float scale = 0.7f
) {
    auto sprite = ButtonSprite::create(text, "goldFont.fnt", "GJ_button_01.png", scale);
    if (!sprite) return nullptr;
    return CCMenuItemSpriteExtra::create(sprite, target, handler);
}

std::string linkErrorText(std::string const& error) {
    if (error == "self_pair") return "That is your own code.";
    if (error == "invite_not_found") return "No invite with that code.";
    if (error == "invite_expired") return "That code expired.";
    if (error == "already_paired") return "You are already linked.";
    if (error == "no_account") return "GD reports no account signed in.";
    if (error == "no_server") return "Set the Server URL in the settings.";
    if (error == "bad_token") return "Session expired, try again.";
    if (error == "bad_response") return "Unexpected answer from the server.";
    if (error == "request_failed" || error == "timeout") return "Cannot reach the server.";
    if (error.rfind("http_", 0) == 0) return fmt::format("The server answered {}.", error.substr(5));
    return fmt::format("Failed: {}", error);
}

} // namespace

void StreakInfoPopup::open(int friendAccountId, std::string friendName) {
    auto popup = new StreakInfoPopup();
    if (!popup->init(friendAccountId, std::move(friendName))) {
        delete popup;
        return;
    }
    popup->autorelease();
    popup->show();
}

bool StreakInfoPopup::openBest() {
    auto& store = StreakStore::get();
    store.load();

    PairState const* best = nullptr;
    for (auto const& pair : store.pairs()) {
        if (!best || pair.current > best->current) best = &pair;
    }
    if (!best) return false;

    StreakInfoPopup::open(best->friendId, best->friendName);
    return true;
}

bool StreakInfoPopup::init(int friendAccountId, std::string friendName) {
    if (!Popup::init(W, H)) return false;

    this->setID("friendship-streak-info"_spr);
    this->setTitle("Friendship Streak");

    m_friendId = friendAccountId;
    m_friendName = std::move(friendName);

    m_body = cocos2d::CCNode::create();
    m_mainLayer->addChild(m_body, 5);

    this->build();
    return true;
}

void StreakInfoPopup::build() {
    if (!m_body) return;
    m_body->removeAllChildren();

    m_codeLabel = nullptr;
    m_statusLabel = nullptr;
    m_input = nullptr;

    auto& store = StreakStore::get();
    store.load();

    auto pair = store.pairForFriend(m_friendId);
    if (pair) {
        m_pairId = pair->id;
        if (!pair->friendName.empty()) m_friendName = pair->friendName;
        this->buildPaired();
    } else {
        m_pairId.clear();
        this->buildUnpaired();
    }
}

void StreakInfoPopup::buildPaired() {
    auto pair = StreakStore::get().pairForFriend(m_friendId);
    if (!pair) {
        this->buildUnpaired();
        return;
    }

    auto flame = FlameNode::create(36.f, false);
    if (flame) {
        flame->setStreak(pair->current);
        flame->setPosition({56.f, 192.f});
        m_body->addChild(flame, 1);
    }

    auto count = ui::label(
        fmt::format("{}", pair->current), "bigFont.fnt", 1.f, pair->current > 0 ? ui::COLOR_GOLD : ui::COLOR_MUTED, {0.f, 0.5f}
    );
    if (count) {
        count->setPosition({82.f, 192.f});
        m_body->addChild(count, 1);

        auto unit = ui::label("day streak", "chatFont.fnt", 0.5f, ui::COLOR_MUTED, {0.f, 0.5f});
        if (unit) {
            unit->setPosition({82.f + count->getContentSize().width * count->getScaleX() + 8.f, 189.f});
            m_body->addChild(unit, 1);
        }
    }

    if (auto with = ui::label(fmt::format("with {}", m_friendName), "chatFont.fnt", 0.52f, ui::COLOR_MUTED)) {
        with->setPosition({W / 2.f, 166.f});
        m_body->addChild(with, 1);
    }

    if (auto caption = ui::label("last 7 days", "chatFont.fnt", 0.38f, ui::COLOR_MUTED)) {
        caption->setPosition({W / 2.f, 146.f});
        m_body->addChild(caption, 1);
    }

    auto const& days = pair->days;
    size_t shown = std::min<size_t>(days.size(), 7);
    float spacing = 26.f;
    float firstX = W / 2.f - spacing * static_cast<float>(shown - 1) / 2.f;
    for (size_t i = 0; i < shown; ++i) {
        auto const& entry = days[days.size() - shown + i];
        if (auto dot = dayDot(entry.mutual)) {
            dot->setPosition({firstX + spacing * static_cast<float>(i), 130.f});
            m_body->addChild(dot, 1);
        }
    }

    auto stat = [this](float y, char const* name, std::string const& value) {
        if (auto key = ui::label(name, "chatFont.fnt", 0.45f, ui::COLOR_MUTED, {0.f, 0.5f})) {
            key->setPosition({48.f, y});
            m_body->addChild(key, 1);
        }
        if (auto text = ui::label(value, "chatFont.fnt", 0.45f, {255, 255, 255}, {0.f, 0.5f})) {
            text->setPosition({146.f, y});
            m_body->addChild(text, 1);
        }
    };

    stat(104.f, "Best", fmt::format("{} days", pair->best));
    stat(86.f, "Started", pair->startDay.empty() ? "-" : pair->startDay);
    stat(68.f, "Last mutual", pair->lastStreakDay.empty() ? "-" : pair->lastStreakDay);

    std::string today;
    cocos2d::ccColor3B color = ui::COLOR_MUTED;
    if (pair->mutualToday) {
        today = "Today is counted, the flame is safe.";
        color = {124, 214, 138};
    } else if (pair->youSentToday) {
        today = fmt::format("You wrote today, waiting for {}.", m_friendName);
    } else if (pair->friendSentToday) {
        today = fmt::format("{} wrote today, your turn.", m_friendName);
    } else {
        today = "Nobody wrote today yet.";
    }
    if (auto line = ui::label(today, "chatFont.fnt", 0.45f, color)) {
        line->setPosition({W / 2.f, 44.f});
        m_body->addChild(line, 1);
    }

    auto menu = cocos2d::CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_body->addChild(menu, 2);

    if (!pair->youSentToday) {
        if (auto item = textButton("I wrote today", this, menu_selector(StreakInfoPopup::onMarkSent), 0.65f)) {
            item->setPosition({W / 2.f, 20.f});
            menu->addChild(item);
        }
    } else if (auto state = ui::label("Repeat tomorrow to keep it.", "chatFont.fnt", 0.4f, ui::COLOR_MUTED)) {
        state->setPosition({W / 2.f, 20.f});
        m_body->addChild(state, 1);
    }
}

void StreakInfoPopup::buildUnpaired() {
    auto flame = FlameNode::create(36.f, false);
    if (flame) {
        flame->setStreak(0);
        flame->setDimmed(true);
        flame->setPosition({56.f, 192.f});
        m_body->addChild(flame, 1);
    }

    if (auto title = ui::label("No streak yet", "bigFont.fnt", 0.7f, ui::COLOR_MUTED, {0.f, 0.5f})) {
        title->setPosition({82.f, 192.f});
        m_body->addChild(title, 1);
    }

    auto caption = ui::label(
        fmt::format("Write to {} and get a message back\non the same day to keep a shared flame.", m_friendName),
        "chatFont.fnt", 0.45f, ui::COLOR_MUTED
    );
    if (caption) {
        caption->setPosition({W / 2.f, 168.f});
        m_body->addChild(caption, 1);
    }

    m_codeLabel = ui::label("------", "bigFont.fnt", 0.8f, ui::COLOR_GOLD);
    if (m_codeLabel) {
        m_codeLabel->setPosition({W / 2.f, 138.f});
        m_body->addChild(m_codeLabel, 1);
    }

    auto menu = cocos2d::CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_body->addChild(menu, 2);

    if (auto item = textButton("My code", this, menu_selector(StreakInfoPopup::onGetCode), 0.65f)) {
        item->setPosition({W / 2.f - 62.f, 110.f});
        menu->addChild(item);
    }
    if (auto item = textButton("Copy", this, menu_selector(StreakInfoPopup::onCopy), 0.65f)) {
        item->setPosition({W / 2.f + 62.f, 110.f});
        menu->addChild(item);
    }

    if (auto hint = ui::label("or enter your friend's code", "chatFont.fnt", 0.4f, ui::COLOR_MUTED)) {
        hint->setPosition({W / 2.f, 86.f});
        m_body->addChild(hint, 1);
    }

    m_input = geode::TextInput::create(120.f, "CODE", "chatFont.fnt");
    if (m_input) {
        m_input->setPosition({W / 2.f - 40.f, 64.f});
        m_body->addChild(m_input, 1);
    }
    if (auto item = textButton("Link", this, menu_selector(StreakInfoPopup::onLink), 0.65f)) {
        item->setPosition({W / 2.f + 70.f, 64.f});
        menu->addChild(item);
    }

    m_statusLabel = ui::label("", "chatFont.fnt", 0.4f, ui::COLOR_MUTED);
    if (m_statusLabel) {
        m_statusLabel->setPosition({W / 2.f, 38.f});
        m_body->addChild(m_statusLabel, 1);
    }
}

void StreakInfoPopup::onGetCode(cocos2d::CCObject*) {
    auto& store = StreakStore::get();
    if (!store.configured()) {
        if (m_statusLabel) {
            m_statusLabel->setColor(ui::COLOR_DEAD);
            m_statusLabel->setString("Set the Server URL in the settings first.");
        }
        return;
    }

    if (m_statusLabel) {
        m_statusLabel->setColor(ui::COLOR_MUTED);
        m_statusLabel->setString("Asking the server...");
    }

    store.createInvite([this](std::string code, std::string error) {
        if (!m_statusLabel) return;
        if (code.empty()) {
            m_statusLabel->setColor(ui::COLOR_DEAD);
            m_statusLabel->setString(linkErrorText(error).c_str());
            return;
        }
        if (m_codeLabel) m_codeLabel->setString(code.c_str());
        geode::utils::clipboard::write(code);
        m_statusLabel->setColor(ui::COLOR_MUTED);
        m_statusLabel->setString("Code copied. Send it to your friend.");
    });
}

void StreakInfoPopup::onCopy(cocos2d::CCObject*) {
    if (!m_codeLabel) return;
    std::string code = m_codeLabel->getString();
    if (code.empty() || code == "------") {
        if (m_statusLabel) m_statusLabel->setString("Press My code first.");
        return;
    }
    geode::utils::clipboard::write(code);
    if (m_statusLabel) m_statusLabel->setString("Code copied.");
}

void StreakInfoPopup::onLink(cocos2d::CCObject*) {
    if (!m_input) return;
    auto code = m_input->getString();
    if (code.empty()) {
        if (m_statusLabel) m_statusLabel->setString("Enter the code first.");
        return;
    }

    auto& store = StreakStore::get();
    if (!store.configured()) {
        if (m_statusLabel) {
            m_statusLabel->setColor(ui::COLOR_DEAD);
            m_statusLabel->setString("Set the Server URL in the settings first.");
        }
        return;
    }

    if (m_statusLabel) {
        m_statusLabel->setColor(ui::COLOR_MUTED);
        m_statusLabel->setString("Linking...");
    }

    store.redeem(code, [this](bool ok, std::string error) {
        if (!ok) {
            if (m_statusLabel) {
                m_statusLabel->setColor(ui::COLOR_DEAD);
                m_statusLabel->setString(linkErrorText(error).c_str());
            }
            return;
        }
        notify::toast(fmt::format("Streak started with {}", m_friendName), 2.f);
        this->build();
    });
}

void StreakInfoPopup::onMarkSent(cocos2d::CCObject*) {
    if (m_pairId.empty()) return;
    StreakStore::get().reportSentForPair(m_pairId);
    this->build();
}

} // namespace fstreak
