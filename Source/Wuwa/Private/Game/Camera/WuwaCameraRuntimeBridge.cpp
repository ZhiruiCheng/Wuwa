#include "Game/Camera/WuwaCameraRuntimeBridge.h"

// 默认实现：C# 子类没有接管时什么都不做
void UWuwaCameraRuntimeBridge::InitCamera_Implementation(APlayerCameraManager*) {}
void UWuwaCameraRuntimeBridge::ClearCamera_Implementation() {}
void UWuwaCameraRuntimeBridge::OnPossess_Implementation(APawn*) {}
void UWuwaCameraRuntimeBridge::TickCamera_Implementation(float, FVector2D, float) {}
AActor* UWuwaCameraRuntimeBridge::GetFightCameraActor_Implementation() const { return nullptr; }
int32 UWuwaCameraRuntimeBridge::ApplyCameraModify_Implementation(const FWuwaCameraModify&, float, float, float, float, UAnimSequenceBase*, AActor*) { return -1; }
void UWuwaCameraRuntimeBridge::StopCameraModify_Implementation(UAnimSequenceBase*, int32) {}
bool UWuwaCameraRuntimeBridge::PlayCameraSequence_Implementation(const FWuwaSequenceCameraConfig&, ACharacter*, FName) { return false; }
bool UWuwaCameraRuntimeBridge::IsPlayingCameraSequence_Implementation() const { return false; }
AActor* UWuwaCameraRuntimeBridge::GetSequenceCameraActor_Implementation() const { return nullptr; }
