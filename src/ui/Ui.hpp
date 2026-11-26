#pragma once
// Small drawing helpers shared by the flame widgets and the streak card.

#include <Geode/Geode.hpp>

namespace fstreak::ui {

constexpr cocos2d::ccColor3B COLOR_TEXT = {238, 238, 238};
constexpr cocos2d::ccColor3B COLOR_MUTED = {158, 158, 158};
constexpr cocos2d::ccColor3B COLOR_GOLD = {255, 209, 102};
constexpr cocos2d::ccColor3B COLOR_EMBER = {255, 138, 51};
constexpr cocos2d::ccColor3B COLOR_DEAD = {196, 96, 88};
constexpr cocos2d::ccColor3B COLOR_ALIVE = {124, 214, 138};

inline cocos2d::CCSprite* rect(float width, float height, cocos2d::ccColor4B color) {
    auto sprite = cocos2d::CCSprite::create("white.png"_spr);
    if (!sprite) return nullptr;
    auto size = sprite->getContentSize();
    sprite->setScaleX(width / std::max(1.f, size.width));
    sprite->setScaleY(height / std::max(1.f, size.height));
    sprite->setColor({color.r, color.g, color.b});
    sprite->setOpacity(color.a);
    return sprite;
}

inline cocos2d::CCLabelBMFont* label(
    std::string const& text, char const* font, float scale, cocos2d::ccColor3B color,
    cocos2d::CCPoint anchor = {0.5f, 0.5f}
) {
    auto label = cocos2d::CCLabelBMFont::create(text.c_str(), font);
    if (!label) return nullptr;
    label->setScale(scale);
    label->setColor(color);
    label->setAnchorPoint(anchor);
    return label;
}

inline void fadeIn(cocos2d::CCNode* node, float delay, float duration = 0.22f) {
    auto rgba = geode::cast::typeinfo_cast<cocos2d::CCNodeRGBA*>(node);
    if (!rgba) return;
    rgba->setOpacity(0);
    rgba->runAction(cocos2d::CCSequence::create(
        cocos2d::CCDelayTime::create(delay), cocos2d::CCFadeTo::create(duration, 255), nullptr
    ));
}

inline CCMenuItemSpriteExtra* button(
    std::string const& text, float scale, cocos2d::CCObject* target, cocos2d::SEL_MenuHandler handler
) {
    // GD's own ButtonSprite draws with the game's sheet and font, so a version that
    // moved a sprite frame cannot turn the button into a missing-texture square.
    auto sprite = ButtonSprite::create(text.c_str(), "goldFont.fnt", "GJ_button_01.png", scale);
    if (!sprite) return nullptr;
    return CCMenuItemSpriteExtra::create(sprite, target, handler);
}

} // namespace fstreak::ui
