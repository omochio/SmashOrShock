#pragma once
#include "Scene.h"
class GameScene :
    public Scene
{
public:
    void initialize() override;
    tinygltf::Model* getModel(std::string modelPath);
private:
    tinygltf::Model* loadModel(std::string path);
    std::vector<std::string> modelPathList;
    std::unordered_map<std::string, std::unique_ptr<tinygltf::Model>> modelList;
};

