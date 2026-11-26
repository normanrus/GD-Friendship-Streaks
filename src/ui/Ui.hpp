#pragma once
// Small drawing helpers so the three popups share one visual language.

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>

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

inline geode::NineSlice* panel(float width, float height, cocos2d::ccColor4B tint = {255, 255, 255, 255}) {
    auto panel = geode::NineSlice::create("pill.png"_spr, {0.f, 0.f, width, height}, {18.f, 18.f, 18.f, 18.f});
    if (!panel) return nullptr;
    panel->setColor({tint.r, tint.g, tint.b});
    panel->setOpacity(tint.a);
    return panel;
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
    // Drawn from the mod's own pill instead of a game sheet frame: a frame that is
    // missing on the running GD version renders as the pink missing-texture square.
    auto caption = label(text, "bigFont.fnt", 0.42f * scale, {255, 255, 255});
    if (!caption) return nullptr;

    float width = std::max(92.f, caption->getContentSize().width * 0.42f * scale + 32.f);
    constexpr float height = 34.f;

    cocos2d::CCNode* background = nullptr;
    if (auto slice = NineSlice::create("pill.png"_spr, {0.f, 0.f, width, height}, {14.f, 14.f, 14.f, 14.f})) {
        slice->setColor({96, 152, 232});
        background = slice;
    } else if (auto solid = rect(width, height, {96, 152, 232, 255})) {
        background = solid;
    }
    if (!background) return nullptr;

    caption->setPosition({width / 2.f, height / 2.f - 1.f});
    background->addChild(caption);

    auto item = CCMenuItemSpriteExtra::create(background, target, handler);
    item->setScale(scale);
    return item;
}

} // namespace fstreak::ui
