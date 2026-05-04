#include "VSRGOverlay.hpp"
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <fstream>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#endif

using namespace geode::prelude;

VSRGOverlay *VSRGOverlay::create(PlayLayer *playLayer) {
  auto ret = new VSRGOverlay();
  if (ret && ret->init(playLayer)) {
    ret->autorelease();
    return ret;
  }
  CC_SAFE_DELETE(ret);
  return nullptr;
}

VSRGOverlay::~VSRGOverlay() { this->saveState(); }

std::string VSRGOverlay::getLevelKey(PlayLayer *playLayer) {
  if (playLayer && playLayer->m_level) {
    return getLevelKey(playLayer->m_level);
  }
  return "unknown";
}

std::string VSRGOverlay::getLevelKey(GJGameLevel *level) {
  if (level) {
    int id = level->m_levelID.value();
    if (id == 0) {
      std::string name = std::string(level->m_levelName.c_str());
      log::info("[VSRG] Editor level key: {} ({})", name, id);
      return "editor_" + name;
    }
    return std::to_string(id);
  }
  return "unknown";
}

void VSRGOverlay::saveState() {
  auto playLayer = PlayLayer::get();
  if (playLayer && playLayer->m_level) {
    std::string levelKey = getLevelKey(playLayer);

    matjson::Value obj = matjson::makeObject({});
    obj["offset"] = m_userOffset;

    Mod::get()->setSavedValue("map-settings-" + levelKey, obj);
    log::info("Saved map settings for level {}", levelKey);
  }
}

bool VSRGOverlay::init(PlayLayer *playLayer) {
  if (!CCNode::init())
    return false;

  bool vsrgEnabled = true;
  if (playLayer && playLayer->m_level) {
    std::string levelKey = getLevelKey(playLayer);
    vsrgEnabled =
        Mod::get()->getSavedValue<bool>("vsrg-enabled-" + levelKey, true);
  }
  if (!vsrgEnabled)
    return false;

  // Load Settings
  m_noteWidth = Mod::get()->getSettingValue<double>("note-width");
  m_laneWidth = m_noteWidth * 4.0f * 1.5f;
  m_laneHeight = Mod::get()->getSettingValue<double>("lane-height");
  m_scrollSpeed = Mod::get()->getSettingValue<double>("scroll-speed");
  m_laneX = Mod::get()->getSettingValue<double>("lane-x-offset");
  m_laneY = Mod::get()->getSettingValue<double>("lane-y-offset");

  m_keyLane1 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-1");
  m_keyLane2 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-2");
  m_keyLane3 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-3");
  m_keyLane4 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-4");
  m_keySpeedUp =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-speed-up");
  m_keySpeedDown = Mod::get()->getSettingValue<std::vector<geode::Keybind>>(
      "key-speed-down");
  m_keyOffsetUp =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-offset-up");
  m_keyOffsetDown = Mod::get()->getSettingValue<std::vector<geode::Keybind>>(
      "key-offset-down");
  m_keyAutoMode =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-auto-mode");

  m_comboYOffset = Mod::get()->getSettingValue<double>("combo-y-offset");
  m_hitBarYOffset = Mod::get()->getSettingValue<double>("hit-bar-y-offset");
  m_errorMsYOffset = Mod::get()->getSettingValue<double>("error-ms-y-offset");
  m_hitWindowFps = Mod::get()->getSettingValue<int>("hit-window-fps");
  m_hitSoundVolume = Mod::get()->getSettingValue<double>("hit-sound-volume");
  m_releaseMissThreshold = Mod::get()->getSettingValue<double>("release-miss-threshold");
  m_laneOpacitySetting = Mod::get()->getSettingValue<double>("lane-opacity");
  m_autoMode = Mod::get()->getSettingValue<bool>("auto-mode");

  srand(static_cast<unsigned int>(time(nullptr)));

  if (playLayer && playLayer->m_level) {
    float songOffset = playLayer->m_levelSettings
                           ? playLayer->m_levelSettings->m_songOffset
                           : 0.0f;
    m_baseOffset = -(songOffset * 240.0f);
    srand(playLayer->m_level->m_levelID.value()); // Stable random for this map
  }

  // Attempt to load map-specific saved settings (like user offset)
  if (playLayer && playLayer->m_level) {
    std::string levelKey = getLevelKey(playLayer);
    auto savedData =
        Mod::get()->getSavedValue<matjson::Value>("map-settings-" + levelKey);
    if (savedData.isObject()) {
      if (savedData.contains("offset")) {
        m_userOffset = savedData["offset"].asDouble().unwrapOr(0.0);
      }
    }
  }

  // 1. Setup Background
  m_background = CCDrawNode::create();
  this->addChild(m_background, 0);

  auto winSize = CCDirector::get()->getWinSize();
  CCPoint laneOrigin = ccp(winSize.width + m_laneX, m_laneY);

  m_background->drawRect(
      laneOrigin, ccp(laneOrigin.x + m_laneWidth, laneOrigin.y + m_laneHeight),
      ccc4f(0, 0, 0, 0.5f), 1, ccc4f(1, 1, 1, 1));

  std::vector<CCPoint> lanePoly = {
      laneOrigin, ccp(laneOrigin.x + m_laneWidth, laneOrigin.y),
      ccp(laneOrigin.x + m_laneWidth, laneOrigin.y + m_laneHeight),
      ccp(laneOrigin.x, laneOrigin.y + m_laneHeight)};

  // 2. Setup Clippers & Nodes
  auto stencil = CCDrawNode::create();
  stencil->drawPolygon(&lanePoly[0], 4, ccc4f(1, 1, 1, 1), 0,
                       ccc4f(0, 0, 0, 0));

  auto clipper = CCClippingNode::create(stencil);
  this->addChild(clipper, 1);

  m_notesNode = CCDrawNode::create();
  clipper->addChild(m_notesNode, 1);

  m_spriteNode = CCNode::create();
  clipper->addChild(m_spriteNode, 2);

  m_judgmentNode = CCDrawNode::create();
  this->addChild(m_judgmentNode, 7);

  // 3. Setup UI Labels
  m_offsetLabel =
      CCLabelBMFont::create("Offset: 0.0 | Spd: 2.0", "goldFont.fnt");
  m_offsetLabel->setAnchorPoint(ccp(0, 0));
  m_offsetLabel->setPosition(ccp(10, 10));
  m_offsetLabel->setScale(0.35f);
  this->addChild(m_offsetLabel, 8);

  m_comboLabel = CCLabelBMFont::create("0", "goldFont.fnt");
  m_comboLabel->setPosition(
      ccp(laneOrigin.x + m_laneWidth / 2, winSize.height / 2 + m_comboYOffset));
  m_comboLabel->setScale(1.0f);
  this->addChild(m_comboLabel, 10);

  m_errorMsLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_errorMsLabel->setPosition(
      ccp(laneOrigin.x + m_laneWidth / 2, laneOrigin.y + m_errorMsYOffset));
  m_errorMsLabel->setScale(0.3f);
  this->addChild(m_errorMsLabel, 10);

  // Extract lnbody color
  extractLnBodyColor();

  // Initial Load
  if (playLayer && playLayer->m_level) {
    std::string levelKey = getLevelKey(playLayer);
    std::filesystem::path saveDir = Mod::get()->getSaveDir();
    std::filesystem::path jsonPath = saveDir / (levelKey + ".gdr.json");
    parseJSON(jsonPath.string());
  }

  if (m_notes.empty())
    return false;

  this->scheduleUpdate();
  return true;
}

void VSRGOverlay::reloadMacro(const std::string &path) { parseJSON(path); }

void VSRGOverlay::parseJSON(const std::string &jsonPathStr) {
  m_notes.clear();
  std::filesystem::path jsonPath = jsonPathStr;

  if (!std::filesystem::exists(jsonPath)) {
    log::info("No VSRG macro found for this map at {}", jsonPathStr);
    return;
  }

  std::ifstream file(jsonPath);
  if (!file.is_open())
    return;

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  try {
    auto result = matjson::parse(content);
    if (!result)
      return;
    auto json = result.unwrap();

    if (json.contains("inputs") && json["inputs"].isArray()) {
      std::map<int, int> holdStarts;
      for (auto &item : json["inputs"].asArray().unwrap()) {
        int frame = item["frame"].asInt().unwrapOr(0);
        int btn = item["btn"].asInt().unwrapOr(0);
        bool down = item["down"].asBool().unwrapOr(false);
        bool is2p =
            item.contains("2p") ? item["2p"].asBool().unwrapOr(false) : false;

        int key = btn | (is2p ? 256 : 0);

        if (down) {
          if (holdStarts.find(key) == holdStarts.end())
            holdStarts[key] = frame;
        } else {
          if (holdStarts.find(key) != holdStarts.end()) {
            VSRGNote note;
            note.startFrame = holdStarts[key];
            note.endFrame = frame;
            note.btn = btn;
            note.is2p = is2p;
            m_notes.push_back(note);
            holdStarts.erase(key);
          }
        }
      }
    }
    this->processNoteMapping();
  } catch (const std::exception &e) {
    log::error("Exception in JSON parsing: {}", e.what());
  }
}

void VSRGOverlay::playHitSound() {
  if (m_hitSoundVolume <= 0.0f)
    return;
  std::string skinFolder =
      Mod::get()->getSettingValue<std::string>("skin-folder");
  std::filesystem::path skinPath =
      Mod::get()->getConfigDir() / "skins" / skinFolder / "hit.wav";

  if (std::filesystem::exists(skinPath)) {
    std::string pathStr = skinPath.string();
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
    FMODAudioEngine::sharedEngine()->playEffect(pathStr, 1.0f, 1.0f,
                                                m_hitSoundVolume / 100.0f);
  }
}

void VSRGOverlay::processNoteMapping() {
  if (m_notes.empty())
    return;

  std::sort(m_notes.begin(), m_notes.end(),
            [](const VSRGNote &a, const VSRGNote &b) {
              return a.startFrame < b.startFrame;
            });

  bool comfortMode = Mod::get()->getSettingValue<bool>("comfort-mode");
  if (comfortMode) {
    std::vector<VSRGNote> filtered;
    int busyUntil = -1;
    for (const auto &n : m_notes) {
      if (n.startFrame >= busyUntil) {
        filtered.push_back(n);
        busyUntil = n.endFrame; // Blocks other keys until this note ends
      }
    }
    m_notes = filtered;
  }

  float cps2k = Mod::get()->getSettingValue<double>("split-cps-2k");
  float cps3k = Mod::get()->getSettingValue<double>("split-cps-3k");
  float cps4k = Mod::get()->getSettingValue<double>("split-cps-4k");
  bool fullyRandom = Mod::get()->getSettingValue<bool>("fully-random-mode");

  int lastFrame = -1;
  std::vector<int> lastUsedLanes;
  std::vector<int> currentGroupLanes;

  for (size_t i = 0; i < m_notes.size(); i++) {
    if (m_notes[i].startFrame != lastFrame) {
      lastUsedLanes = currentGroupLanes;
      currentGroupLanes.clear();
      lastFrame = m_notes[i].startFrame;
    }

    int gap = 30;
    if (i > 0)
      gap = m_notes[i].startFrame - m_notes[i - 1].startFrame;
    if (gap <= 0)
      gap = 1;

    float cps = 240.0f / gap;
    int lanesToUse = 1;

    if (cps >= cps4k)
      lanesToUse = 4;
    else if (cps >= cps3k)
      lanesToUse = 3;
    else if (cps >= cps2k)
      lanesToUse = 2;

    int lane;
    int attempts = 0;
    do {
      if (fullyRandom) {
        lane = rand() % 4;
      } else {
        if (lanesToUse == 1) {
          lane = 1;
        } else {
          lane = rand() % lanesToUse;
          if (lanesToUse == 2)
            lane += 1;
          else if (lanesToUse == 3)
            lane += 0;
        }
      }
      attempts++;

      // Exclude lanes used in previous group AND lanes already used in current
      // group
      bool conflict = false;
      for (int l : lastUsedLanes)
        if (l == lane)
          conflict = true;
      for (int l : currentGroupLanes)
        if (l == lane)
          conflict = true;

      if (!conflict)
        break;
    } while (attempts < 20);

    m_notes[i].btn = lane;
    currentGroupLanes.push_back(lane);
  }
}

CCSprite *VSRGOverlay::getStartSprite(int index) {
  while (m_startSpritesPool.size() <= index) {
    std::string skinFolder =
        Mod::get()->getSettingValue<std::string>("skin-folder");
    std::filesystem::path skinPath =
        Mod::get()->getConfigDir() / "skins" / skinFolder / "holdstart.png";

    CCSprite *spr = nullptr;
    if (std::filesystem::exists(skinPath)) {
      // Ensure forward slashes for Cocos
      std::string path = skinPath.string();
      std::replace(path.begin(), path.end(), '\\', '/');
      spr = CCSprite::create(path.c_str());
    }

    if (!spr) {
      std::string bundledPath = fmt::format("{}/skins/{}/holdstart.png",
                                            Mod::get()->getID(), skinFolder);
      spr = CCSprite::create(bundledPath.c_str());
    }

    if (!spr)
      spr = CCSprite::create();

    spr->retain();
    m_startSpritesPool.push_back(spr);
    m_spriteNode->addChild(spr);
  }
  return m_startSpritesPool[index];
}

CCSprite *VSRGOverlay::getEndSprite(int index) {
  while (m_endSpritesPool.size() <= index) {
    std::string skinFolder =
        Mod::get()->getSettingValue<std::string>("skin-folder");
    std::filesystem::path skinPath =
        Mod::get()->getConfigDir() / "skins" / skinFolder / "holdend.png";

    CCSprite *spr = nullptr;
    if (std::filesystem::exists(skinPath)) {
      std::string path = skinPath.string();
      std::replace(path.begin(), path.end(), '\\', '/');
      spr = CCSprite::create(path.c_str());
    }

    if (!spr) {
      std::string bundledPath = fmt::format("{}/skins/{}/holdend.png",
                                            Mod::get()->getID(), skinFolder);
      spr = CCSprite::create(bundledPath.c_str());
    }

    if (!spr)
      spr = CCSprite::create();

    spr->retain();
    m_endSpritesPool.push_back(spr);
    m_spriteNode->addChild(spr);
  }
  return m_endSpritesPool[index];
}

CCSprite *VSRGOverlay::getBacknoteSprite(int index) {
  while (m_backnoteSpritesPool.size() <= index) {
    std::string skinFolder =
        Mod::get()->getSettingValue<std::string>("skin-folder");
    std::filesystem::path skinPath =
        Mod::get()->getConfigDir() / "skins" / skinFolder / "backnote.png";

    CCSprite *spr = nullptr;
    if (std::filesystem::exists(skinPath)) {
      std::string path = skinPath.string();
      std::replace(path.begin(), path.end(), '\\', '/');
      spr = CCSprite::create(path.c_str());
    }
    if (!spr) {
      std::string bundledPath = fmt::format("{}/skins/{}/backnote.png",
                                            Mod::get()->getID(), skinFolder);
      spr = CCSprite::create(bundledPath.c_str());
    }
    if (!spr)
      spr = CCSprite::create();

    spr->retain();
    m_backnoteSpritesPool.push_back(spr);
    m_spriteNode->addChild(spr);
  }
  return m_backnoteSpritesPool[index];
}

void VSRGOverlay::update(float dt) {
  auto playLayer = PlayLayer::get();
  if (!playLayer)
    return;

  if (m_needsReset) {
    double fmodSeconds =
        FMODAudioEngine::sharedEngine()->getMusicTimeMS(0) / 1000.0;
    double fmodFrame = fmodSeconds * 240.0;
    m_currentSmoothedFrame = fmodFrame;
    resetNotesState();
    m_needsReset = false;
  }

  // Update labels and UI state
  m_noteWidth = Mod::get()->getSettingValue<double>("note-width");
  m_laneWidth = m_noteWidth * 4.0f * 1.5f;
  m_laneHeight = Mod::get()->getSettingValue<double>("lane-height");
  m_laneX = Mod::get()->getSettingValue<double>("lane-x-offset");
  m_laneY = Mod::get()->getSettingValue<double>("lane-y-offset");
  m_comboXOffset = Mod::get()->getSettingValue<double>("combo-x-offset");
  m_comboYOffset = Mod::get()->getSettingValue<double>("combo-y-offset");

  auto winSize = CCDirector::get()->getWinSize();
  CCPoint laneOrigin = ccp(winSize.width + m_laneX, m_laneY);
  if (m_comboLabel) {
    m_comboLabel->setPosition(
        ccp(laneOrigin.x + m_laneWidth / 2 + m_comboXOffset,
            winSize.height / 2 + m_comboYOffset));
  }

  // Save state on key release (rough impl: could save on destructor too)
  // We will do a full save in the destructor or UI.

#ifdef GEODE_IS_WINDOWS
  // Direct OS polling to bypass all Geode/GD event consumption
  auto checkKey = [](const std::vector<geode::Keybind> &binds) {
    for (auto &b : binds) {
      int vk = (int)b.key;
      if (GetAsyncKeyState(vk) & 0x8000)
        return true;
    }
    return false;
  };

  bool currentLanes[4] = {checkKey(m_keyLane1), checkKey(m_keyLane2),
                          checkKey(m_keyLane3), checkKey(m_keyLane4)};

  // We need currentFrame for hit logic, so we compute a rough estimate or use
  // the last one
  double currentFrameTemp =
      m_currentSmoothedFrame + m_baseOffset + m_userOffset;
  // Dynamically read settings that might change during gameplay
  m_hitWindowFps = Mod::get()->getSettingValue<int>("hit-window-fps");
  m_autoMode = Mod::get()->getSettingValue<bool>("auto-mode");
  m_laneOpacitySetting = Mod::get()->getSettingValue<double>("lane-opacity");

  float fpsBase = m_hitWindowFps > 0.0f ? m_hitWindowFps : 240.0f;
  float msPerFrame = 1000.0f / fpsBase;
  float goldThresh = msPerFrame * 1.0f;
  float whiteThresh = msPerFrame * 2.0f;
  float greenThresh = msPerFrame * 4.0f;
  float blueThresh = msPerFrame * 8.0f;
  float purpleThresh = msPerFrame * 12.0f;
  float missThresh = purpleThresh;
  float maxErrorFrames = (missThresh / 1000.0f) * 240.0f;

  if (m_autoMode) {
    for (auto &note : m_notes) {
      if (!note.hitStart && currentFrameTemp >= note.startFrame) {
        note.hitStart = true;
        m_currentCombo++;
        if (m_comboLabel)
          m_comboLabel->setString(std::to_string(m_currentCombo).c_str());
        m_hitErrors.push_back({1.0f, 0.0f, {1.0f, 0.84f, 0.0f, 1.0f}}); // Gold
        if (m_errorMsLabel)
          m_errorMsLabel->setString(fmt::format("{:+.1f}ms", 0.0f).c_str());
        playHitSound();
        m_laneOpacity[note.btn % 4] = m_laneOpacitySetting;
      }
      if (note.hitStart && !note.hitEnd && currentFrameTemp >= note.endFrame) {
        note.hitEnd = true;
        m_currentCombo++;
        if (m_comboLabel)
          m_comboLabel->setString(std::to_string(m_currentCombo).c_str());
        m_hitErrors.push_back({1.0f, 0.0f, {1.0f, 0.84f, 0.0f, 1.0f}}); // Gold
        if (m_errorMsLabel)
          m_errorMsLabel->setString(fmt::format("{:+.1f}ms", 0.0f).c_str());
      }
      if (note.hitStart && !note.hitEnd) {
        m_laneOpacity[note.btn % 4] = m_laneOpacitySetting;
      }
    }
  } else {
    for (int i = 0; i < 4; i++) {
      if (currentLanes[i] && !m_keysHeld[i]) {
        // Just Pressed
        m_keysHeld[i] = true;
        playHitSound(); // Play sound unconditionally on press
        float minDiff = 9999.0f;
        VSRGNote *closest = nullptr;
        for (auto &note : m_notes) {
          if (note.btn % 4 == i && !note.hitStart) {
            float diff = currentFrameTemp - note.startFrame;
            if (std::abs(diff) <= maxErrorFrames && std::abs(diff) < minDiff) {
              minDiff = std::abs(diff);
              closest = &note;
            }
          }
        }
        if (closest) {
          closest->hitStart = true;
          m_currentCombo++;
          if (m_comboLabel)
            m_comboLabel->setString(std::to_string(m_currentCombo).c_str());

          float errorFrames = currentFrameTemp - closest->startFrame;
          float errorMs = errorFrames * (1000.0f / 240.0f);
          ccColor4F color = {1.0f, 0.0f, 0.0f, 1.0f}; // Red default
          if (std::abs(errorMs) <= goldThresh)
            color = {1.0f, 0.84f, 0.0f, 1.0f};
          else if (std::abs(errorMs) <= whiteThresh)
            color = {1.0f, 1.0f, 1.0f, 1.0f};
          else if (std::abs(errorMs) <= greenThresh)
            color = {0.0f, 1.0f, 0.0f, 1.0f};
          else if (std::abs(errorMs) <= blueThresh)
            color = {0.0f, 0.5f, 1.0f, 1.0f};
          else if (std::abs(errorMs) <= purpleThresh)
            color = {0.5f, 0.0f, 0.5f, 1.0f};

          m_hitErrors.push_back({1.0f, errorMs, color});
          if (m_errorMsLabel)
            m_errorMsLabel->setString(
                fmt::format("{:+.1f}ms", errorMs).c_str());
        }
      } else if (!currentLanes[i] && m_keysHeld[i]) {
        // Just Released
        m_keysHeld[i] = false;
        float minDiff = 9999.0f;
        VSRGNote *closest = nullptr;
        for (auto &note : m_notes) {
          if (note.btn % 4 == i && note.hitStart && !note.hitEnd) {
            float diff = currentFrameTemp - note.endFrame;
            if (std::abs(diff) <= maxErrorFrames && std::abs(diff) < minDiff) {
              minDiff = std::abs(diff);
              closest = &note;
            }
          }
        }
        if (closest) {
          closest->hitEnd = true;
          m_currentCombo++;
          if (m_comboLabel)
            m_comboLabel->setString(std::to_string(m_currentCombo).c_str());

          float errorFrames = currentFrameTemp - closest->endFrame;
          float errorMs = errorFrames * (1000.0f / 240.0f);
          ccColor4F color = {1.0f, 0.0f, 0.0f, 1.0f};
          if (std::abs(errorMs) <= goldThresh)
            color = {1.0f, 0.84f, 0.0f, 1.0f};
          else if (std::abs(errorMs) <= whiteThresh)
            color = {1.0f, 1.0f, 1.0f, 1.0f};
          else if (std::abs(errorMs) <= greenThresh)
            color = {0.0f, 1.0f, 0.0f, 1.0f};
          else if (std::abs(errorMs) <= blueThresh)
            color = {0.0f, 0.5f, 1.0f, 1.0f};
          else if (std::abs(errorMs) <= purpleThresh)
            color = {0.5f, 0.0f, 0.5f, 1.0f};

          m_hitErrors.push_back({1.0f, errorMs, color});
          if (m_errorMsLabel)
            m_errorMsLabel->setString(
                fmt::format("{:+.1f}ms", errorMs).c_str());
        }
      }
    }
  }
#endif

  // Sync Logic
  double fmodSeconds =
      FMODAudioEngine::sharedEngine()->getMusicTimeMS(0) / 1000.0;
  double fmodFrame = fmodSeconds * 240.0;
  bool isPlaying = (fmodSeconds > 0.001);

  bool isDead =
      playLayer && playLayer->m_player1 && playLayer->m_player1->m_isDead;

  if (!isDead) {
    if (isPlaying) {
      m_currentSmoothedFrame += dt * 240.0;
      double error = fmodFrame - m_currentSmoothedFrame;
      m_currentSmoothedFrame += error * 5.0 * dt;
      if (std::abs(error) > 10.0)
        m_currentSmoothedFrame = fmodFrame;
    } else {
      m_currentSmoothedFrame = fmodFrame;
    }
  }

  double currentFrame = m_currentSmoothedFrame + m_baseOffset + m_userOffset;

  if (m_offsetLabel) {
    if (m_autoMode) {
      m_offsetLabel->setString(
          CCString::createWithFormat("Offset: %+.1f | Spd: %.1f | Auto: ON",
                                     m_userOffset, m_scrollSpeed)
              ->getCString());
    } else {
      m_offsetLabel->setString(
          CCString::createWithFormat("Offset: %+.1f | Spd: %.1f", m_userOffset,
                                     m_scrollSpeed)
              ->getCString());
    }
  }

  // Process Misses only if not auto mode
  if (!m_autoMode) {
    for (auto &note : m_notes) {
      if (!note.hitStart && !note.missedStart) {
        if (currentFrameTemp - note.startFrame > maxErrorFrames) {
          note.missedStart = true;
          m_currentCombo = 0;
          if (m_comboLabel)
            m_comboLabel->setString("0");
          m_hitErrors.push_back({1.0f, missThresh, {1.0f, 0.0f, 0.0f, 1.0f}});
          if (m_errorMsLabel)
            m_errorMsLabel->setString("MISS");
        }
      }
      bool requireRelease = false;
      if (m_releaseMissThreshold >= 0.0f) {
        float thresholdFrames = (m_releaseMissThreshold / 1000.0f) * 240.0f;
        if (note.endFrame - note.startFrame > thresholdFrames) {
          requireRelease = true;
        }
      }

      if (note.hitStart && !note.hitEnd && !note.missedEnd && requireRelease) {
        if (currentFrameTemp - note.endFrame > maxErrorFrames) {
          note.missedEnd = true;
          m_currentCombo = 0;
          if (m_comboLabel)
            m_comboLabel->setString("0");
          m_hitErrors.push_back({1.0f, missThresh, {1.0f, 0.0f, 0.0f, 1.0f}});
          if (m_errorMsLabel)
            m_errorMsLabel->setString("MISS");
        }
      }
    }
  }

  drawNotes((int)currentFrame, dt);
}

void VSRGOverlay::drawNotes(int currentFrame, float dt) {
  m_notesNode->clear();
  m_judgmentNode->clear();

  for (auto spr : m_startSpritesPool)
    spr->setVisible(false);
  for (auto spr : m_endSpritesPool)
    spr->setVisible(false);
  for (auto spr : m_backnoteSpritesPool)
    spr->setVisible(false);

  int visibleIndex = 0;

  // Update Layout based on settings (but NOT scrollSpeed - that is controlled
  // by keyDown)
  m_noteWidth = Mod::get()->getSettingValue<double>("note-width");
  m_laneWidth = m_noteWidth * 4.0f * 1.5f;
  m_laneHeight = Mod::get()->getSettingValue<double>("lane-height");
  m_laneX = Mod::get()->getSettingValue<double>("lane-x-offset");
  m_laneY = Mod::get()->getSettingValue<double>("lane-y-offset");

  auto winSize = CCDirector::get()->getWinSize();
  CCPoint laneOrigin = ccp(winSize.width + m_laneX, m_laneY);

  // Update Background and Clipper
  m_background->clear();
  m_background->drawRect(
      laneOrigin, ccp(laneOrigin.x + m_laneWidth, laneOrigin.y + m_laneHeight),
      ccc4f(0, 0, 0, 0.5f), 1, ccc4f(1, 1, 1, 1));

  // Note: Stencil update is tricky as it's a separate node, but we'll try to
  // re-draw it Actually we can just keep the stencil very large or update it if
  // needed. For now, let's focus on the lanes.

  float judgmentY = laneOrigin.y + 50;
  float laneWidth = m_laneWidth;
  float noteWidth = m_noteWidth;
  float colWidth = laneWidth / 4.0f;

  for (int i = 1; i < 4; i++) {
    float x = laneOrigin.x + (m_laneWidth * (i / 4.0f));
    m_judgmentNode->drawSegment(ccp(x, laneOrigin.y),
                                ccp(x, laneOrigin.y + m_laneHeight), 0.5f,
                                ccc4f(1, 1, 1, 0.2f));
  }

  // Draw backnote sprites instead of red judgment line
  for (int i = 0; i < 4; i++) {
    auto spr = getBacknoteSprite(i);
    spr->setVisible(true);
    float colX = laneOrigin.x + (i * colWidth);
    float centerX = colX + colWidth / 2.0f;
    spr->setPosition(ccp(centerX, judgmentY));
    if (spr->getContentSize().width > 0) {
      float scale = (noteWidth * 1.5f) / spr->getContentSize().width;
      spr->setScaleX(scale);
      spr->setScaleY(scale);
    }
  }

  bool laneActive[4] = {m_keysHeld[0], m_keysHeld[1], m_keysHeld[2],
                        m_keysHeld[3]};

  for (int i = 0; i < 4; i++) {
    if (laneActive[i]) {
      m_laneOpacity[i] = m_laneOpacitySetting;
    } else {
      m_laneOpacity[i] -= dt * 1.5f; // 부드럽게 페이드 아웃
      if (m_laneOpacity[i] < 0.0f)
        m_laneOpacity[i] = 0.0f;
    }

    if (m_laneOpacity[i] > 0.0f) {
      float x = laneOrigin.x + (i * colWidth);
      CCPoint verts[4] = {ccp(x, laneOrigin.y), ccp(x + colWidth, laneOrigin.y),
                          ccp(x + colWidth, laneOrigin.y + m_laneHeight),
                          ccp(x, laneOrigin.y + m_laneHeight)};
      m_notesNode->drawPolygon(verts, 4,
                               ccc4f(m_laneOpacity[i], m_laneOpacity[i],
                                     m_laneOpacity[i], m_laneOpacity[i]),
                               0, ccc4f(0, 0, 0, 0));
    }
  }

  for (const auto &note : m_notes) {
    // Skip fully processed notes
    if ((note.hitStart && note.hitEnd) || (note.missedStart && note.missedEnd))
      continue;

    float dyStart = (note.startFrame - currentFrame) * m_scrollSpeed;
    float dyEnd = (note.endFrame - currentFrame) * m_scrollSpeed;

    // If note is hit but not ended, lock the head to judgment line
    if (note.hitStart && !note.hitEnd)
      dyStart = 0;
    // If note is missed but not ended, lock the head to judgment line (for
    // visual clarity of the miss)
    if (note.missedStart && !note.missedEnd)
      dyStart = 0;

    if (dyStart > m_laneHeight && dyEnd > m_laneHeight)
      continue;
    if (dyEnd < -100)
      continue; // Allow some buffer for LN bodies

    float bottomY = judgmentY + dyStart;
    float topY = judgmentY + dyEnd;

    int col = note.btn % 4;
    float colX = laneOrigin.x + (col * colWidth);
    float noteX = colX + (colWidth - noteWidth) / 2.0f;
    float centerX = colX + colWidth / 2.0f;

    // Draw lnbody sprite
    float segmentBottom = std::max(bottomY, laneOrigin.y);
    float segmentTop = std::min(topY, laneOrigin.y + m_laneHeight);

    if (segmentTop > segmentBottom) {
      // Draw colored rectangle for hold body
      float halfW = (noteWidth * 1.5f) / 2.0f;
      CCPoint bodyVerts[4] = {ccp(centerX - halfW, segmentBottom),
                              ccp(centerX + halfW, segmentBottom),
                              ccp(centerX + halfW, segmentTop),
                              ccp(centerX - halfW, segmentTop)};
      m_notesNode->drawPolygon(bodyVerts, 4, m_lnBodyColor, 0,
                               ccc4f(0, 0, 0, 0));
    }

    // Draw Sprites (Heads/Tails)
    if (bottomY >= laneOrigin.y && bottomY <= laneOrigin.y + m_laneHeight) {
      auto spr = getStartSprite(visibleIndex);
      spr->setVisible(true);
      spr->setPosition(ccp(centerX, bottomY));
      if (spr->getContentSize().width > 0) {
        float scale = (noteWidth * 1.5f) / spr->getContentSize().width;
        spr->setScaleX(scale);
        spr->setScaleY(scale);
        spr->setZOrder(2); // Front
      }
    }
    if (topY >= laneOrigin.y && topY <= laneOrigin.y + m_laneHeight) {
      auto spr = getEndSprite(visibleIndex);
      spr->setVisible(true);
      spr->setPosition(ccp(centerX, topY));
      if (spr->getContentSize().width > 0) {
        float scale = (noteWidth * 1.5f) / spr->getContentSize().width;
        spr->setScaleX(scale);
        spr->setScaleY(scale);
        spr->setZOrder(2); // Front
      }
    }

    visibleIndex++;
  }

  // Draw Hit Error Bar
  float hitBarY = laneOrigin.y + m_hitBarYOffset;
  float hitBarWidth = m_laneWidth * 0.8f;
  float hitBarCenterX = laneOrigin.x + m_laneWidth / 2.0f;

  // Background bar
  m_judgmentNode->drawSegment(ccp(hitBarCenterX - hitBarWidth / 2.0f, hitBarY),
                              ccp(hitBarCenterX + hitBarWidth / 2.0f, hitBarY),
                              1.5f, ccc4f(0.5f, 0.5f, 0.5f, 0.5f));
  // Center tick
  m_judgmentNode->drawSegment(ccp(hitBarCenterX, hitBarY - 4.0f),
                              ccp(hitBarCenterX, hitBarY + 4.0f), 1.0f,
                              ccc4f(1.0f, 1.0f, 1.0f, 0.8f));

  // Draw and update hit error ticks
  float maxMs = m_hitWindowFps > 0.0f ? (12000.0f / m_hitWindowFps) : 50.0f;

  for (auto it = m_hitErrors.begin(); it != m_hitErrors.end();) {
    it->time -= dt;
    if (it->time <= 0.0f) {
      it = m_hitErrors.erase(it);
    } else {
      float xOffset = (it->errorMs / maxMs) * (hitBarWidth / 2.0f);

      // clamp xOffset
      if (xOffset > hitBarWidth / 2.0f)
        xOffset = hitBarWidth / 2.0f;
      if (xOffset < -hitBarWidth / 2.0f)
        xOffset = -hitBarWidth / 2.0f;

      float tickX = hitBarCenterX + xOffset;

      ccColor4F color = it->color;
      color.a = std::min(1.0f, it->time * 2.0f); // fade out faster at the end

      // Fix Premultiplied Alpha for CCDrawNode
      color.r *= color.a;
      color.g *= color.a;
      color.b *= color.a;

      m_judgmentNode->drawSegment(ccp(tickX, hitBarY - 5.0f),
                                  ccp(tickX, hitBarY + 5.0f), 1.5f, color);

      ++it;
    }
  }
}

void VSRGOverlay::onKeyDown(cocos2d::enumKeyCodes key) {
  // Re-read keybinds to ensure they are always up-to-date
  m_keyLane1 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-1");
  m_keyLane2 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-2");
  m_keyLane3 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-3");
  m_keyLane4 =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-lane-4");
  m_keySpeedUp =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-speed-up");
  m_keySpeedDown = Mod::get()->getSettingValue<std::vector<geode::Keybind>>(
      "key-speed-down");
  m_keyOffsetUp =
      Mod::get()->getSettingValue<std::vector<geode::Keybind>>("key-offset-up");
  m_keyOffsetDown = Mod::get()->getSettingValue<std::vector<geode::Keybind>>(
      "key-offset-down");

  // DEBUG: Log incoming key and all loaded binds
  log::info("[VSRG keyDown] pressed key={}", (int)key);
  log::info("[VSRG keyDown] lane1 binds count={}", m_keyLane1.size());
  for (size_t i = 0; i < m_keyLane1.size(); i++) {
    log::info("[VSRG keyDown]   lane1[{}].key={}", i, (int)m_keyLane1[i].key);
  }
  log::info("[VSRG keyDown] speedUp binds count={}", m_keySpeedUp.size());
  for (size_t i = 0; i < m_keySpeedUp.size(); i++) {
    log::info("[VSRG keyDown]   speedUp[{}].key={}", i,
              (int)m_keySpeedUp[i].key);
  }
  log::info("[VSRG keyDown] offsetUp binds count={}", m_keyOffsetUp.size());
  for (size_t i = 0; i < m_keyOffsetUp.size(); i++) {
    log::info("[VSRG keyDown]   offsetUp[{}].key={}", i,
              (int)m_keyOffsetUp[i].key);
  }

  auto isKey = [](cocos2d::enumKeyCodes k,
                  const std::vector<geode::Keybind> &binds) {
    for (auto &b : binds)
      if (b.key == k)
        return true;
    return false;
  };

  bool shiftHeld = CCDirector::sharedDirector()
                       ->getKeyboardDispatcher()
                       ->getShiftKeyPressed();

  if (isKey(key, m_keyLane1)) {
    m_keysHeld[0] = true;
    log::info("[VSRG] Lane 1 MATCHED");
  }
  if (isKey(key, m_keyLane2)) {
    m_keysHeld[1] = true;
    log::info("[VSRG] Lane 2 MATCHED");
  }
  if (isKey(key, m_keyLane3)) {
    m_keysHeld[2] = true;
    log::info("[VSRG] Lane 3 MATCHED");
  }
  if (isKey(key, m_keyLane4)) {
    m_keysHeld[3] = true;
    log::info("[VSRG] Lane 4 MATCHED");
  }

  // speed logic
  if (isKey(key, m_keySpeedUp)) {
    m_scrollSpeed += shiftHeld ? 0.01f : 0.1f;
    Mod::get()->setSettingValue("scroll-speed", (double)m_scrollSpeed);
    log::info("[VSRG] Speed UP -> {}", m_scrollSpeed);
  }
  if (isKey(key, m_keySpeedDown)) {
    m_scrollSpeed -= shiftHeld ? 0.01f : 0.1f;
    if (m_scrollSpeed < 0.01f)
      m_scrollSpeed = 0.01f;
    Mod::get()->setSettingValue("scroll-speed", (double)m_scrollSpeed);
    log::info("[VSRG] Speed DOWN -> {}", m_scrollSpeed);
  }

  // offset logic
  if (isKey(key, m_keyOffsetUp)) {
    m_userOffset += shiftHeld ? 1.0f : 20.0f;
    resetNotesState();
    saveState(); // saveState() already handles per-map offset
    log::info("[VSRG] Offset UP -> {}", m_userOffset);
  }
  if (isKey(key, m_keyOffsetDown)) {
    m_userOffset -= shiftHeld ? 1.0f : 20.0f;
    resetNotesState();
    saveState(); // saveState() already handles per-map offset
    log::info("[VSRG] Offset DOWN -> {}", m_userOffset);
  }

  // auto mode toggle
  if (isKey(key, m_keyAutoMode)) {
    m_autoMode = !m_autoMode;
    Mod::get()->setSavedValue("auto-mode", m_autoMode);
    log::info("[VSRG] Auto Mode -> {}", m_autoMode);
  }
}

void VSRGOverlay::onKeyUp(cocos2d::enumKeyCodes key) {
  auto isKey = [](cocos2d::enumKeyCodes k,
                  const std::vector<geode::Keybind> &binds) {
    for (auto &b : binds)
      if (b.key == k)
        return true;
    return false;
  };

  if (isKey(key, m_keyLane1))
    m_keysHeld[0] = false;
  if (isKey(key, m_keyLane2))
    m_keysHeld[1] = false;
  if (isKey(key, m_keyLane3))
    m_keysHeld[2] = false;
  if (isKey(key, m_keyLane4))
    m_keysHeld[3] = false;
}

void VSRGOverlay::extractLnBodyColor() {
  std::string skinFolder =
      Mod::get()->getSettingValue<std::string>("skin-folder");
  std::filesystem::path skinPath =
      Mod::get()->getConfigDir() / "skins" / skinFolder / "lnbody.png";

  CCImage *img = new CCImage();
  bool loaded = false;

  if (std::filesystem::exists(skinPath)) {
    std::string path = skinPath.string();
    std::replace(path.begin(), path.end(), '\\', '/');
    loaded = img->initWithImageFile(path.c_str());
  }

  if (!loaded) {
    // Try bundled path
    std::string bundledPath =
        fmt::format("{}/skins/{}/lnbody.png", Mod::get()->getID(), skinFolder);
    loaded = img->initWithImageFile(bundledPath.c_str());
  }

  if (loaded && img->getData()) {
    int w = img->getWidth();
    int h = img->getHeight();
    int cx = w / 2;
    int cy = h / 2;

    unsigned char *data = img->getData();
    // RGBA format, 4 bytes per pixel
    int idx = (cy * w + cx) * 4;

    m_lnBodyColor.r = data[idx + 0] / 255.0f;
    m_lnBodyColor.g = data[idx + 1] / 255.0f;
    m_lnBodyColor.b = data[idx + 2] / 255.0f;
    m_lnBodyColor.a = data[idx + 3] / 255.0f;

    m_lnBodyColor.r *= m_lnBodyColor.a;
    m_lnBodyColor.g *= m_lnBodyColor.a;
    m_lnBodyColor.b *= m_lnBodyColor.a;

    log::info("Extracted lnbody color: R={} G={} B={} A={}", data[idx + 0],
              data[idx + 1], data[idx + 2], data[idx + 3]);
  } else {
    log::info("Could not load lnbody.png, using default purple color");
    m_lnBodyColor = {0.6f * 0.8f, 0.4f * 0.8f, 0.8f * 0.8f, 0.8f};
  }

  delete img;
}

void VSRGOverlay::resetNotesState() {
  double currentFrameTemp =
      m_currentSmoothedFrame + m_baseOffset + m_userOffset;
  float fpsBase = m_hitWindowFps > 0.0f ? m_hitWindowFps : 240.0f;
  float msPerFrame = 1000.0f / fpsBase;
  float purpleThresh = msPerFrame * 12.0f;
  float maxErrorFrames = (purpleThresh / 1000.0f) * 240.0f;

  m_currentCombo = 0;
  if (m_comboLabel)
    m_comboLabel->setString("0");
  if (m_errorMsLabel)
    m_errorMsLabel->setString("");
  m_hitErrors.clear();

  for (auto &note : m_notes) {
    if (currentFrameTemp - note.startFrame > maxErrorFrames) {
      // Already completely missed in the past
      note.hitStart = false;
      note.hitEnd = false;
      note.missedStart = true;
      note.missedEnd = true;
    } else {
      // In the future or currently within the hit window
      note.hitStart = false;
      note.hitEnd = false;
      note.missedStart = false;
      note.missedEnd = false;
    }
  }
}
