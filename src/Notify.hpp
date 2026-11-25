#pragma once
// Toast notifications for streak changes. All of them respect the mod setting.

#include "StreakModel.hpp"

namespace fstreak::notify {

void toast(std::string const& text, float seconds = 2.5f);

void streakIncreased(PairState const& pair);
void streakBroken(PairState const& pair, int lostStreak);
void reminder(PairState const& pair);

/// Small confetti-ish burst on milestone days; no-op when effects are disabled.
void milestoneEffect(cocos2d::CCNode* parent, cocos2d::CCPoint at);

} // namespace fstreak::notify
