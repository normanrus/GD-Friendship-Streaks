#include "FlameNode.hpp"

using namespace geode::prelude;

namespace fstreak {

namespace {
// The stage textures are pre-tinted copies of the same drawing: multiplying the
// orange artwork with a blue tint only makes it muddy.
cocos2d::CCSprite* makeFlameSprite(FlameStage stage) {
    switch (stage) {
        case FlameStage::Blue: return CCSprite::create("flame-blue.png"_spr);
        case FlameStage::Purple: return CCSprite::create("flame-purple.png"_spr);
        case FlameStage::Gold: return CCSprite::create("flame-gold.png"_spr);
        default: return CCSprite::create("flame.png"_spr);
    }
}
} // namespace

FlameNode* FlameNode::create(float size, bool showCount) {
    auto node = new FlameNode();
    if (node && node->init(size, showCount)) {
        node->autorelease();
        return node;
    }
    CC_SAFE_DELETE(node);
    return nullptr;
}

bool FlameNode::init(float size, bool showCount) {
    if (!CCNode::init()) return false;
    m_size = size;
    m_showCount = showCount;

    m_flame = makeFlameSprite(FlameStage::Orange);
    if (!m_flame) return false;
    m_flame->setAnchorPoint({0.5f, 0.5f});
    m_flame->setScale(m_size / m_flame->getContentSize().height);
    this->addChild(m_flame, 0);

    m_count = CCLabelBMFont::create("0", "bigFont.fnt");
    if (!m_count) return false;
    m_count->setAnchorPoint({0.f, 0.5f});
    m_count->setScale(m_size / 34.f);
    m_count->setPosition({m_size * 0.5f + 2.f, 0.f});
    this->addChild(m_count, 1);

    this->setContentSize({m_size, m_size});
    m_stage = FlameStage::Dead;
    this->applyStage(0);
    return true;
}

cocos2d::ccColor3B FlameNode::stageColor(int streak) {
    switch (flameStage(streak)) {
        case FlameStage::Blue: return {92, 168, 255};
        case FlameStage::Purple: return {186, 132, 255};
        case FlameStage::Gold: return {255, 209, 102};
        case FlameStage::Orange: return {255, 152, 66};
        default: return {150, 150, 150};
    }
}

char const* FlameNode::stageTexture(FlameStage stage) {
    switch (stage) {
        case FlameStage::Blue: return "flame-blue.png";
        case FlameStage::Purple: return "flame-purple.png";
        case FlameStage::Gold: return "flame-gold.png";
        case FlameStage::Orange: return "flame.png";
        default: return "flame.png";
    }
}

void FlameNode::replaceFlameSprite(char const* /*texture*/, bool animate) {
    auto fresh = makeFlameSprite(m_stage);
    if (!fresh) return;

    float scale = m_size / fresh->getContentSize().height;
    fresh->setAnchorPoint({0.5f, 0.5f});
    fresh->setScale(scale);
    fresh->setColor(
        m_stage == FlameStage::Dead ? cocos2d::ccColor3B{130, 130, 130} : cocos2d::ccColor3B{255, 255, 255}
    );
    fresh->setOpacity(m_dimmed ? 120 : 255);

    if (m_flame) m_flame->removeFromParent();
    m_flame = fresh;
    this->addChild(m_flame, 0);

    if (!animate) return;

    // A short flare when the flame enters a new colour stage.
    m_flame->setOpacity(0);
    m_flame->setScale(scale * 1.4f);
    m_flame->runAction(cocos2d::CCEaseOut::create(cocos2d::CCScaleTo::create(0.22f, scale), 2.f));
    m_flame->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(0.05f),
        cocos2d::CCFadeTo::create(0.18f, m_dimmed ? 120 : 255), nullptr
    ));
}

void FlameNode::applyStage(int streak) {
    auto stage = flameStage(streak);
    bool changed = stage != m_stage;
    m_stage = stage;

    if (!m_flame || changed) this->replaceFlameSprite(nullptr, changed);

    if (m_count) {
        m_count->setColor(
            stage == FlameStage::Dead ? cocos2d::ccColor3B{150, 150, 150} : stageColor(streak)
        );
    }
}

void FlameNode::setStreak(int streak) {
    bool showCount = m_showCount && Mod::get()->getSettingValue<bool>("show-counter");
    if (m_count) {
        m_count->setString(fmt::format("{}", streak).c_str());
        m_count->setVisible(showCount && streak > 0);
        m_count->setOpacity(m_dimmed ? 120 : 255);
    }
    this->applyStage(streak);
    this->setContentSize({this->contentWidth(), m_size});
}

void FlameNode::setShowCount(bool show) {
    m_showCount = show;
    if (m_count) m_count->setVisible(m_showCount);
    this->setContentSize({this->contentWidth(), m_size});
}

void FlameNode::setDimmed(bool dimmed) {
    m_dimmed = dimmed;
    if (m_flame) m_flame->setOpacity(dimmed ? 120 : 255);
    if (m_count) m_count->setOpacity(dimmed ? 120 : 255);
}

float FlameNode::contentWidth() const {
    if (!m_count || !m_count->isVisible()) return m_size;
    return m_size * 0.5f + m_count->getContentSize().width * m_count->getScale() + 4.f;
}

void FlameNode::celebrate() {
    if (!m_flame) return;
    float base = m_size / m_flame->getContentSize().height;
    m_flame->stopAllActions();
    m_flame->setScale(base);
    m_flame->runAction(cocos2d::CCSequence::create(
        cocos2d::CCEaseOut::create(cocos2d::CCScaleTo::create(0.18f, base * 1.45f), 2.f),
        cocos2d::CCEaseInOut::create(cocos2d::CCScaleTo::create(0.22f, base), 2.f),
        nullptr
    ));
}

} // namespace fstreak
