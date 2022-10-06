#pragma once
#include "GameObject.h"

class Field :
    public GameObject
{
public:
    Field();
    ~Field();
    void update() override;
private:
    
};

