#include "Scene.h"

Scene::Scene()
{
    m_modelPathList.push_back("Resources/Field");
    m_modelPathList.push_back("Resources/Player");
    m_modelPathList.push_back("Resources/Enemy");
}

void
Scene::initialize()
{
    for (std::string i : m_modelPathList) 
    {
        m_modelList.insert(std::make_pair(i, loadModel(i)));
    }

    for (std::string i : m_modelPathList)
    {
        m_gameObjectList[i]->initialize();
    }
}

void Scene::draw()
{
    for (std::string i : m_modelPathList) 
    {
        m_gameObjectList[i]->draw();
    }

}

tinygltf::Model* Scene::getModel(std::string modelPath)
{
    return m_modelList[modelPath].get();
}

tinygltf::Model* Scene::loadModel(std::string path)
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