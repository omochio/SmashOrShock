#pragma once
#include "Scene.h"
class GameScene :
    public Scene
{
public:
    void update() override;
    void reset() override;
};

