#include "Player.h"

Player::Player()
{
    m_modelID = 1;
}

void Player::update()
{
    m_renderer->delta -= 0.1;
}
