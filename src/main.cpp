// Friendship Streaks: daily mutual streaks with your Geometry Dash friends.
//
// Hooks are deliberately small: each one only decorates an existing GD screen
// and never changes vanilla behaviour.

#include <Geode/Geode.hpp>

#include "Platform.hpp"

#include <Geode/modify/FriendsProfilePage.hpp>
#include <Geode/modify/GJMessageCell.hpp>
#if __has_include(<Geode/modify/GJUserCell.hpp>)
#include <Geode/modify/GJUserCell.hpp>
#define FSTREAK_USER_CELL GJUserCell
#else
// GD 2.2091 renamed the friends list row to GJCompactUserCell.
#include <Geode/modify/GJCompactUserCell.hpp>
#define FSTREAK_USER_CELL GJCompactUserCell
#endif
#if FSTREAK_CAN_HOOK_MESSAGE
#include <Geode/modify/GJWriteMessagePopup.hpp>
#endif
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>

#include "Notify.hpp"
#include "StreakStore.hpp"
#include "ui/FlameNode.hpp"
#include "ui/FriendshipStreaksPopup.hpp"
#include "ui/Ui.hpp"
#include "ui/StreakBadge.hpp"

using namespace geode::prelude;

namespace {

constexpr int kFlameTag = 0x5354524B; // "STRK"

template <class Str>
std::string toStdString(Str const& value) {
    return std::string(value.c_str());
}

/// Finds the label that shows the player name so decorations can sit next to it.
CCLabelBMFont* findNameLabel(cocos2d::CCNode* root, std::string const& name) {
    if (!root || name.empty()) return nullptr;
    if (auto children = root->getChildren()) {
        for (auto child : CCArrayExt<CCNode*>(children)) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(child)) {
                if (name == label->getString()) return label;
            }
            if (auto nested = findNameLabel(child, name)) return nested;
        }
    }
    return nullptr;
}

void replaceFlame(cocos2d::CCNode* host, fstreak::PairState const& pair, cocos2d::CCPoint fallback) {
    if (auto old = host->getChildByTag(kFlameTag)) old->removeFromParent();

    auto flame = fstreak::FlameNode::create(20.f, true);
    if (!flame) return;
    flame->setTag(kFlameTag);
    flame->setStreak(pair.current);
    flame->setDimmed(pair.current == 0);
    flame->setPosition(fallback);
    host->addChild(flame, 20);
}

} // namespace

class $modify(FriendshipStreakMenuLayer, MenuLayer) {
    struct Fields {
        fstreak::StreakBadge* badge = nullptr;
        int listenerId = 0;
        geode::ListenerHandle* showBadgeHandle = nullptr;
        geode::ListenerHandle* showCounterHandle = nullptr;
    };

    bool init() {
        if (!MenuLayer::init()) return false;

        auto& store = fstreak::StreakStore::get();
        store.load();

        this->addBadge();

        m_fields->listenerId = store.addListener([this] {
            if (m_fields->badge) m_fields->badge->updateFromStore();
        });

        m_fields->showBadgeHandle = listenForSettingChanges<bool>("show-badge", [this](bool value) {
            if (m_fields->badge) m_fields->badge->setVisible(value);
        });
        m_fields->showCounterHandle = listenForSettingChanges<bool>("show-counter", [this](bool) {
            if (m_fields->badge) m_fields->badge->updateFromStore();
        });

        store.onMenuOpened();
        return true;
    }

    void addBadge() {
        if (!Mod::get()->getSettingValue<bool>("show-badge")) return;

        auto badge = fstreak::StreakBadge::create();
        if (!badge) return;
        badge->setID("friendship-streak-badge"_spr);
        m_fields->badge = badge;

        // Sit just above the profile button so the flame reads as "your streak".
        CCPoint position = {56.f, 96.f};
        if (m_profileButton && m_profileButton->getParent()) {
            auto world = m_profileButton->getParent()->convertToWorldSpace(m_profileButton->getPosition());
            position = this->convertToNodeSpace(world);
            position.x += 2.f;
            position.y += 54.f;
        }
        badge->setPosition(position);
        this->addChild(badge, 100);
    }
};

class $modify(FriendshipStreakUserCell, FSTREAK_USER_CELL) {
    void loadFromScore(GJUserScore* score) {
        FSTREAK_USER_CELL::loadFromScore(score);
        if (!score) return;

        if (auto old = this->getChildByTag(kFlameTag)) old->removeFromParent();
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(score->m_accountID);
        if (!pair) return;

        auto name = toStdString(score->m_userName);
        CCPoint position = {this->getContentSize().width - 56.f, this->getContentSize().height / 2.f};
        if (auto label = findNameLabel(this, name)) {
            position.setPoint(
                label->getPositionX() + label->getContentSize().width * label->getScaleX() * 0.5f + 18.f,
                label->getPositionY()
            );
        }
        replaceFlame(this, *pair, position);
    }
};

class $modify(FriendshipStreakMessageCell, GJMessageCell) {
    void loadFromMessage(GJUserMessage* message) {
        GJMessageCell::loadFromMessage(message);
        if (!message) return;

        if (auto old = this->getChildByTag(kFlameTag)) old->removeFromParent();
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(message->m_accountID);
        if (!pair) return;

        CCPoint position = {this->getContentSize().width - 46.f, this->getContentSize().height - 16.f};
        replaceFlame(this, *pair, position);
    }
};

class $modify(FriendshipStreakProfilePage, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);
        if (!score) return;

        if (auto old = this->getChildByTag(kFlameTag)) old->removeFromParent();
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(score->m_accountID);
        if (!pair) return;

        auto name = toStdString(score->m_userName);
        auto label = findNameLabel(this, name);
        if (!label || !label->getParent()) return;

        auto line = CCLabelBMFont::create(
            fmt::format("{} day friendship streak", pair->current).c_str(), "goldFont.fnt"
        );
        if (!line) return;
        line->setTag(kFlameTag);
        line->setScale(0.42f);
        line->setColor(pair->current > 0 ? fstreak::ui::COLOR_GOLD : fstreak::ui::COLOR_MUTED);
        line->setAnchorPoint({0.f, 0.5f});
        line->setPosition({label->getPositionX(), label->getPositionY() - 20.f});
        label->getParent()->addChild(line, 20);
    }
};

class $modify(FriendshipStreakFriendsPage, FriendsProfilePage) {
    void userListChanged(CCArray* scores, UserListType type) {
        FriendsProfilePage::userListChanged(scores, type);
        if (type != UserListType::Friends) return;
        this->addStreaksButton();
    }

    void addStreaksButton() {
        if (this->getChildByID("friendship-streaks-button"_spr)) return;
        if (!m_refreshBtn || !m_refreshBtn->getParent()) return;

        auto item = fstreak::ui::button(
            "Streaks", 0.75f, this, menu_selector(FriendshipStreakFriendsPage::onStreaks)
        );
        if (!item) return;
        item->setID("friendship-streaks-button"_spr);
        item->setPosition({m_refreshBtn->getPositionX() - 58.f, m_refreshBtn->getPositionY()});
        m_refreshBtn->getParent()->addChild(item);
    }

    void onStreaks(CCObject*) {
        fstreak::FriendshipStreaksPopup::open();
    }
};

#if FSTREAK_CAN_HOOK_MESSAGE
class $modify(FriendshipStreakWriteMessage, GJWriteMessagePopup) {
    void uploadMessageFinished(int accountID) {
        GJWriteMessagePopup::uploadMessageFinished(accountID);
        // A delivered message is the daily interaction that keeps a streak alive.
        fstreak::StreakStore::get().reportMessageSent(accountID);
    }
};
#endif
