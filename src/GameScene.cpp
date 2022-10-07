#include "GameScene.h"

GameScene::GameScene()
{

    m_gameObjectList.push_back(std::make_unique<Field>());
    m_gameObjectList.push_back(std::make_unique<Player>());
    m_gameObjectList.push_back(std::make_unique<Enemy>());
}

void GameScene::update()
{
}

void GameScene::reset()
{
}
