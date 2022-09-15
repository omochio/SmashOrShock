#pragma once
#include "Scene.h"
#include <memory>

class SceneManager
{
public:
    SceneManager();
    ~SceneManager();
    void ChangeScene(Scene* scene);
    
private:
    std::unique_ptr<Scene> mCurrentScene;

};

