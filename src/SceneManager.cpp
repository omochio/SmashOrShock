#include "SceneManager.h"

SceneManager::SceneManager()
{
    
}

/// <summary>
///     シーン切り替えを行う 
/// </summary>
/// <param name="scnene">切り替え先シーンクラスのポインタ</param>
void
SceneManager::ChangeScene (Scene* scnene) 
{
    mCurrentScene.reset(scnene);
}