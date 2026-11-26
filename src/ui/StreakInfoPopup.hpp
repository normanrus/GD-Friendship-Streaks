#pragma once
// Compact streak card shown when a flame is tapped in a chat or the main menu.
// Also the place where a streak is started: it holds the code exchange, so the
// mod needs no separate screen of its own.

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <string>

#include "FlameNode.hpp"

namespace fstreak {

class StreakInfoPopup : public geode::Popup {
public:
    static void open(int friendAccountId, std::string friendName);
    /// Opens the card of the friend with the longest running streak.
    static bool openBest();

protected:
    bool init(int friendAccountId, std::string friendName);

private:
    void build();
    void buildPaired();
    void buildUnpaired();

    void onGetCode(cocos2d::CCObject* sender);
    void onCopy(cocos2d::CCObject* sender);
    void onLink(cocos2d::CCObject* sender);
    void onMarkSent(cocos2d::CCObject* sender);

    int m_friendId = 0;
    std::string m_friendName;
    std::string m_pairId;
    cocos2d::CCNode* m_body = nullptr;
    cocos2d::CCLabelBMFont* m_codeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    geode::TextInput* m_input = nullptr;
};

} // namespace fstreak
