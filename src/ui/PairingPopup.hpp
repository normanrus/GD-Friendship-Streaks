#pragma once
// Pairing flow: show your own invite code, or enter the code a friend sent you.

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <string>

namespace fstreak {

class PairingPopup : public geode::Popup {
public:
    static PairingPopup* create();
    static void open();

private:
    bool init() override;

    void buildInviteSection();
    void buildRedeemSection();
    void refreshInviteCode();

    void onGenerate(cocos2d::CCObject* sender);
    void onCopy(cocos2d::CCObject* sender);
    void onLink(cocos2d::CCObject* sender);

    cocos2d::CCLabelBMFont* m_codeLabel = nullptr;
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCNode* m_inviteNode = nullptr;
    geode::TextInput* m_input = nullptr;
};

} // namespace fstreak
