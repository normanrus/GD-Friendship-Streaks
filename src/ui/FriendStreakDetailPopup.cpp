#include "FriendStreakDetailPopup.hpp"

#include "FlameNode.hpp"
#include "Ui.hpp"

#include "../Notify.hpp"
#include "../StreakStore.hpp"

#include "Platform.hpp"

#include <Geode/binding/FLAlertLayer.hpp>
#if FSTREAK_CAN_OPEN_WRITE_POPUP
#include <Geode/binding/GJWriteMessagePopup.hpp>
#else
#include <Geode/binding/ProfilePage.hpp>
#endif

using namespace geode::prelude;

namespace fstreak {
namespace {

constexpr float POPUP_WIDTH = 400.f;
constexpr float POPUP_HEIGHT = 340.f;

constexpr int CALENDAR_DAYS = 14;

#if FSTREAK_CAN_HOOK_MESSAGE
constexpr float LEFT_BUTTON_X = -78.f;
constexpr float RIGHT_BUTTON_X = 78.f;
#else
// Three buttons on Windows 2.2091, where the player confirms the send by hand.
constexpr float LEFT_BUTTON_X = -112.f;
constexpr float RIGHT_BUTTON_X = 112.f;
#endif
constexpr float CELL_SIZE = 30.f;
constexpr float CELL_GAP = 9.f;

int64_t serverNow(PairState const& pair) {
    int64_t local = static_cast<int64_t>(std::time(nullptr));
    if (pair.fetchedAt <= 0) return local;
    return pair.serverTime + (local - pair.fetchedAt);
}

cocos2d::ccColor3B statusColor(TodayStatus status) {
    switch (status) {
        case TodayStatus::Done: return ui::COLOR_ALIVE;
        case TodayStatus::WaitingForYou: return ui::COLOR_EMBER;
        case TodayStatus::WaitingForFriend: return ui::COLOR_GOLD;
        default: return ui::COLOR_MUTED;
    }
}

} // namespace

FriendStreakDetailPopup* FriendStreakDetailPopup::create(PairState const& pair) {
    auto ret = new FriendStreakDetailPopup();
    if (ret->init(pair)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void FriendStreakDetailPopup::open(PairState const& pair) {
    if (auto popup = FriendStreakDetailPopup::create(pair)) {
        popup->show();
    }
}

bool FriendStreakDetailPopup::init(PairState const& pair) {
    if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT)) return false;
    m_pair = pair;

    this->setID("friend-streak-detail"_spr);
    this->setTitle(m_pair.friendName.c_str());

    this->buildHeader();
    this->buildCalendar();
    this->buildFooter();

    m_listenerId = StreakStore::get().addListener([this] {
        auto fresh = StreakStore::get().pairForFriend(m_pair.friendId);
        if (!fresh) return;
        m_pair = *fresh;
        this->buildHeader();
        this->buildCalendar();
        this->buildFooter();
    });

    this->schedule(schedule_selector(FriendStreakDetailPopup::onTick), 1.f);
    return true;
}

void FriendStreakDetailPopup::onClose(CCObject* sender) {
    StreakStore::get().removeListener(m_listenerId);
    m_listenerId = 0;
    this->unscheduleAllSelectors();
    Popup::onClose(sender);
}

void FriendStreakDetailPopup::buildHeader() {
    if (auto old = m_mainLayer->getChildByID("detail-header"_spr)) old->removeFromParent();

    auto header = CCNode::create();
    header->setID("detail-header"_spr);
    header->setPosition({POPUP_WIDTH / 2.f, 250.f});
    m_mainLayer->addChild(header, 1);

    auto glow = CCSprite::create("glow.png"_spr);
    if (glow) {
        glow->setScale(0.55f);
        glow->setPosition({-110.f, 6.f});
        glow->setOpacity(m_pair.current > 0 ? 120 : 45);
        glow->setColor(m_pair.current > 0 ? cocos2d::ccColor3B{255, 168, 76} : cocos2d::ccColor3B{120, 120, 120});
        header->addChild(glow, 0);
    }

    auto flame = CCSprite::create("flame.png"_spr);
    if (flame) {
        flame->setScale(0.3f);
        flame->setPosition({-110.f, 6.f});
        if (m_pair.current == 0) flame->setColor({140, 140, 140});
        header->addChild(flame, 1);
    }

    auto count = ui::label(
        fmt::format("{}", m_pair.current), "bigFont.fnt", 1.15f,
        m_pair.current > 0 ? ui::COLOR_GOLD : ui::COLOR_MUTED, {0.f, 0.5f}
    );
    if (count) {
        count->setPosition({-76.f, 14.f});
        header->addChild(count, 2);
    }

    auto caption = ui::label("day streak", "chatFont.fnt", 0.62f, ui::COLOR_MUTED, {0.f, 0.5f});
    if (caption) {
        caption->setPosition({-76.f, -12.f});
        header->addChild(caption);
    }

    auto bestTitle = ui::label("BEST", "chatFont.fnt", 0.5f, ui::COLOR_MUTED, {1.f, 0.5f});
    if (bestTitle) {
        bestTitle->setPosition({160.f, 18.f});
        header->addChild(bestTitle);
    }
    auto bestValue = ui::label(fmt::format("{}", m_pair.best), "bigFont.fnt", 0.8f, ui::COLOR_TEXT, {1.f, 0.5f});
    if (bestValue) {
        bestValue->setPosition({160.f, -8.f});
        header->addChild(bestValue);
    }

    if (auto pill = ui::panel(300.f, 26.f, {255, 255, 255, 24})) {
        pill->setPosition({0.f, -46.f});
        header->addChild(pill, 0);
        auto dot = ui::rect(6.f, 6.f, {statusColor(m_pair.status()).r, statusColor(m_pair.status()).g,
                                       statusColor(m_pair.status()).b, 255});
        if (dot) {
            dot->setPosition({-136.f, -46.f});
            header->addChild(dot, 1);
        }
    }
    auto status = ui::label(todayStatusText(m_pair.status()), "chatFont.fnt", 0.66f, statusColor(m_pair.status()));
    if (status) {
        status->setPosition({0.f, -46.f});
        header->addChild(status, 2);
    }
}

void FriendStreakDetailPopup::buildCalendar() {
    if (m_calendarNode) {
        m_calendarNode->removeFromParent();
        m_calendarNode = nullptr;
    }

    auto node = CCNode::create();
    node->setID("detail-calendar"_spr);
    node->setPosition({POPUP_WIDTH / 2.f, 160.f});
    m_mainLayer->addChild(node, 1);
    m_calendarNode = node;

    std::vector<DayEntry> days(m_pair.days);
    if (days.size() > CALENDAR_DAYS) {
        days.erase(days.begin(), days.end() - CALENDAR_DAYS);
    }

    float rowWidth = CALENDAR_DAYS / 2 * CELL_SIZE + (CALENDAR_DAYS / 2 - 1) * CELL_GAP;
    float startX = -rowWidth / 2.f + CELL_SIZE / 2.f;

    int index = 0;
    for (auto const& entry : days) {
        int column = index % (CALENDAR_DAYS / 2);
        int row = index / (CALENDAR_DAYS / 2);
        CCPoint position = {
            startX + column * (CELL_SIZE + CELL_GAP), row == 0 ? 16.f : -22.f
        };

        auto sprite = CCSprite::create(entry.mutual ? "circle-on.png"_spr : "circle-off.png"_spr);
        if (sprite) {
            sprite->setScale(CELL_SIZE / sprite->getContentSize().width);
            sprite->setPosition(position);
            sprite->setOpacity(entry.mutual ? 255 : 130);
            if (entry.mutual) sprite->setColor({255, 150, 70});
            node->addChild(sprite, 1);
        }

        if (entry.youSent || entry.friendSent) {
            auto half = ui::rect(CELL_SIZE * 0.42f, 3.f, {255, 209, 102, static_cast<GLubyte>(entry.mutual ? 230 : 150)});
            if (half) {
                half->setPosition({position.x, position.y - CELL_SIZE * 0.42f});
                node->addChild(half, 2);
            }
        }

        auto letter = ui::label(std::string(1, weekdayLetter(entry.day)), "chatFont.fnt", 0.4f, ui::COLOR_MUTED);
        if (letter) {
            letter->setPosition({position.x, position.y - CELL_SIZE * 0.5f - 8.f});
            node->addChild(letter, 2);
        }

        if (entry.day == m_pair.todayDay) {
            if (auto ring = CCSprite::create("ring.png"_spr)) {
                ring->setScale(CELL_SIZE / ring->getContentSize().width * 1.25f);
                ring->setPosition(position);
                ring->setOpacity(180);
                node->addChild(ring, 0);
            }
        }
        ++index;
    }
}

void FriendStreakDetailPopup::buildFooter() {
    if (auto old = m_mainLayer->getChildByID("detail-footer"_spr)) old->removeFromParent();

    auto footer = CCNode::create();
    footer->setID("detail-footer"_spr);
    footer->setPosition({POPUP_WIDTH / 2.f, 74.f});
    m_mainLayer->addChild(footer, 1);

    auto started = ui::label(
        fmt::format("Started {}", formatDayShort(m_pair.startDay)), "chatFont.fnt", 0.6f, ui::COLOR_MUTED,
        {0.f, 0.5f}
    );
    if (started) {
        started->setPosition({-164.f, 16.f});
        footer->addChild(started);
    }

    auto last = ui::label(
        m_pair.lastStreakDay.empty() ? "No counted day yet"
                                     : fmt::format("Last counted {}", formatDayShort(m_pair.lastStreakDay)),
        "chatFont.fnt", 0.6f, ui::COLOR_MUTED, {1.f, 0.5f}
    );
    if (last) {
        last->setPosition({164.f, 16.f});
        footer->addChild(last);
    }

    m_deadlineLabel = ui::label("", "chatFont.fnt", 0.6f, ui::COLOR_GOLD);
    if (m_deadlineLabel) {
        m_deadlineLabel->setPosition({0.f, -4.f});
        footer->addChild(m_deadlineLabel);
    }
    this->onTick(0.f);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, -32.f});
    footer->addChild(menu);
#if FSTREAK_CAN_OPEN_WRITE_POPUP
    auto messageLabel = "Send message";
#else
    auto messageLabel = "Profile";
#endif
    if (auto message = ui::button(messageLabel, 0.85f, this, menu_selector(FriendStreakDetailPopup::onMessage))) {
        message->setPosition({LEFT_BUTTON_X, 0.f});
        menu->addChild(message);
    }
#if !FSTREAK_CAN_HOOK_MESSAGE
    if (auto mark = ui::button("Mark written", 0.85f, this, menu_selector(FriendStreakDetailPopup::onMarkWritten))) {
        mark->setPosition({0.f, 0.f});
        menu->addChild(mark);
    }
#endif
    if (auto unlink = ui::button("Unlink", 0.85f, this, menu_selector(FriendStreakDetailPopup::onUnlink))) {
        unlink->setPosition({RIGHT_BUTTON_X, 0.f});
        menu->addChild(unlink);
    }
}

void FriendStreakDetailPopup::onTick(float) {
    if (!m_deadlineLabel) return;

    if (m_pair.mutualToday) {
        m_deadlineLabel->setString("Today is counted. See you tomorrow!");
        m_deadlineLabel->setColor(ui::COLOR_ALIVE);
        return;
    }

    auto remaining = secondsUntilUtcMidnight(serverNow(m_pair));
    m_deadlineLabel->setColor(ui::COLOR_GOLD);
    if (m_pair.current == 0 && m_pair.status() == TodayStatus::Nobody) {
        m_deadlineLabel->setString(fmt::format("Start a new streak today · {} left", formatCountdown(remaining)).c_str());
    } else {
        m_deadlineLabel->setString(fmt::format("Answer before midnight UTC · {}", formatCountdown(remaining)).c_str());
    }
}

void FriendStreakDetailPopup::onMessage(CCObject*) {
#if FSTREAK_CAN_OPEN_WRITE_POPUP
    if (auto popup = GJWriteMessagePopup::create(m_pair.friendId, 0)) {
        popup->show();
    }
#else
    if (auto page = ProfilePage::create(m_pair.friendId, false)) {
        page->show();
    }
#endif
}

#if !FSTREAK_CAN_HOOK_MESSAGE
void FriendStreakDetailPopup::onMarkWritten(CCObject*) {
    StreakStore::get().reportSentForPair(m_pair.id);
    m_pair.youSentToday = true;
    this->buildCalendar();
    this->onTick(0.f);
    notify::toast("Marked as written today");
}
#endif

void FriendStreakDetailPopup::onUnlink(CCObject*) {
    auto* alert = FLAlertLayer::create(
        this, "Unlink friend",
        fmt::format(
            "This removes your shared streak with <cy>{}</c> and resets it to zero. Your best of <cg>{}</c> "
            "days stays on the server.",
            m_pair.friendName, m_pair.best
        ),
        "Cancel", "Unlink", 340.f, true, 160.f, 1.f
    );
    alert->show();
}

void FriendStreakDetailPopup::onUnlinkConfirm(bool confirmed) {
    if (!confirmed) return;
    auto pairId = m_pair.id;
    StreakStore::get().unlink(pairId, [](bool ok) {
        notify::toast(ok ? "Friend unlinked" : "Could not unlink, try again", 2.5f);
    });
    // The list refreshes through the store listener, so closing right away is safe.
    this->onClose(nullptr);
}

void FriendStreakDetailPopup::FLAlert_Clicked(FLAlertLayer* layer, bool btn2) {
    this->onUnlinkConfirm(btn2);
}

} // namespace fstreak
