#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "ThirdPartyHeaders/tiny_gltf.h"

class Scene
{
public:
    virtual void initialize() = 0;
    virtual void update() = 0;
    virtual void draw() = 0;
    virtual void reset() = 0;

protected:
    std::unordered_map<std::string, std::unique_ptr<tinygltf::Model>> modelResources;
};

