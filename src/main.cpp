// Friendship Streaks: daily mutual streaks with your Geometry Dash friends.
//
// Hooks are deliberately small: each one only decorates an existing GD screen
// and never changes vanilla behaviour.

#include <Geode/Geode.hpp>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

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
#if __has_include(<Geode/binding/CustomListView.hpp>)
#include <Geode/binding/CustomListView.hpp>
#endif
#if FSTREAK_HAS_THREADS
#include <Geode/modify/GJThreadCell.hpp>
#include <Geode/modify/MessageThreadsPage.hpp>
#include <Geode/modify/ThreadViewPopup.hpp>
#else
#include <Geode/modify/GJMessagePopup.hpp>
#endif

#include "Notify.hpp"
#include "StreakStore.hpp"
#include "ui/FlameNode.hpp"
#include "ui/StreakInfoPopup.hpp"
#include "ui/Ui.hpp"

using namespace geode::prelude;

namespace {

constexpr int kFlameTag = 0x5354524B; // "STRK"

// Redraw callbacks of the chat surfaces that are on screen right now. Keeping the
// callbacks instead of the node pointers means a screen that has been closed can
// never be touched.
std::function<void()> g_refreshChat;
std::function<void()> g_refreshThreadList;

template <class Str>
std::string toStdString(Str const& value) {
    return std::string(value.c_str());
}

std::string labelText(CCLabelBMFont* label) {
    return label ? std::string(label->getString()) : std::string();
}

std::string lowercased(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool textMatches(std::string const& text, std::string const& name) {
    auto haystack = lowercased(text);
    auto needle = lowercased(name);
    if (needle.empty() || haystack.empty()) return false;
    return haystack == needle || haystack.find(needle) != std::string::npos;
}

void collectLabels(cocos2d::CCNode* root, std::vector<CCLabelBMFont*>& out) {
    if (!root) return;
    if (auto children = root->getChildren()) {
        for (auto child : CCArrayExt<cocos2d::CCNode*>(children)) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(child)) out.push_back(label);
            collectLabels(child, out);
        }
    }
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

/// Bounding box of a node in world space, so nodes from different parents compare.
[[maybe_unused]] cocos2d::CCRect worldBox(cocos2d::CCNode* node) {
    auto box = node->boundingBox();
    if (auto parent = node->getParent()) {
        auto low = parent->convertToWorldSpace({box.getMinX(), box.getMinY()});
        auto high = parent->convertToWorldSpace({box.getMaxX(), box.getMaxY()});
        return cocos2d::CCRect(low.x, low.y, high.x - low.x, high.y - low.y);
    }
    return box;
}

bool usesChatFont(CCLabelBMFont* label) {
    if (!label) return false;
    return std::string(label->m_sFntFile).find("chatFont") != std::string::npos;
}

#if FSTREAK_HAS_THREADS
/// The chat row draws its preview and timestamp with chatFont.fnt, so the label
/// that carries the friend name is the other one that matches the thread.
CCLabelBMFont* threadNameLabel(cocos2d::CCNode* cell, std::string const& name) {
    std::vector<CCLabelBMFont*> labels;
    collectLabels(cell, labels);

    bool hasName = !name.empty();
    CCLabelBMFont* best = nullptr;
    float bestY = -1e9f;
    CCLabelBMFont* fallback = nullptr;

    for (auto label : labels) {
        if (usesChatFont(label)) {
            // The preview and the timestamp: never a good place for the flame.
            if (hasName && !fallback && textMatches(labelText(label), name)) {
                fallback = label;
            }
            continue;
        }
        if (hasName && !textMatches(labelText(label), name)) continue;
        if (label->getPositionY() > bestY) {
            bestY = label->getPositionY();
            best = label;
        }
    }
    // Some threads carry the name on the other member instead of m_userName; the
    // top text of the row is the friend's name in that case.
    return best ? best : fallback;
}

/// Left edge of everything that must stay readable on the right of a row.
float rowRightLimit(GJThreadCell* cell, cocos2d::CCNode* host) {
    float limit = host->convertToNodeSpace(
        cell->convertToWorldSpace({cell->getContentSize().width - 12.f, 0.f})
    ).x;

    std::vector<CCLabelBMFont*> labels;
    collectLabels(cell, labels);
    for (auto label : labels) {
        if (!usesChatFont(label)) continue;
        auto box = worldBox(label);
        float left = host->convertToNodeSpace({box.getMinX(), box.getMidY()}).x;
        limit = std::min(limit, left - 4.f);
    }

    auto children = cell->getChildren();
    if (!children) return limit;
    for (auto child : CCArrayExt<cocos2d::CCNode*>(children)) {
        auto sprite = typeinfo_cast<cocos2d::CCSprite*>(child);
        if (!sprite) continue;
        // The unread bubble and the muted/star marks also live on the right side.
        auto box = worldBox(sprite);
        if (box.getMinX() < cell->convertToWorldSpace({cell->getContentSize().width * 0.55f, 0.f}).x) {
            continue;
        }
        float left = host->convertToNodeSpace({box.getMinX(), box.getMidY()}).x;
        limit = std::min(limit, left - 4.f);
    }
    return limit;
}

/// Flame and day count next to the friend name in a chat list row.
void decorateThreadCell(GJThreadCell* cell) {
    if (!cell) return;
    removeDecoration(cell, kFlameTag);

    auto thread = cell->m_thread;
    if (!thread || !thread->m_isDM || thread->m_accountID <= 0) return;
    if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

    auto pair = fstreak::StreakStore::get().pairForFriend(thread->m_accountID);
    // No active streak, no indicator: the row stays exactly as vanilla draws it.
    if (!pair || pair->current <= 0) return;

    auto label = threadNameLabel(cell, toStdString(thread->m_userName));
    if (!label || !label->getParent()) return;

    auto host = label->getParent();
    auto box = label->boundingBox();

    constexpr float SIZE = 15.f;
    float half = SIZE * 0.5f;
    float x = box.getMaxX() + 6.f + half;
    float limit = rowRightLimit(cell, host);
    if (x + half > limit) return;

    auto flame = fstreak::FlameNode::create(SIZE, true);
    if (!flame) return;
    flame->setTag(kFlameTag);
    flame->setStreak(pair->current);
    flame->setPosition({x, box.getMidY()});
    host->addChild(flame, 20);
}

/// Flame next to the name in the open chat, on the same line as the game's own
/// header buttons. Tapping it opens the streak card.
void placeChatChip(ThreadViewPopup* popup, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler handler) {
    if (!popup) return;
    removeDecoration(popup, kFlameTag);

    auto thread = popup->m_thread;
    auto host = popup->m_mainLayer;
    if (!thread || !thread->m_isDM || !host || !popup->m_titleLabel) return;
    if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;
    if (!popup->m_titleLabel->getParent()) return;

    // The header row is the line the game puts its own buttons on, so it is the
    // only vertical anchor that cannot drift from the title. Buttons that sit
    // lower in the popup (send, scroll to bottom) are not part of that row.
    auto titleBox = worldBox(popup->m_titleLabel);
    float headerY = host->convertToNodeSpace({titleBox.getMidX(), titleBox.getMidY()}).y;

    std::vector<cocos2d::CCNode*> headerRow;
    float rowTop = -1e9f;
    if (auto menu = popup->m_buttonMenu) {
        if (auto children = menu->getChildren()) {
            for (auto child : CCArrayExt<cocos2d::CCNode*>(children)) {
                if (!child || !child->isVisible()) continue;
                rowTop = std::max(rowTop, worldBox(child).getMaxY());
            }
            for (auto child : CCArrayExt<cocos2d::CCNode*>(children)) {
                if (!child || !child->isVisible()) continue;
                if (rowTop - worldBox(child).getMaxY() <= 24.f) headerRow.push_back(child);
            }
        }
    }

    if (!headerRow.empty()) {
        float rowBottom = 1e9f;
        for (auto child : headerRow) rowBottom = std::min(rowBottom, worldBox(child).getMidY());
        headerY = host->convertToNodeSpace({0.f, rowBottom}).y;
    }

    float nameRight = host->convertToNodeSpace({titleBox.getMaxX(), titleBox.getMidY()}).x;
    for (cocos2d::CCNode* extra : {static_cast<cocos2d::CCNode*>(popup->m_roleBadge),
                                   static_cast<cocos2d::CCNode*>(popup->m_titleEmote)}) {
        if (!extra) continue;
        auto box = worldBox(extra);
        nameRight = std::max(nameRight, host->convertToNodeSpace({box.getMaxX(), box.getMidY()}).x);
    }

    float limit = 1e9f;
    for (auto child : headerRow) {
        float left = host->convertToNodeSpace({worldBox(child).getMinX(), 0.f}).x;
        limit = std::min(limit, left - 4.f);
    }
    if (popup->m_membersButton) {
        auto box = worldBox(popup->m_membersButton);
        float left = host->convertToNodeSpace({box.getMinX(), box.getMidY()}).x;
        if (left > nameRight) limit = std::min(limit, left - 4.f);
    }
    if (limit > 1e8f) limit = host->getContentSize().width - 10.f;

    constexpr float SIZE = 18.f;
    float half = SIZE * 0.5f;
    float x = nameRight + 7.f + half;
    if (x + half > limit) {
        float squeezed = limit - half;
        if (squeezed - half < nameRight + 2.f) return; // no room: leave the header alone
        x = squeezed;
    }

    auto pair = fstreak::StreakStore::get().pairForFriend(thread->m_accountID);

    auto flame = fstreak::FlameNode::create(SIZE, true);
    if (!flame) return;
    if (pair) {
        flame->setStreak(pair->current);
    } else {
        flame->setStreak(0);
        flame->setDimmed(true);
    }

    auto item = CCMenuItemSpriteExtra::create(flame, target, handler);
    if (!item) return;

    auto menu = cocos2d::CCMenu::create();
    menu->setPosition({x, headerY});
    menu->addChild(item);
    menu->setTag(kFlameTag);
    host->addChild(menu, 20);
}
#endif // FSTREAK_HAS_THREADS

/// Keeps the visible streak decorations in step with the mirror.
void refreshStreakSurfaces() {
    // Rebuilding the rows re-runs loadFromThread, which redecorates every cell.
    if (g_refreshThreadList) g_refreshThreadList();
    if (g_refreshChat) g_refreshChat();
    fstreak::StreakInfoPopup::refreshOpen();
}

/// One second heartbeat that drives background syncing.
class StreakTicker : public cocos2d::CCNode {
public:
    static StreakTicker* get() {
        static StreakTicker* instance = nullptr;
        if (!instance) {
            instance = new StreakTicker();
            if (!instance->init()) {
                delete instance;
                return nullptr;
            }
            instance->retain();
        }
        return instance;
    }

    /// Re-attaches after a scene change; safe to call from any screen hook.
    void attach() {
        auto director = cocos2d::CCDirector::get();
        auto scene = director ? director->getRunningScene() : nullptr;
        if (!scene || this->getParent() == scene) return;
        this->removeFromParentAndCleanup(false);
        scene->addChild(this, 999);
    }

private:
    bool init() {
        if (!CCNode::init()) return false;
        this->setID("friendship-streak-ticker"_spr);
        this->schedule(schedule_selector(StreakTicker::tick), 1.f);
        return true;
    }

    void tick(float dt) {
        fstreak::StreakStore::get().onTick(dt);
    }
};

} // namespace

$on_mod(Loaded) {
    // One listener for every surface: the chat rows, the open chat and the card.
    fstreak::StreakStore::get().addListener([] {
        refreshStreakSurfaces();
    });
}

class $modify(FriendshipStreakMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        // The menu scene is not the running scene yet while init runs, so attach on
        // the next frame or the ticker would be dropped with the loading scene.
        this->scheduleOnce(schedule_selector(FriendshipStreakMenuLayer::attachTicker), 0.2f);
        fstreak::StreakStore::get().onMenuOpened();
        return true;
    }

    void attachTicker(float) {
        if (auto ticker = StreakTicker::get()) ticker->attach();
    }
};

#if FSTREAK_CAN_HOOK_USER_CELL
class $modify(FriendshipStreakUserCell, FSTREAK_USER_CELL) {
    void loadFromScore(GJUserScore* score) {
        FSTREAK_USER_CELL::loadFromScore(score);
        if (!score) return;

        removeDecoration(this, kFlameTag);
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto pair = fstreak::StreakStore::get().pairForFriend(score->m_accountID);
        if (!pair || pair->current <= 0) return;

        auto name = toStdString(score->m_userName);
        cocos2d::CCPoint position = {this->getContentSize().width - 56.f, this->getContentSize().height / 2.f};
        auto label = [&]() -> CCLabelBMFont* {
            std::vector<CCLabelBMFont*> labels;
            collectLabels(this, labels);
            for (auto candidate : labels) {
                if (textMatches(labelText(candidate), name) && !usesChatFont(candidate)) {
                    return candidate;
                }
            }
            return nullptr;
        }();
        if (label) {
            position.setPoint(
                label->getPositionX() + label->getContentSize().width * label->getScaleX() * 0.5f + 18.f,
                label->getPositionY()
            );
        }
        auto flame = fstreak::FlameNode::create(20.f, true);
        if (!flame) return;
        flame->setTag(kFlameTag);
        flame->setStreak(pair->current);
        flame->setPosition(position);
        this->addChild(flame, 20);
    }
};
#endif

class $modify(FriendshipStreakMessageCell, GJMessageCell) {
    void loadFromMessage(GJUserMessage* message) {
        GJMessageCell::loadFromMessage(message);
        if (!message) return;

        removeDecoration(this, kFlameTag);
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto pair = fstreak::StreakStore::get().pairForFriend(message->m_accountID);
        if (!pair || pair->current <= 0) return;

        auto flame = fstreak::FlameNode::create(20.f, true);
        if (!flame) return;
        flame->setTag(kFlameTag);
        flame->setStreak(pair->current);
        flame->setPosition({this->getContentSize().width - 46.f, this->getContentSize().height - 16.f});
        this->addChild(flame, 20);
    }
};

class $modify(FriendshipStreakProfilePage, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);
        if (!score) return;

        removeDecoration(this, kFlameTag);
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return;

        auto pair = fstreak::StreakStore::get().pairForFriend(score->m_accountID);
        if (!pair || pair->current <= 0) return;

        auto name = toStdString(score->m_userName);
        CCLabelBMFont* label = nullptr;
        std::vector<CCLabelBMFont*> labels;
        collectLabels(this, labels);
        for (auto candidate : labels) {
            if (textMatches(labelText(candidate), name) && !usesChatFont(candidate)) {
                label = candidate;
                break;
            }
        }
        if (!label || !label->getParent()) return;

        auto line = CCLabelBMFont::create(
            fmt::format("{} day friendship streak", pair->current).c_str(), "goldFont.fnt"
        );
        if (!line) return;
        line->setTag(kFlameTag);
        line->setScale(0.42f);
        line->setColor(fstreak::FlameNode::stageColor(pair->current));
        line->setAnchorPoint({0.f, 0.5f});
        line->setPosition({label->getPositionX(), label->getPositionY() - 20.f});
        label->getParent()->addChild(line, 20);
    }
};

#if FSTREAK_HAS_THREADS
class $modify(FriendshipStreakThreadCell, GJThreadCell) {
    void loadFromThread(GJMessageThread* thread) {
        GJThreadCell::loadFromThread(thread);
        decorateThreadCell(this);
    }
};

/// Chat list page: keeps the streak mirror fresh while it is open.
class $modify(FriendshipStreakThreadsPage, MessageThreadsPage) {
    bool init() {
        if (!MessageThreadsPage::init()) return false;

        g_refreshThreadList = [this] {
            if (m_threadListView) m_threadListView->reloadAll();
        };

        auto& store = fstreak::StreakStore::get();
        store.enterChat();
        store.requestRefresh("threads-open");
        return true;
    }

    void onExit() {
        g_refreshThreadList = nullptr;
        fstreak::StreakStore::get().leaveChat();
        MessageThreadsPage::onExit();
    }

    void threadListFinished(cocos2d::CCArray* threads, int page, int total, int unread) {
        MessageThreadsPage::threadListFinished(threads, page, total, unread);
        fstreak::StreakStore::get().requestRefresh("threads-loaded");
    }

    void threadUpdateFinished(int first, int last) {
        MessageThreadsPage::threadUpdateFinished(first, last);
        fstreak::StreakStore::get().requestRefresh("threads-update");
    }

    void onRefresh(cocos2d::CCObject* sender) {
        MessageThreadsPage::onRefresh(sender);
        fstreak::StreakStore::get().requestRefresh("threads-refresh");
    }
};

/// Open chat: chip in the header, faster polling, and a sync whenever messages
/// arrive or leave.
class $modify(FriendshipStreakThreadView, ThreadViewPopup) {
    bool init(GJMessageThread* thread) {
        if (!ThreadViewPopup::init(thread)) return false;

        g_refreshChat = [this] {
            placeChatChip(this, this, menu_selector(FriendshipStreakThreadView::onChip));
        };

        auto& store = fstreak::StreakStore::get();
        store.enterChat();

        this->placeChip();
        return true;
    }

    void onExit() {
        g_refreshChat = nullptr;
        fstreak::StreakStore::get().leaveChat();
        ThreadViewPopup::onExit();
    }

    void layoutTitleRow() {
        ThreadViewPopup::layoutTitleRow();
        this->placeChip();
    }

    void threadMessagesFinished(int accountID, cocos2d::CCArray* messages, bool hasMore) {
        ThreadViewPopup::threadMessagesFinished(accountID, messages, hasMore);

        this->reportOwnMessage(messages);
        fstreak::StreakStore::get().requestRefresh("messages");
    }

    void placeChip() {
        placeChatChip(this, this, menu_selector(FriendshipStreakThreadView::onChip));
    }

    /// Reports the first message we wrote today, which is what keeps the flame alive.
    void reportOwnMessage(cocos2d::CCArray* messages) {
        auto thread = m_thread;
        if (!thread || !thread->m_isDM || !messages) return;

        auto& store = fstreak::StreakStore::get();

        int self = fstreak::StreakStore::signedInAccountId();
        if (self <= 0) return;

        auto today = fstreak::utcDay(static_cast<int64_t>(std::time(nullptr)));
        for (auto object : CCArrayExt<cocos2d::CCObject*>(messages)) {
            auto message = static_cast<GJThreadMessage*>(object);
            if (!message || message->m_deleted) continue;
            if (message->m_accountID != self) continue;
            if (fstreak::utcDay(message->m_timestamp) != today) continue;
            store.reportMessageSent(thread->m_accountID);
            return;
        }
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

        auto& store = fstreak::StreakStore::get();
        store.enterChat();
        store.requestRefresh("message-open");

        removeDecoration(this, kFlameTag);
        if (!Mod::get()->getSettingValue<bool>("show-flame-in-friends")) return true;

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

    void onExit() {
        fstreak::StreakStore::get().leaveChat();
        GJMessagePopup::onExit();
    }

    void downloadMessageFinished(GJUserMessage* message) {
        GJMessagePopup::downloadMessageFinished(message);
        fstreak::StreakStore::get().requestRefresh("message-downloaded");
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
        auto& store = fstreak::StreakStore::get();
        store.reportMessageSent(accountID);
        store.requestRefresh("sent");
    }
};
#endif
