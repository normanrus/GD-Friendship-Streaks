#include "PairingPopup.hpp"

#include "Ui.hpp"

#include "../Notify.hpp"
#include "../StreakStore.hpp"

#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

namespace fstreak {
namespace {

constexpr float POPUP_WIDTH = 400.f;
constexpr float POPUP_HEIGHT = 300.f;

} // namespace

PairingPopup* PairingPopup::create() {
    auto ret = new PairingPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void PairingPopup::open() {
    if (auto popup = PairingPopup::create()) {
        popup->show();
    }
}

bool PairingPopup::init() {
    if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT)) return false;

    this->setID("friendship-streaks-pairing"_spr);
    this->setTitle("Pair a friend");

    auto& store = StreakStore::get();
    store.load();

    auto intro = ui::label(
        "A friendship streak belongs to two players.\nShare your code, or enter the code your friend sends you.",
        "chatFont.fnt", 0.6f, ui::COLOR_MUTED
    );
    if (intro) {
        intro->setPosition({POPUP_WIDTH / 2.f, POPUP_HEIGHT - 62.f});
        m_mainLayer->addChild(intro, 1);
    }

    if (auto line = ui::rect(POPUP_WIDTH - 60.f, 1.5f, {255, 255, 255, 36})) {
        line->setPosition({POPUP_WIDTH / 2.f, POPUP_HEIGHT - 84.f});
        m_mainLayer->addChild(line, 1);
    }

    this->buildInviteSection();
    this->buildRedeemSection();

    m_statusLabel = ui::label("", "chatFont.fnt", 0.58f, ui::COLOR_MUTED);
    if (m_statusLabel) {
        m_statusLabel->setPosition({POPUP_WIDTH / 2.f, 22.f});
        m_mainLayer->addChild(m_statusLabel, 2);
    }

    if (!store.configured()) {
        if (m_statusLabel) m_statusLabel->setString("Set the sync server URL in mod settings first");
        return true;
    }
    if (!store.hasAccount()) {
        if (m_statusLabel) m_statusLabel->setString("Log in to a GD account to create your code");
        return true;
    }
    this->refreshInviteCode();
    return true;
}

void PairingPopup::buildInviteSection() {
    auto node = CCNode::create();
    node->setPosition({POPUP_WIDTH / 2.f, 164.f});
    m_mainLayer->addChild(node, 1);
    m_inviteNode = node;

    auto title = ui::label("YOUR CODE", "chatFont.fnt", 0.5f, ui::COLOR_MUTED, {0.f, 0.5f});
    if (title) {
        title->setPosition({-160.f, 34.f});
        node->addChild(title);
    }

    if (auto pill = ui::panel(300.f, 44.f, {255, 255, 255, 30})) {
        pill->setPosition({0.f, 4.f});
        node->addChild(pill, 0);
    }

    m_codeLabel = ui::label("------", "bigFont.fnt", 1.f, ui::COLOR_GOLD);
    if (m_codeLabel) {
        m_codeLabel->setPosition({0.f, 4.f});
        node->addChild(m_codeLabel, 2);
    }

    auto menu = CCMenu::create();
    menu->setPosition({0.f, -36.f});
    node->addChild(menu, 2);

    if (auto generate = ui::button("New code", 0.8f, this, menu_selector(PairingPopup::onGenerate))) {
        generate->setPosition({-62.f, 0.f});
        menu->addChild(generate);
    }
    if (auto copy = ui::button("Copy", 0.8f, this, menu_selector(PairingPopup::onCopy))) {
        copy->setPosition({62.f, 0.f});
        menu->addChild(copy);
    }
}

void PairingPopup::buildRedeemSection() {
    auto node = CCNode::create();
    node->setPosition({POPUP_WIDTH / 2.f, 78.f});
    m_mainLayer->addChild(node, 1);

    auto title = ui::label("FRIEND'S CODE", "chatFont.fnt", 0.5f, ui::COLOR_MUTED, {0.f, 0.5f});
    if (title) {
        title->setPosition({-160.f, 26.f});
        node->addChild(title);
    }

    m_input = TextInput::create(220.f, "ABC123", "bigFont.fnt");
    if (m_input) {
        m_input->setMaxCharCount(12);
        m_input->setPosition({-30.f, -4.f});
        node->addChild(m_input, 1);
    }

    auto menu = CCMenu::create();
    menu->setPosition({132.f, -4.f});
    node->addChild(menu, 2);
    if (auto link = ui::button("Link", 0.85f, this, menu_selector(PairingPopup::onLink))) {
        link->setPosition({0.f, 0.f});
        menu->addChild(link);
    }
}

void PairingPopup::refreshInviteCode() {
    auto& store = StreakStore::get();
    if (m_statusLabel) m_statusLabel->setString("Creating a code...");

    geode::Ref<PairingPopup> self = this;
    store.createInvite([self](std::string code, std::string error) {
        if (!self->m_codeLabel || !self->m_statusLabel) return;
        if (!error.empty()) {
            self->m_statusLabel->setString(fmt::format("Could not create a code ({})", error).c_str());
            return;
        }
        self->m_codeLabel->setString(code.c_str());
        self->m_statusLabel->setString("Send this code to your friend in a GD message");
        self->m_codeLabel->runAction(CCSequence::create(
            CCScaleTo::create(0.12f, 1.15f), CCScaleTo::create(0.16f, 1.f), nullptr
        ));
    });
}

void PairingPopup::onGenerate(CCObject*) {
    if (!StreakStore::get().configured()) {
        notify::toast("Set the server URL in mod settings first", 3.f);
        return;
    }
    this->refreshInviteCode();
}

void PairingPopup::onCopy(CCObject*) {
    if (!m_codeLabel) return;
    std::string code = m_codeLabel->getString();
    if (code == "------" || code.empty()) {
        notify::toast("Create a code first", 2.f);
        return;
    }
    geode::utils::clipboard::write(code);
    notify::toast("Code copied", 1.6f);
}

void PairingPopup::onLink(CCObject*) {
    auto& store = StreakStore::get();
    if (!store.configured()) {
        notify::toast("Set the server URL in mod settings first", 3.f);
        return;
    }

    std::string code = m_input ? std::string(m_input->getString()) : std::string();
    while (!code.empty() && (code.back() == ' ' || code.back() == '\n')) code.pop_back();
    while (!code.empty() && code.front() == ' ') code.erase(code.begin());
    if (code.empty()) {
        if (m_statusLabel) m_statusLabel->setString("Enter the code your friend sent you");
        return;
    }

    if (m_statusLabel) m_statusLabel->setString("Linking...");
    geode::Ref<PairingPopup> self = this;
    store.redeem(code, [self](bool ok, std::string error) {
        if (!self->m_statusLabel) return;
        if (!ok) {
            self->m_statusLabel->setString(fmt::format("Link failed ({})", error).c_str());
            notify::toast("Could not link that code", 2.5f);
            return;
        }
        self->m_statusLabel->setString("Linked! Your shared streak starts today.");
        notify::toast("Friendship streak linked", 2.5f);
    });
}

} // namespace fstreak
