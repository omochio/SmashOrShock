#include "GameObject.h"

void GameObject::initialize()
{
    m_renderer.reset(new Renderer);
    m_renderer->prepare(m_modelID);
}

void GameObject::draw()
{
    m_renderer->render();
}

DirectX::XMVECTOR GameObject::getPosition()
{
    return m_position;
}

Renderer* GameObject::getRenderer()
{
    return m_renderer.get();
}

void GameObject::terminate()
{
    m_renderer->terminate();
}