#pragma once
// Small "flame + day count" widget reused in the friends list, the message
// list, the profile page and the streak popups.

#include <Geode/Geode.hpp>

namespace fstreak {

class FlameNode : public cocos2d::CCNode {
public:
    static FlameNode* create(float size = 22.f, bool showCount = true);

    void setStreak(int streak);
    void setShowCount(bool show);
    void celebrate();
    void setDimmed(bool dimmed);

    /// Width of the whole widget after the last setStreak, for laying out neighbours.
    float contentWidth() const;

private:
    bool init(float size, bool showCount);

    cocos2d::CCSprite* m_flame = nullptr;
    cocos2d::CCLabelBMFont* m_count = nullptr;
    float m_size = 22.f;
    bool m_showCount = true;
    bool m_dimmed = false;
};

} // namespace fstreak
