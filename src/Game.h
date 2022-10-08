#pragma once
#include "GameScene.h"

class Game
{
public:
    Game();
    ~Game();
    void initialize();
    void draw();
    void update();
    void terminate();
    const bool getIsGameRunning();

private:
    bool m_isGameRunning;
    
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneList;
};

