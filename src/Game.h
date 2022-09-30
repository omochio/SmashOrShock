#pragma once
class Game
{
public:
    Game() : m_isGameRunning(true) {}
    ~Game();
    void initialize();
    void draw();
    void update();
    const bool getIsGameRunning();

private:
    bool m_isGameRunning;
};

