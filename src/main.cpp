// Friendship Streaks: daily mutual streaks with your Geometry Dash friends.
//
// Hooks are deliberately small: each one only decorates an existing GD screen
// and never changes vanilla behaviour.

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cctype>

#include "Platform.hpp"

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
#if FSTREAK_HAS_THREADS
#include <Geode/modify/GJThreadCell.hpp>
#include <Geode/modify/ThreadViewPopup.hpp>
#else
#include <Geode/modify/GJMessagePopup.hpp>
#endif

#include "Notify.hpp"
#include "StreakStore.hpp"
#include "ui/FlameNode.hpp"
#include "ui/StreakBadge.hpp"
#include "ui/StreakInfoPopup.hpp"
#include "ui/Ui.hpp"

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

/// Same, but tolerant: chats may decorate or shorten the name in the row.
CCLabelBMFont* findLabelContaining(cocos2d::CCNode* root, std::string const& needle) {
    if (!root || needle.empty()) return nullptr;
    std::string wanted = needle;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c) { return std::tolower(c); });
    if (auto children = root->getChildren()) {
        for (auto child : CCArrayExt<CCNode*>(children)) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(child)) {
                std::string text = label->getString();
                std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
                if (!text.empty() && text.find(wanted) != std::string::npos) return label;
            }
            if (auto nested = findLabelContaining(child, needle)) return nested;
        }
    }
    return nullptr;
}

/// Removes a decoration wherever a reused cell or a re-laid-out popup left it.
void removeDecoration(cocos2d::CCNode* root, int tag) {
    if (!root) return;
    if (auto child = root->getChildByTag(tag)) {
        child->removeFromParent();
        return;
    }
    if (auto children = root->getChildren()) {
        for (auto child : CCArrayExt<cocos2d::CCNode*>(children)) {
            removeDecoration(child, tag);
        }
    }
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

#if FSTREAK_CAN_HOOK_USER_CELL
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
#endif

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

#if FSTREAK_HAS_THREADS
/// Flame next to the friend name in a chat list row.
class $modify(FriendshipStreakThreadCell, GJThreadCell) {
    void loadFromThread(GJMessageThread* thread) {
        GJThreadCell::loadFromThread(thread);
        removeDecoration(this, kFlameTag);
        if (!thread || !thread->m_isDM || thread->m_accountID <= 0) return;
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(thread->m_accountID);
        if (!pair) return;

        auto name = toStdString(thread->m_userName);
        auto label = findNameLabel(this, name);
        if (!label) label = findLabelContaining(this, name);
        if (!label || !label->getParent()) return;

        constexpr float SIZE = 15.f;
        auto box = label->boundingBox();
        float x = box.getMaxX() + 8.f + SIZE * 0.5f;
        // The right side of the row keeps its timestamp and unread badge.
        if (x + SIZE > this->getContentSize().width - 96.f) return;

        auto flame = fstreak::FlameNode::create(SIZE, true);
        if (!flame) return;
        flame->setTag(kFlameTag);
        flame->setStreak(pair->current);
        flame->setPosition({x, box.getMidY()});
        label->getParent()->addChild(flame, 20);
    }
};

/// Flame next to the title of an open chat; tapping it opens the streak card.
class $modify(FriendshipStreakThreadView, ThreadViewPopup) {
    bool init(GJMessageThread* thread) {
        if (!ThreadViewPopup::init(thread)) return false;
        this->placeChip();
        return true;
    }

    void layoutTitleRow() {
        ThreadViewPopup::layoutTitleRow();
        this->placeChip();
    }

    void placeChip() {
        removeDecoration(this, kFlameTag);

        auto thread = m_thread;
        if (!thread || !thread->m_isDM || !m_titleLabel || !m_titleLabel->getParent()) return;
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto host = m_titleLabel->getParent();
        if (auto old = host->getChildByTag(kFlameTag)) old->removeFromParent();

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(thread->m_accountID);

        constexpr float SIZE = 18.f;
        auto flame = fstreak::FlameNode::create(SIZE, true);
        if (!flame) return;
        if (pair) {
            flame->setStreak(pair->current);
        } else {
            flame->setStreak(0);
            flame->setDimmed(true);
        }

        auto item = CCMenuItemSpriteExtra::create(
            flame, this, menu_selector(FriendshipStreakThreadView::onChip)
        );
        if (!item) return;

        auto box = m_titleLabel->boundingBox();
        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({box.getMaxX() + 6.f + SIZE * 0.5f, box.getMidY()});
        menu->addChild(item);
        menu->setTag(kFlameTag);
        host->addChild(menu, 20);
    }

    void onChip(cocos2d::CCObject*) {
        if (!m_thread) return;
        fstreak::StreakInfoPopup::open(m_thread->m_accountID, toStdString(m_thread->m_userName));
    }
};
#else
/// GD 2.2081 has no threads, so the flame sits in the open message window.
class $modify(FriendshipStreakMessagePopup, GJMessagePopup) {
    struct Fields {
        int friendId = 0;
        std::string friendName;
    };

    bool init(GJUserMessage* message) {
        if (!GJMessagePopup::init(message)) return false;
        if (!message || message->m_accountID <= 0) return true;

        m_fields->friendId = message->m_accountID;
        m_fields->friendName = toStdString(message->m_username);

        removeDecoration(this, kFlameTag);
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return true;

        auto& store = fstreak::StreakStore::get();
        store.load();
        auto pair = store.pairForFriend(message->m_accountID);

        auto flame = fstreak::FlameNode::create(18.f, true);
        if (!flame) return true;
        if (pair) {
            flame->setStreak(pair->current);
        } else {
            flame->setStreak(0);
            flame->setDimmed(true);
        }

        auto item = CCMenuItemSpriteExtra::create(
            flame, this, menu_selector(FriendshipStreakMessagePopup::onChip)
        );
        if (!item) return true;

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({this->getContentSize().width / 2.f - 96.f, this->getContentSize().height - 52.f});
        menu->addChild(item);
        menu->setTag(kFlameTag);
        this->addChild(menu, 20);
        return true;
    }

    void onChip(cocos2d::CCObject*) {
        fstreak::StreakInfoPopup::open(m_fields->friendId, m_fields->friendName);
    }
};
#endif

#if FSTREAK_CAN_HOOK_MESSAGE
class $modify(FriendshipStreakWriteMessage, GJWriteMessagePopup) {
    void uploadMessageFinished(int accountID) {
        GJWriteMessagePopup::uploadMessageFinished(accountID);
        // A delivered message is the daily interaction that keeps a streak alive.
        fstreak::StreakStore::get().reportMessageSent(accountID);
    }
};
#endif
