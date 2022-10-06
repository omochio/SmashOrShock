#include "GameScene.h"

void 
GameScene::initialize()
{
    for (std::string i : modelPathList) {
        modelList.insert(std::make_pair(i, loadModel(i)));
    }
    draw();
}

tinygltf::Model* GameScene::getModel(std::string modelPath)
{
    return modelList[modelPath].get();
}

tinygltf::Model*
GameScene::loadModel(std::string path)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!warn.empty()) {
        throw std::runtime_error(warn.c_str());
    }

    if (!err.empty()) {
        throw std::runtime_error(err.c_str());
    }

    if (!ret) {
        throw std::runtime_error("Failed to parse glTF");
    }

    return &model;
}