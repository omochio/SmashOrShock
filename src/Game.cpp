#include "Game.h"

Game::Game() 
{
    m_isGameRunning = true;
    m_sceneList.insert(std::make_pair("GameScene", new GameScene));
}

Game::~Game()
{
}

void Game::initialize()
{
    m_sceneList["GameScene"]->initialize();
}

void Game::draw()
{
    m_sceneList["GameScene"]->draw();
}

void Game::update()
{
}

void Game::terminate()
{
    m_sceneList["GameScene"]->terminate();
}

const bool Game::getIsGameRunning()
{
    return m_isGameRunning;
}
