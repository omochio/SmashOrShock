#pragma once
#include "GameObject.h"

class Field :
    public GameObject
{
public:
    Field();
    ~Field();
    void init();
    void update();
    void draw();
private:
    
};

