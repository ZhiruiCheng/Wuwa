#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSequenceCameraSetupCommandlet.generated.h"

/**
 * 按原作技能特写镜头重建 LevelSequence（配方由 ArtSource/Sequence/prepare_sequence_camera.py 生成）：
 *   -run=WuwaSequenceCameraSetup -Recipe=ArtSource/Sequence/SC_Changli_Burst01.sequence.json [-Apply]
 * - 帧率 / 播放范围同原作（30 帧，帧号即时间）。
 * - Role：可占有绑定（ACharacter，Tag Role），运行时换成放技能的角色（C# ZRCSequenceCameraPlayerComponent）。
 * - 相机：CineCamera 生成物（Tag SequenceCamera），挂到 Role 的 CharacterMesh0 / Root 插槽，位置 / 旋转关键帧原样，
 *   相机组件的焦距（由原作 FOV 换算）/ 光圈 / 手动对焦距离轨道；另加一条镜头切换轨道，方便在编辑器里预览（运行时关掉镜头切换，由子系统自己切视角）。
 * 已有资产整份重建（先备份到 Saved/Backups/SequenceCameraSetup/<时间>/）。默认只读，-Apply 才写。
 */
UCLASS()
class UWuwaSequenceCameraSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaSequenceCameraSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
