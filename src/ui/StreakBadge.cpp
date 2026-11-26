#include "StreakBadge.hpp"

#include "StreakInfoPopup.hpp"
#include "Ui.hpp"

#include "../Notify.hpp"
#include "../StreakStore.hpp"


#include <algorithm>

using namespace geode::prelude;

namespace fstreak {
namespace {

constexpr float BADGE_HEIGHT = 30.f;
constexpr float FLAME_SIZE = 22.f;
constexpr float GAP = 4.f;
constexpr float SIDE_PAD = 8.f;

int bestActiveStreak() {
    int best = 0;
    for (auto const& pair : StreakStore::get().pairs()) {
        best = std::max(best, pair.current);
    }
    return best;
}

} // namespace

StreakBadge* StreakBadge::create() {
    auto ret = new StreakBadge();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StreakBadge::init() {
    if (!CCNode::init()) return false;

    m_glow = CCSprite::create("glow.png"_spr);
    if (m_glow) {
        m_glow->setScale(0.18f);
        m_glow->setOpacity(0);
        m_glow->setColor({255, 170, 80});
        m_glow->setPosition({-18.f, 0.f});
        this->addChild(m_glow, 1);
    }

    m_flame = CCSprite::create("flame.png"_spr);
    if (m_flame) {
        m_flame->setScale(FLAME_SIZE / m_flame->getContentSize().height);
        m_flame->setPosition({-18.f, 0.f});
        this->addChild(m_flame, 2);
    }

    m_count = ui::label("0", "bigFont.fnt", 0.62f, ui::COLOR_GOLD, {0.f, 0.5f});
    if (m_count) {
        m_count->setPosition({-18.f + FLAME_SIZE * 0.5f + GAP, 0.5f});
        this->addChild(m_count, 3);
    }

    this->setContentSize({76.f, BADGE_HEIGHT});
    this->updateFromStore();

    auto item = CCMenuItemSpriteExtra::create(
        ui::rect(76.f, BADGE_HEIGHT, {255, 255, 255, 0}), this, menu_selector(StreakBadge::onOpen)
    );
    if (item) {
        item->setPosition({0.f, 0.f});
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(item);
        this->addChild(menu, 4);
    }

    this->startIdle();
    return true;
}

void StreakBadge::startIdle() {
    if (!m_glow) return;
    m_glow->stopAllActions();
    m_glow->runAction(CCRepeatForever::create(CCSequence::create(
        CCFadeTo::create(1.1f, m_streak > 0 ? 90 : 35), CCFadeTo::create(1.3f, 0), nullptr
    )));
}

void StreakBadge::updateFromStore() {
    m_streak = bestActiveStreak();
    if (m_count) {
        m_count->setString(fmt::format("{}", m_streak).c_str());
        m_count->setVisible(Mod::get()->getSettingValue<bool>("show-counter"));
        m_count->setColor(m_streak > 0 ? ui::COLOR_GOLD : ui::COLOR_MUTED);
    }
    if (m_flame) {
        m_flame->setColor(m_streak > 0 ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{130, 130, 130});
    }
}

void StreakBadge::celebrate() {
    if (!m_flame) return;
    float base = FLAME_SIZE / m_flame->getContentSize().height;
    m_flame->stopAllActions();
    m_flame->setScale(base);
    m_flame->runAction(CCSequence::create(
        CCEaseOut::create(CCScaleTo::create(0.16f, base * 1.5f), 2.f),
        CCEaseInOut::create(CCScaleTo::create(0.2f, base), 2.f), nullptr
    ));
    this->startIdle();
}

void StreakBadge::onOpen(CCObject*) {
    if (!StreakInfoPopup::openBest()) {
        notify::toast("Open a chat with a friend to start a streak", 2.f);
    }
}

} // namespace fstreak
