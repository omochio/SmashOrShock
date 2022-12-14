#pragma once
#include <DirectXMath.h>
#include <string>
#include <memory>
#include "Renderer.h"

class GameObject
{
public:
    void initialize();
    virtual void update() = 0;
    void draw();
    void terminate();

    /// <summary>
    /// 現在の座標を取得
    /// </summary>
    /// <returns>座標</returns>
    DirectX::XMVECTOR getPosition();

protected:
    Renderer* getRenderer();

    //Position
    DirectX::XMVECTOR m_position = { 0.0f, 0.0f, 0.0f, 1.0f };
    //Renderer
    std::unique_ptr<Renderer> m_renderer;
    
    UINT m_modelID;
};

