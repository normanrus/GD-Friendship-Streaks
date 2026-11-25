#pragma once
// One friend: current and best streak, start date, activity calendar, and the
// deadline for today's exchange.

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include "../StreakModel.hpp"

namespace fstreak {

class FriendStreakDetailPopup : public geode::Popup, public FLAlertLayerProtocol {
public:
    static FriendStreakDetailPopup* create(PairState const& pair);
    static void open(PairState const& pair);

private:
    bool init(PairState const& pair);
    void onClose(cocos2d::CCObject* sender) override;

    void buildHeader();
    void buildCalendar();
    void buildFooter();

    void onMessage(cocos2d::CCObject* sender);
    void onUnlink(cocos2d::CCObject* sender);
    void onUnlinkConfirm(bool confirmed);
    void onTick(float dt);

public:
    void FLAlert_Clicked(FLAlertLayer* layer, bool btn2) override;

private:

    PairState m_pair;
    cocos2d::CCNode* m_calendarNode = nullptr;
    cocos2d::CCLabelBMFont* m_deadlineLabel = nullptr;
    int m_listenerId = 0;
};

} // namespace fstreak
