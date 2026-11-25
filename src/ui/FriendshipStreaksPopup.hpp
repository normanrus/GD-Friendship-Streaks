#pragma once
// The window opened from the flame in the main menu: one row per paired friend.

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include "../StreakModel.hpp"

namespace fstreak {

class FriendshipStreaksPopup : public geode::Popup {
public:
    static FriendshipStreaksPopup* create();
    static void open();

private:
    bool init() override;
    void onClose(cocos2d::CCObject* sender) override;

    void buildChrome();
    void rebuildRows();
    void buildRow(PairState const& pair, float y);
    void buildEmptyState();
    void buildSetupState();
    void updateFooter();

    void onRefresh(cocos2d::CCObject* sender);
    void onPair(cocos2d::CCObject* sender);
    void onRowPressed(cocos2d::CCObject* sender);

    cocos2d::CCNode* m_emptyNode = nullptr;
    cocos2d::CCLabelBMFont* m_summaryLabel = nullptr;
    cocos2d::CCLabelBMFont* m_footerLabel = nullptr;
    geode::ScrollLayer* m_scroll = nullptr;
    int m_listenerId = 0;
};

} // namespace fstreak
