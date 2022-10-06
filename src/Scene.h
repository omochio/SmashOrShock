#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include "ThirdPartyHeaders/tiny_gltf.h"
#include "GameObject.h"

class Scene
{
public:
    Scene();
    void initialize();
    virtual void update() = 0;
    void draw();
    virtual void reset() = 0;

protected:
    tinygltf::Model* getModel(std::string modelPath);
    tinygltf::Model* loadModel(std::string path);
    std::unordered_map<std::string, std::unique_ptr<tinygltf::Model>> m_modelList;
    std::vector<std::string> m_modelPathList;
    std::unordered_map <std::string, std::unique_ptr<GameObject>> m_gameObjectList;
};

