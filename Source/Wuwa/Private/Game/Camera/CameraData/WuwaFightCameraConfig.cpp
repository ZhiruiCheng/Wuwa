#include "Game/Camera/CameraData/WuwaFightCameraConfig.h"

UWuwaFightCameraConfig::UWuwaFightCameraConfig()
{
	// 战斗镜头自己的默认配置
	Base.Add(EWuwaFightCameraDefault::ArmLength, 380.f);
	Base.Add(EWuwaFightCameraDefault::MinArmLength, 75.f);
	Base.Add(EWuwaFightCameraDefault::MaxArmLength, 600.f);
	Base.Add(EWuwaFightCameraDefault::Fov, 75.f);
	Base.Add(EWuwaFightCameraDefault::CameraOffsetX, 0.f);
	Base.Add(EWuwaFightCameraDefault::CameraOffsetY, 0.f);
	Base.Add(EWuwaFightCameraDefault::CameraOffsetZ, 0.f);
	Base.Add(EWuwaFightCameraDefault::CollisionProbeSize, 9.5f);
	Base.Add(EWuwaFightCameraDefault::PitchLimitMin, -90.f);
	Base.Add(EWuwaFightCameraDefault::PitchLimitMax, 90.f);
	Base.Add(EWuwaFightCameraDefault::ArmOffsetX, 0.f);
	Base.Add(EWuwaFightCameraDefault::ArmOffsetY, 0.f);
	Base.Add(EWuwaFightCameraDefault::ArmOffsetZ, 0.f);

	// 输入控制器的默认配置
	Input.Add(EWuwaFightCameraInput::ZoomSpeed, 10.f);
	Input.Add(EWuwaFightCameraInput::SmoothFactorMin, 0.6f);
	Input.Add(EWuwaFightCameraInput::SmoothFactorMax, 0.4f);
	Input.Add(EWuwaFightCameraInput::SmoothFactorRange, 100.f);
	Input.Add(EWuwaFightCameraInput::InputSpeedMin, 0.01f);
	Input.Add(EWuwaFightCameraInput::InputSpeedMax, 12.f);
	Input.Add(EWuwaFightCameraInput::SensitivityYawMin, 20.f);
	Input.Add(EWuwaFightCameraInput::SensitivityYawMax, 100.f);
	Input.Add(EWuwaFightCameraInput::SensitivityYawRange, 100.f);
	Input.Add(EWuwaFightCameraInput::SensitivityPitchMin, 15.f);
	Input.Add(EWuwaFightCameraInput::SensitivityPitchMax, 75.f);
	Input.Add(EWuwaFightCameraInput::SensitivityPitchRange, 100.f);

	// 缓冲系数和两个灵敏度都按 幂函数-凸函数 N=1.5 随输入大小变化
	const FWuwaCameraCurve PowerConvex{EWuwaCameraCurveType::PowerConvex, 1.5f};
	InputCurves.Add(EWuwaFightCameraInput::SmoothFactorRange, PowerConvex);
	InputCurves.Add(EWuwaFightCameraInput::SensitivityYawRange, PowerConvex);
	InputCurves.Add(EWuwaFightCameraInput::SensitivityPitchRange, PowerConvex);

	// 镜头修改结束后，各项按速度回去时的速度
	ModifyCamera.Add(EWuwaFightCameraModify::ArmLengthLagSpeed, 8.f);
	ModifyCamera.Add(EWuwaFightCameraModify::CameraOffsetLagSpeed, 8.f);
	ModifyCamera.Add(EWuwaFightCameraModify::ArmRotationLagSpeed, 8.f);
	ModifyCamera.Add(EWuwaFightCameraModify::FovLagSpeed, 8.f);
}
