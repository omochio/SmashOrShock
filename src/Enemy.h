#pragma once
#include "GameObject.h"
class Enemy :
    public GameObject
{
public:
    Enemy();
    void update() override;
};

