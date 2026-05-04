#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "VSRGOverlay.hpp"

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        log::info("-----------------------------------------");
        log::info("[VSRG] PlayLayer::init START");
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        
        log::info("[VSRG] PlayLayer::init SUCCESS");
        
        if (auto overlay = VSRGOverlay::create(this)) {
            this->addChild(overlay, 100); 
            log::info("VSRG Overlay added to PlayLayer");
        }
        
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (auto overlay = this->getChildByType<VSRGOverlay>(0)) {
            overlay->onResetLevel();
        }
    }

    void onQuit() {
        PlayLayer::onQuit();
    }
};

#include <Geode/modify/CCKeyboardDispatcher.hpp>

class $modify(MyKeyboardDispatcher, CCKeyboardDispatcher) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("CCKeyboardDispatcher::dispatchKeyboardMSG", -9999999);
        (void)self.setHookPriority("cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG", -9999999);
    }

    bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double timestamp) {
        // Forward to VSRGOverlay if we are in a level
        auto playLayer = PlayLayer::get();
        if (playLayer && !isKeyRepeat) { // We only care about press/release, not repeat
            if (auto overlay = playLayer->getChildByType<VSRGOverlay>(0)) {
                if (isKeyDown) {
                    overlay->onKeyDown(key);
                } else {
                    overlay->onKeyUp(key);
                }
            }
        }
        
        // Always call original to let the game process normal inputs (like pause, jump)
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat, timestamp);
    }
};
