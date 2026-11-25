#pragma once
// The little flame in the main menu: shows the best active streak and opens
// the friendship streak window when tapped.

#include <Geode/Geode.hpp>

namespace fstreak {

class StreakBadge : public cocos2d::CCNode {
public:
    static StreakBadge* create();

    void updateFromStore();
    void celebrate();

private:
    bool init();
    void startIdle();
    void onOpen(cocos2d::CCObject* sender);

    cocos2d::CCNode* m_pill = nullptr;
    cocos2d::CCSprite* m_flame = nullptr;
    cocos2d::CCSprite* m_glow = nullptr;
    cocos2d::CCLabelBMFont* m_count = nullptr;
    int m_streak = 0;
};

} // namespace fstreak
