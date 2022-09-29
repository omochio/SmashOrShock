#include "GameObject.h"

DirectX::XMVECTOR GameObject::getPosition()
{
    return _position;
}

void GameObject::addComponent(std::string name, Component* component)
{
    _ComponentList.insert(std::make_pair(name, component));
}

std::shared_ptr<Component> GameObject::getComponent(std::string name)
{
    return _ComponentList.at(name);
}
