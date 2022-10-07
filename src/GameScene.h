#pragma once
#include "Scene.h"
#include "Field.h"
#include "Player.h"
#include "Enemy.h"

class GameScene :
    public Scene
{
public:
    GameScene();
    void update() override;
    void reset() override;
};

