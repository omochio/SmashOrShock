#include "Scene.h"

Scene::Scene()
{

}

void
Scene::initialize()
{
    for (size_t i = 0; i < m_gameObjectList.size(); ++i)
    {
        m_gameObjectList[i]->initialize();
    }
}

void Scene::draw()
{
    for (size_t i = 0; i < m_gameObjectList.size(); ++i)
    {
        m_gameObjectList[i]->draw();
    }

}

