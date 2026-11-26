#pragma once
// Small "flame + day count" widget reused in the chat list, the open chat, the
// message list, the profile page and the streak card.
//
// The flame colour follows the streak length, so both players see the same stage
// without the server having to send anything: it is derived from the day count.

#include <Geode/Geode.hpp>

#include "core/FriendStreakCore.hpp"

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

    /// Colour of the stage a streak belongs to, shared by every surface that shows it.
    static cocos2d::ccColor3B stageColor(int streak);

private:
    bool init(float size, bool showCount);

    static char const* stageTexture(FlameStage stage);
    void replaceFlameSprite(char const* texture, bool animate);
    void applyStage(int streak);

    cocos2d::CCSprite* m_flame = nullptr;
    cocos2d::CCLabelBMFont* m_count = nullptr;
    float m_size = 22.f;
    bool m_showCount = true;
    bool m_dimmed = false;
    FlameStage m_stage = FlameStage::Dead;
};

} // namespace fstreak
