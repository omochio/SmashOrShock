#include "SceneManager.h"

SceneManager::SceneManager()
{
    
}

/// <summary>
///     �V�[���؂�ւ����s�� 
/// </summary>
/// <param name="scnene">�؂�ւ���V�[���N���X�̃|�C���^</param>
void
SceneManager::ChangeScene (Scene* scnene) 
{
    mCurrentScene.reset(scnene);
}