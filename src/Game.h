#pragma once
class Game
{
public:
    Game();
    ~Game();
    void initialize();
    void draw();
    void update();
    bool getIsGameRunning();

private:
    bool _isGameRunning = true;
};

