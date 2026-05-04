#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/cocos.hpp>
#include <vector>

using namespace geode::prelude;



struct VSRGNote {
    int startFrame;
    int endFrame;
    int btn;
    bool is2p;
    bool hitStart = false;
    bool hitEnd = false;
    bool missedStart = false;
    bool missedEnd = false;
};

struct HitErrorTick {
    float time; // to fade out over time
    float errorMs;
    ccColor4F color;
};

class VSRGOverlay : public CCNode {
protected:
    std::vector<VSRGNote> m_notes;
    std::vector<HitErrorTick> m_hitErrors;

    CCDrawNode* m_background;
    CCDrawNode* m_judgmentNode;
    CCDrawNode* m_notesNode; // For hold bodies
    CCNode* m_spriteNode; // Parent for sprite pooling
    
    std::vector<CCSprite*> m_startSpritesPool;
    std::vector<CCSprite*> m_endSpritesPool;
    std::vector<CCSprite*> m_backnoteSpritesPool;

    ccColor4F m_lnBodyColor = {0.6f, 0.4f, 0.8f, 0.8f};
    
    CCLabelBMFont* m_offsetLabel;
    CCLabelBMFont* m_comboLabel;
    CCLabelBMFont* m_errorMsLabel;
    
    // Visual Effects
    float m_laneOpacity[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Settings
    float m_scrollSpeed = 2.0f;
    float m_laneHeight = 400.0f;
    float m_laneWidth = 100.0f; 
    float m_laneX = -120.0f;
    float m_laneY = 20.0f;
    float m_noteWidth = 14.0f;
    float m_comboXOffset = 0.0f;
    float m_comboYOffset = 0.0f;
    float m_hitBarYOffset = 15.0f;
    float m_errorMsYOffset = 25.0f;
    float m_hitWindowFps = 240.0f;
    float m_hitSoundVolume = 50.0f;
    float m_releaseMissThreshold = 100.0f;
    float m_laneOpacitySetting = 0.25f;
    bool m_autoMode = false;
    
    // Sync
    float m_baseOffset = 0.0f;
    float m_userOffset = 0.0f;
    double m_currentSmoothedFrame = 0.0;
    
    int m_currentCombo = 0;
    
    // Input Handling
    bool m_keysHeld[8] = {false, false, false, false, false, false, false, false};
    std::vector<geode::Keybind> m_keyLane1;
    std::vector<geode::Keybind> m_keyLane2;
    std::vector<geode::Keybind> m_keyLane3;
    std::vector<geode::Keybind> m_keyLane4;
    std::vector<geode::Keybind> m_keySpeedUp;
    std::vector<geode::Keybind> m_keySpeedDown;
    std::vector<geode::Keybind> m_keyOffsetUp;
    std::vector<geode::Keybind> m_keyOffsetDown;
    std::vector<geode::Keybind> m_keyAutoMode;

//    float m_comboXOffset = 0.0f;
//    float m_comboYOffset = 0.0f;

    std::vector<int> m_lastLanes;
    

    void parseJSON(const std::string& path);
    void processNoteMapping();
    void drawNotes(int currentFrame, float dt);
    
    CCSprite* getStartSprite(int index);
    CCSprite* getEndSprite(int index);
    CCSprite* getBacknoteSprite(int index);
    void extractLnBodyColor();
    void resetNotesState();

public:
    static VSRGOverlay* create(PlayLayer* playLayer);
    virtual ~VSRGOverlay();
    bool init(PlayLayer* playLayer);
    void update(float dt) override;
    void reloadMacro(const std::string& path);
    void saveState();
    static std::string getLevelKey(PlayLayer* playLayer);
    
    void playHitSound();
    static std::string getLevelKey(GJGameLevel* level);

    void onKeyDown(cocos2d::enumKeyCodes key);
    void onKeyUp(cocos2d::enumKeyCodes key);
    
    bool m_needsReset = false;
    void onResetLevel() { m_needsReset = true; }
};
