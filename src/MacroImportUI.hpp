#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class MacroImportUI {
public:
    static void onImportVSRGMacro(GJGameLevel* level, CCMenu* menu);
    static void onDeleteVSRGMacro(GJGameLevel* level, CCMenu* menu);
    static void onToggleVSRG(CCObject* sender);
    static void updateButtons(CCMenu* menu, GJGameLevel* level);
};
