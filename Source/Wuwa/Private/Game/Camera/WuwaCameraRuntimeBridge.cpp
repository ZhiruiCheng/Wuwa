#include "Game/Camera/WuwaCameraRuntimeBridge.h"

FWuwaCameraView UWuwaCameraRuntimeBridge::EvaluateCamera_Implementation(FWuwaCameraFrame, FWuwaCameraSettings) { return {}; }
FWuwaCameraSettings UWuwaCameraRuntimeBridge::ResolveSettings_Implementation(FWuwaCameraSettings BaseSettings) { return BaseSettings; }
int32 UWuwaCameraRuntimeBridge::PushCameraMode_Implementation(UObject*, FWuwaCameraSettings, int32) { return 0; }
bool UWuwaCameraRuntimeBridge::PopCameraMode_Implementation(int32) { return false; }
void UWuwaCameraRuntimeBridge::ResetCamera_Implementation(bool) {}
