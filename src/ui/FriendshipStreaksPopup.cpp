#include "FriendshipStreaksPopup.hpp"

#include "FlameNode.hpp"
#include "FriendStreakDetailPopup.hpp"
#include "PairingPopup.hpp"
#include "Ui.hpp"

#include "../Notify.hpp"
#include "../StreakStore.hpp"

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

namespace fstreak {
namespace {

constexpr float POPUP_WIDTH = 440.f;
constexpr float POPUP_HEIGHT = 300.f;

constexpr float LIST_X = 22.f;
constexpr float LIST_Y = 72.f;
constexpr float LIST_WIDTH = 396.f;
constexpr float LIST_HEIGHT = 156.f;

constexpr float ROW_WIDTH = 392.f;
constexpr float ROW_HEIGHT = 50.f;
constexpr float ROW_GAP = 6.f;

char const* const kRowID = "friend-row";

std::string summaryText(std::vector<PairState> const& pairs) {
    if (pairs.empty()) return "No friendship streaks yet";
    int best = 0;
    int alive = 0;
    for (auto const& pair : pairs) {
        best = std::max(best, pair.best);
        if (pair.current > 0) ++alive;
    }
    return fmt::format(
        "{} {} · {} alive · best {}", pairs.size(), pairs.size() == 1 ? "friend" : "friends", alive, best
    );
}

} // namespace

FriendshipStreaksPopup* FriendshipStreaksPopup::create() {
    auto ret = new FriendshipStreaksPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void FriendshipStreaksPopup::open() {
    if (auto scene = CCDirector::get()->getRunningScene()) {
        if (scene->getChildByID("friendship-streaks-popup"_spr)) return;
    }
    if (auto popup = FriendshipStreaksPopup::create()) {
        popup->show();
    }
}

bool FriendshipStreaksPopup::init() {
    if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT)) return false;

    this->setID("friendship-streaks-popup"_spr);
    this->setTitle("Friendship Streaks");

    this->buildChrome();

    m_listenerId = StreakStore::get().addListener([this] {
        this->rebuildRows();
    });

    StreakStore::get().onMenuOpened();
    this->rebuildRows();
    return true;
}

void FriendshipStreaksPopup::onClose(CCObject* sender) {
    StreakStore::get().removeListener(m_listenerId);
    m_listenerId = 0;
    Popup::onClose(sender);
}

void FriendshipStreaksPopup::buildChrome() {
    m_summaryLabel = ui::label(summaryText(StreakStore::get().pairs()), "goldFont.fnt", 0.5f, ui::COLOR_MUTED);
    if (m_summaryLabel) {
        m_summaryLabel->setPosition({POPUP_WIDTH / 2.f, POPUP_HEIGHT - 58.f});
        m_mainLayer->addChild(m_summaryLabel, 2);
    }

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(menu, 3);

    if (auto pair = ui::button("Pair a friend", 1.f, this, menu_selector(FriendshipStreaksPopup::onPair))) {
        pair->setPosition({88.f, 46.f});
        menu->addChild(pair);
    }
    if (auto refresh = ui::button("Refresh", 1.f, this, menu_selector(FriendshipStreaksPopup::onRefresh))) {
        refresh->setPosition({POPUP_WIDTH - 78.f, 46.f});
        menu->addChild(refresh);
    }

    m_footerLabel = ui::label("", "chatFont.fnt", 0.62f, ui::COLOR_MUTED);
    if (m_footerLabel) {
        m_footerLabel->setPosition({POPUP_WIDTH / 2.f, 10.f});
        m_mainLayer->addChild(m_footerLabel, 2);
    }
    this->updateFooter();
}

void FriendshipStreaksPopup::updateFooter() {
    if (!m_footerLabel) return;
    auto& store = StreakStore::get();
    if (!store.configured()) {
        m_footerLabel->setString("Sync server not set up");
    } else if (!store.hasAccount()) {
        m_footerLabel->setString("Log in to a GD account to sync streaks");
    } else {
        m_footerLabel->setString(fmt::format("Synced as {}", store.account().username).c_str());
    }
}

void FriendshipStreaksPopup::rebuildRows() {
    if (m_emptyNode) {
        m_emptyNode->removeFromParent();
        m_emptyNode = nullptr;
    }
    if (m_scroll) {
        m_scroll->removeFromParent();
        m_scroll = nullptr;
    }

    auto& store = StreakStore::get();
    if (m_summaryLabel) m_summaryLabel->setString(summaryText(store.pairs()).c_str());
    this->updateFooter();

    if (!store.configured()) {
        this->buildSetupState();
        return;
    }
    if (store.pairs().empty()) {
        this->buildEmptyState();
        return;
    }

    auto scroll = ScrollLayer::create({LIST_WIDTH, LIST_HEIGHT});
    scroll->setPosition({LIST_X, LIST_Y});
    scroll->m_contentLayer->removeAllChildren();
    m_mainLayer->addChild(scroll, 1);
    m_scroll = scroll;

    auto const& pairs = store.pairs();
    float total = pairs.size() * (ROW_HEIGHT + ROW_GAP);
    float height = std::max(total, LIST_HEIGHT);
    scroll->m_contentLayer->setContentSize({LIST_WIDTH, height});

    float y = height - ROW_HEIGHT / 2.f - 2.f;
    for (auto const& pair : pairs) {
        this->buildRow(pair, y);
        y -= ROW_HEIGHT + ROW_GAP;
    }
    scroll->moveToTop();
}

void FriendshipStreaksPopup::buildRow(PairState const& pair, float y) {
    auto row = CCNode::create();
    row->setPosition({(LIST_WIDTH - ROW_WIDTH) / 2.f + ROW_WIDTH / 2.f, y});
    row->setContentSize({ROW_WIDTH, ROW_HEIGHT});
    m_scroll->m_contentLayer->addChild(row);

    if (auto bg = ui::panel(ROW_WIDTH, ROW_HEIGHT, {255, 255, 255, 26})) {
        bg->setPosition({0.f, 0.f});
        row->addChild(bg, 0);
    }
    if (pair.current > 0) {
        if (auto accent = ui::rect(3.f, ROW_HEIGHT - 14.f, {255, 138, 51, 200})) {
            accent->setPosition({-ROW_WIDTH / 2.f + 6.f, 0.f});
            row->addChild(accent, 1);
        }
    }

    auto flame = FlameNode::create(26.f, false);
    if (flame) {
        flame->setPosition({-ROW_WIDTH / 2.f + 26.f, 0.f});
        flame->setStreak(pair.current);
        flame->setDimmed(pair.current == 0);
        row->addChild(flame, 2);
    }

    auto name = ui::label(pair.friendName, "bigFont.fnt", 0.6f, ui::COLOR_TEXT, {0.f, 0.5f});
    if (name) {
        name->setPosition({-ROW_WIDTH / 2.f + 46.f, 9.f});
        name->limitLabelWidth(190.f, 0.6f, 0.25f);
        row->addChild(name, 2);
    }

    auto status = ui::label(
        todayStatusText(pair.status()), "chatFont.fnt", 0.6f,
        pair.status() == TodayStatus::Done ? ui::COLOR_ALIVE : ui::COLOR_MUTED, {0.f, 0.5f}
    );
    if (status) {
        status->setPosition({-ROW_WIDTH / 2.f + 46.f, -11.f});
        status->limitLabelWidth(210.f, 0.6f, 0.25f);
        row->addChild(status, 2);
    }

    auto count = ui::label(
        fmt::format("{}", pair.current), "bigFont.fnt", 0.85f,
        pair.current > 0 ? ui::COLOR_GOLD : ui::COLOR_MUTED, {1.f, 0.5f}
    );
    if (count) {
        count->setPosition({ROW_WIDTH / 2.f - 74.f, 6.f});
        row->addChild(count, 2);
    }

    auto best = ui::label(
        fmt::format("best {}", pair.best), "chatFont.fnt", 0.55f, ui::COLOR_MUTED, {1.f, 0.5f}
    );
    if (best) {
        best->setPosition({ROW_WIDTH / 2.f - 16.f, -12.f});
        row->addChild(best, 2);
    }

    auto hit = CCMenuItemSpriteExtra::create(
        ui::rect(ROW_WIDTH, ROW_HEIGHT, {255, 255, 255, 0}), this, menu_selector(FriendshipStreaksPopup::onRowPressed)
    );
    hit->setPosition({0.f, 0.f});
    hit->setUserObject(CCString::create(pair.id));
    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->addChild(hit);
    row->addChild(menu, 3);
}

void FriendshipStreaksPopup::buildEmptyState() {
    auto node = CCNode::create();
    node->setPosition({POPUP_WIDTH / 2.f, LIST_Y + LIST_HEIGHT / 2.f});
    m_mainLayer->addChild(node, 1);
    m_emptyNode = node;

    auto flame = FlameNode::create(56.f, false);
    if (flame) {
        flame->setPosition({0.f, 26.f});
        flame->setDimmed(true);
        node->addChild(flame);
    }

    auto title = ui::label("No friendship streaks yet", "goldFont.fnt", 0.62f, ui::COLOR_TEXT);
    if (title) {
        title->setPosition({0.f, -18.f});
        node->addChild(title);
    }

    auto hint = ui::label(
        "Pair with a friend to start a shared flame", "chatFont.fnt", 0.65f, ui::COLOR_MUTED
    );
    if (hint) {
        hint->setPosition({0.f, -38.f});
        node->addChild(hint);
    }
    ui::fadeIn(node, 0.05f);
}

void FriendshipStreaksPopup::buildSetupState() {
    auto node = CCNode::create();
    node->setPosition({POPUP_WIDTH / 2.f, LIST_Y + LIST_HEIGHT / 2.f + 10.f});
    m_mainLayer->addChild(node, 1);
    m_emptyNode = node;

    auto title = ui::label("Sync server is not set up", "goldFont.fnt", 0.6f, ui::COLOR_TEXT);
    if (title) {
        title->setPosition({0.f, 34.f});
        node->addChild(title);
    }

    auto hint = ui::label(
        "Paste your server URL in the mod settings to\nshare streaks with friends.",
        "chatFont.fnt", 0.62f, ui::COLOR_MUTED
    );
    if (hint) {
        hint->setPosition({0.f, 4.f});
        node->addChild(hint);
    }

    auto menu = CCMenu::create();
    menu->setPosition({0.f, -40.f});
    node->addChild(menu);
    if (auto settings = ui::button("Open settings", 1.f, this, menu_selector(FriendshipStreaksPopup::onPair))) {
        settings->setTag(1);
        menu->addChild(settings);
    }
}

void FriendshipStreaksPopup::onPair(CCObject* sender) {
    if (sender && sender->getTag() == 1) {
        geode::openSettingsPopup(Mod::get());
        return;
    }
    PairingPopup::open();
}

void FriendshipStreaksPopup::onRefresh(CCObject* sender) {
    auto& store = StreakStore::get();
    if (!store.configured()) {
        notify::toast("Set the server URL in mod settings first", 3.f);
        return;
    }
    notify::toast("Syncing streaks...", 1.2f);
    geode::Ref<FriendshipStreaksPopup> self = this;
    store.refresh([self](bool ok) {
        if (self->m_footerLabel) {
            self->m_footerLabel->setString(ok ? "Synced just now" : "Sync failed, will retry");
        }
    });
}

void FriendshipStreaksPopup::onRowPressed(CCObject* sender) {
    auto item = typeinfo_cast<CCMenuItemSpriteExtra*>(sender);
    if (!item) return;
    auto id = static_cast<CCString*>(item->getUserObject());
    if (!id) return;

    for (auto const& pair : StreakStore::get().pairs()) {
        if (pair.id == id->getCString()) {
            FriendStreakDetailPopup::open(pair);
            return;
        }
    }
}

} // namespace fstreak
