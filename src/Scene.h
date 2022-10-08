#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include "GameObject.h"

class Scene
{
public:
    Scene();
    void initialize();
    virtual void update() = 0;
    void draw();
    void terminate();

protected:
    std::vector<std::unique_ptr<GameObject>> m_gameObjectList;
};

