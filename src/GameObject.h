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
    /// 現在の座標を取得
    /// </summary>
    /// <returns>座標</returns>
    DirectX::XMVECTOR getPosition();

    virtual void init() = 0;
    virtual void update() = 0;
    virtual void draw() const = 0;

protected:
    /// <summary>
    /// コンポーネントを取得
    /// </summary>
    /// <param name="name">コンポーネントの名前</param>
    /// <returns>指定したコンポーネント</returns>
    std::shared_ptr<Component> getComponent(std::string name);

    /// <summary>
    /// コンポーネントを追加
    /// </summary>
    /// <param name="name">コンポーネントの名前</param>
    /// <param name="component">コンポーネントのポインタ</param>
    void addComponent(std::string name, Component* component);

    //位置
    DirectX::XMVECTOR _position = { 0.0f, 0.0f, 0.0f, 1.0f };
    //コンポーネントリスト
    std::unordered_map<std::string, std::shared_ptr<Component>> _ComponentList;
};

