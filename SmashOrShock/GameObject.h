#pragma once
#include <DirectXMath.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Component.h"

class GameObject
{
public:
    /// <summary>
    /// ���݂̍��W���擾
    /// </summary>
    /// <returns>���W</returns>
    DirectX::XMVECTOR getPosition();

    virtual void init() = 0;
    virtual void update() = 0;
    virtual void draw() const = 0;

protected:
    /// <summary>
    /// �R���|�[�l���g���擾
    /// </summary>
    /// <param name="name">�R���|�[�l���g�̖��O</param>
    /// <returns>�w�肵���R���|�[�l���g</returns>
    std::shared_ptr<Component> getComponent(std::string name);

    /// <summary>
    /// �R���|�[�l���g��ǉ�
    /// </summary>
    /// <param name="name">�R���|�[�l���g�̖��O</param>
    /// <param name="component">�R���|�[�l���g�̃|�C���^</param>
    void addComponent(std::string name, Component* component);

    //�ʒu
    DirectX::XMVECTOR _position = { 0.0f, 0.0f, 0.0f, 1.0f };
    //�R���|�[�l���g���X�g
    std::unordered_map<std::string, std::shared_ptr<Component>> _ComponentList;
};

