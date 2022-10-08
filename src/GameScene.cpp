#include "GameScene.h"

GameScene::GameScene()
{

    //m_gameObjectList.emplace_back(std::make_unique<Field>());
    m_gameObjectList.emplace_back(std::make_unique<Player>());
    //m_gameObjectList.emplace_back(std::make_unique<Enemy>());
}

void GameScene::update()
{
    m_gameObjectList[0]->draw();
}

