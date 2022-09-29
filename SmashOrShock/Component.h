#pragma once

class Component
{
public:
    virtual void Update() = 0;
    virtual void Draw() const = 0;

private:
};

