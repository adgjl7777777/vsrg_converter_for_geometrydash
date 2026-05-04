#include "MacroImportUI.hpp"
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <fstream>
#include "VSRGOverlay.hpp"

class $modify(MyLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        
        auto menu = CCMenu::create();
        menu->setID("vsrg-menu"_spr);
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        // Place it to the right of the copy button on the left
        menu->setPosition({ 115, winSize.height / 2 - 30 });
        this->addChild(menu);
        
        MacroImportUI::updateButtons(menu, level);
        
        return true;
    }
};

class $modify(MyEditLevelLayer, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;
        
        auto menu = CCMenu::create();
        menu->setID("vsrg-menu"_spr);
        
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({ 115, winSize.height / 2 - 30 });
        this->addChild(menu);
        
        MacroImportUI::updateButtons(menu, level);
        
        return true;
    }
};

void MacroImportUI::updateButtons(CCMenu* menu, GJGameLevel* level) {
    menu->removeAllChildren();
    
    std::string levelKey = VSRGOverlay::getLevelKey(level);
    std::filesystem::path saveDir = Mod::get()->getSaveDir();
    std::filesystem::path macroPath = saveDir / (levelKey + ".gdr.json");
    
    auto layout = ColumnLayout::create();
    layout->setGap(5.0f);
    
    if (std::filesystem::exists(macroPath)) {
        auto reimportBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Reimport VSRG", "bigFont.fnt", "GJ_button_01.png", 0.3f),
            [level, menu](CCObject*) { MacroImportUI::onImportVSRGMacro(level, menu); }
        );
        auto deleteBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Delete VSRG", "bigFont.fnt", "GJ_button_06.png", 0.3f),
            [level, menu](CCObject*) { MacroImportUI::onDeleteVSRGMacro(level, menu); }
        );
        reimportBtn->setScaleX(0.5f);
        reimportBtn->setScaleY(0.4f);
        deleteBtn->setScaleX(0.5f);
        deleteBtn->setScaleY(0.4f);
        menu->addChild(reimportBtn);
        menu->addChild(deleteBtn);
        
        bool vsrgEnabled = Mod::get()->getSavedValue<bool>("vsrg-enabled-" + levelKey, true);
        auto toggleSpr = CCSprite::createWithSpriteFrameName(vsrgEnabled ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png");
        toggleSpr->setScale(0.8f);
        auto toggleBtn = CCMenuItemExt::createSpriteExtra(toggleSpr, [levelKey, toggleSpr](CCObject*) {
            bool current = Mod::get()->getSavedValue<bool>("vsrg-enabled-" + levelKey, true);
            bool newState = !current;
            Mod::get()->setSavedValue<bool>("vsrg-enabled-" + levelKey, newState);
            toggleSpr->setDisplayFrame(CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(newState ? "GJ_checkOn_001.png" : "GJ_checkOff_001.png"));
            log::info("[VSRG] Toggled VSRG mode for {} to {}", levelKey, newState);
        });
        menu->addChild(toggleBtn);
        
    } else {
        auto importBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Import VSRG", "bigFont.fnt", "GJ_button_01.png", 0.3f),
            [level, menu](CCObject*) { MacroImportUI::onImportVSRGMacro(level, menu); }
        );
        importBtn->setScaleX(0.5f);
        importBtn->setScaleY(0.4f);
        menu->addChild(importBtn);
    }
    
    menu->setLayout(layout);
}

void MacroImportUI::onImportVSRGMacro(GJGameLevel* level, CCMenu* menu) {
    std::filesystem::path saveDir = Mod::get()->getSaveDir();
    std::string levelKey = VSRGOverlay::getLevelKey(level);
    
    FLAlertLayer::create(
        "Import VSRG Macro", 
        fmt::format("Due to a bug in Geode v3, the file picker is disabled.\n\nPlease manually copy your macro .json file to:\n<cy>{}</c>\n\nName it <cg>{}.gdr.json</c>.", saveDir.string(), levelKey),
        "OK"
    )->show();
}

void MacroImportUI::onDeleteVSRGMacro(GJGameLevel* level, CCMenu* menu) {
    std::string levelKey = VSRGOverlay::getLevelKey(level);
    std::filesystem::path saveDir = Mod::get()->getSaveDir();
    std::filesystem::path macroPath = saveDir / (levelKey + ".gdr.json");
    
    if (std::filesystem::exists(macroPath)) {
        std::filesystem::remove(macroPath);
    }
    
    MacroImportUI::updateButtons(menu, level);
    FLAlertLayer::create("Deleted", "Macro deleted successfully.", "OK")->show();
}
