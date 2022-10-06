#include "GameObject.h"

void GameObject::initialize()
{
    
}

void GameObject::draw()
{
    //renderer->prepare(modelPath);
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
