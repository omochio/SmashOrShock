#include "GameScene.h"

GameScene::GameScene()
{

    //m_gameObjectList.emplace_back(std::make_unique<Field>());
    m_gameObjectList.emplace_back(std::make_unique<Player>());
    //m_gameObjectList.emplace_back(std::make_unique<Enemy>());
}

void GameScene::update()
{
    for (size_t i = 0; i < m_gameObjectList.size(); ++i)
    {
        m_gameObjectList[i]->update();
    }

}

