#include "Notify.hpp"

#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace fstreak::notify {

namespace {
bool enabled() {
    return Mod::get()->getSettingValue<bool>("notifications");
}

void pop(std::string const& text, NotificationIcon icon, float seconds) {
    if (auto notification = Notification::create(text, icon, seconds)) {
        notification->show();
    }
}
} // namespace

void toast(std::string const& text, float seconds) {
    // Direct feedback for a user action, so it ignores the streak notification setting.
    pop(text, NotificationIcon::Info, seconds);
}

void streakIncreased(PairState const& pair) {
    if (!enabled()) return;

    if (isMilestone(pair.current)) {
        pop(
            fmt::format("{} day streak with {}!", pair.current, pair.friendName),
            NotificationIcon::Success, 4.f
        );
        pop(milestoneMessage(pair.current), NotificationIcon::Success, 4.f);
        return;
    }

    pop(
        fmt::format("{} day streak with {} keeps growing", pair.current, pair.friendName),
        NotificationIcon::Info, 3.f
    );
}

void streakBroken(PairState const& pair, int lostStreak) {
    if (!enabled()) return;
    pop(
        fmt::format("{} day streak with {} ended", lostStreak, pair.friendName),
        NotificationIcon::Warning, 4.f
    );
}

void reminder(PairState const& pair) {
    if (!enabled()) return;
    pop(fmt::format("{} is waiting for your reply", pair.friendName), NotificationIcon::Info, 3.5f);
}

void milestoneEffect(cocos2d::CCNode* parent, cocos2d::CCPoint at) {
    if (!parent || !Mod::get()->getSettingValue<bool>("milestone-effects")) return;

    auto glow = cocos2d::CCSprite::create("flame.png"_spr);
    if (!glow) return;
    glow->setPosition(at);
    glow->setScale(0.4f);
    glow->setOpacity(170);
    glow->setColor({255, 200, 90});
    parent->addChild(glow, 20);

    glow->runAction(cocos2d::CCSequence::create(
        cocos2d::CCEaseOut::create(cocos2d::CCScaleTo::create(0.45f, 1.6f), 2.f),
        cocos2d::CCFadeOut::create(0.25f), cocos2d::CCRemoveSelf::create(), nullptr
    ));
}

} // namespace fstreak::notify
