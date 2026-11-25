#include "FlameNode.hpp"

using namespace geode::prelude;

namespace fstreak {

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

    m_flame = CCSprite::create("flame.png"_spr);
    if (!m_flame) return false;
    float scale = m_size / m_flame->getContentSize().height;
    m_flame->setScale(scale);
    m_flame->setAnchorPoint({0.5f, 0.5f});
    this->addChild(m_flame);

    m_count = CCLabelBMFont::create("0", "bigFont.fnt");
    m_count->setAnchorPoint({0.f, 0.5f});
    m_count->setScale(m_size / 34.f);
    m_count->setPosition({m_size * 0.5f, 0.f});
    this->addChild(m_count);

    this->setContentSize({m_size, m_size});
    this->setStreak(0);
    return true;
}

void FlameNode::setStreak(int streak) {
    if (m_count) {
        m_count->setString(fmt::format("{}", streak).c_str());
        m_count->setVisible(m_showCount && streak > 0);
    }
    if (m_flame) {
        // A quiet flame for a dead streak, a warm one while it is alive.
        m_flame->setColor(streak > 0 ? cocos2d::ccColor3B{255, 255, 255} : cocos2d::ccColor3B{120, 120, 120});
    }
    this->setContentSize({this->contentWidth(), m_size});
}

void FlameNode::setShowCount(bool show) {
    m_showCount = show;
    if (m_count) m_count->setVisible(show);
    this->setContentSize({this->contentWidth(), m_size});
}

void FlameNode::setDimmed(bool dimmed) {
    m_dimmed = dimmed;
    if (m_flame) m_flame->setOpacity(dimmed ? 120 : 255);
    if (m_count) m_count->setOpacity(dimmed ? 120 : 255);
}

float FlameNode::contentWidth() const {
    if (!m_showCount || !m_count || m_count->getString() == "0") return m_size;
    return m_size * 0.5f + m_count->getContentSize().width * m_count->getScale() + 2.f;
}

void FlameNode::celebrate() {
    if (!m_flame) return;
    m_flame->stopAllActions();
    m_flame->setScale(m_size / m_flame->getContentSize().height);
    m_flame->runAction(CCSequence::create(
        CCEaseOut::create(CCScaleTo::create(0.18f, m_size / m_flame->getContentSize().height * 1.45f), 2.f),
        CCEaseInOut::create(CCScaleTo::create(0.22f, m_size / m_flame->getContentSize().height), 2.f),
        nullptr
    ));
}

} // namespace fstreak
