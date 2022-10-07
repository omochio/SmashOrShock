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

    /// <summary>
    /// Œ»İ‚ÌÀ•W‚ğæ“¾
    /// </summary>
    /// <returns>À•W</returns>
    DirectX::XMVECTOR getPosition();

protected:
    Renderer* getRenderer();

    //Position
    DirectX::XMVECTOR m_position = { 0.0f, 0.0f, 0.0f, 1.0f };
    //Renderer
    std::unique_ptr<Renderer> m_renderer;
    
    UINT modelID;
};

