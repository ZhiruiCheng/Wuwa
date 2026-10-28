/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal

License Usage

Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#include "WwiseMotionOpenXRExtension.h"

#ifdef WWISE_MOTION_OPENXR_SUPPORT
#include "Platforms/AkPlatformInfo.h"

#include <AK/Plugin/AkMotionSinkOpenXRHelpers.h>

#include "OpenXRCore.h"
#include "IOpenXRHMDModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogWwiseMotionOpenXRExtension, Log, All);


void FWwiseMotionOpenXRExtension::Init()
{
    FCoreDelegates::OnPostEngineInit.AddLambda([]()
        {
            if (FModuleManager::Get().IsModuleLoaded(TEXT("OpenXRHMD")))
            {
#if PLATFORM_WINDOWS
                FString libMotionPath = FPaths::Combine(FAkPlatform::GetWwiseSoundEnginePluginDirectory(), TEXT(WWISE_MOTION_LIB_PATH));
#elif PLATFORM_ANDROID_ARM64
                FString libMotionPath = TEXT("libAkMotion.so");
#endif
                void* MotionDllHandle = FPlatformProcess::GetDllHandle(*libMotionPath);
                if (MotionDllHandle)
                {
                    typedef int(*_akmotionOpenXRInitFunc)(
                        AK::_akmotionXrResultToString in_pXrResultToString,
                        AK::_akmotionXrEnumerateInstanceExtensionProperties in_pXrEnumerateInstanceExtensionProperties,
                        AK::_akmotionXrStringToPath in_pXrStringToPath,
                        AK::_akmotionXrGetInstanceProcAddr in_pXrGetInstanceProcAddr,
                        AK::_akmotionXrApplyHapticFeedback in_pXrApplyHapticFeedback);

                    _akmotionOpenXRInitFunc _akmotion_pInitFn = reinterpret_cast<_akmotionOpenXRInitFunc>(FPlatformProcess::GetDllExport(MotionDllHandle, TEXT("AkMotionInitializeOpenXRFunctions")));
                    if (_akmotion_pInitFn)
                    {
                        _akmotion_pInitFn(
                            (AK::_akmotionXrResultToString)xrResultToString
                            , (AK::_akmotionXrEnumerateInstanceExtensionProperties)xrEnumerateInstanceExtensionProperties
                            , (AK::_akmotionXrStringToPath)xrStringToPath
                            , (AK::_akmotionXrGetInstanceProcAddr)xrGetInstanceProcAddr
                            , (AK::_akmotionXrApplyHapticFeedback)xrApplyHapticFeedback
                        );
                    }
                    else
                    {
                        UE_LOG(LogWwiseMotionOpenXRExtension, Error, TEXT("AkMotionInitializeOpenXRFunctions definition missing in AkMotion lib"));
                    }
                }
            }
        });

    RegisterOpenXRExtensionModularFeature();
}

bool FWwiseMotionOpenXRExtension::GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions)
{
    OutExtensions.Add(XR_FB_HAPTIC_PCM_EXTENSION_NAME);

    return true;
}

const void* FWwiseMotionOpenXRExtension::OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext)
{
    bExtFBHapticsPcmAvailable = InModule->IsExtensionEnabled(XR_FB_HAPTIC_PCM_EXTENSION_NAME);
    if (!bExtFBHapticsPcmAvailable)
    {
        UE_LOG(LogWwiseMotionOpenXRExtension, Warning, TEXT("XR FB Haptic PCM Extension is not enabled."));
    }

    return InNext;
}

void FWwiseMotionOpenXRExtension::PostCreateInstance(XrInstance InInstance)
{
    Instance = InInstance;

    CreateActionSet();
    InitOpenXRFunctions();
}

void FWwiseMotionOpenXRExtension::InitOpenXRFunctions()
{
    if (bExtFBHapticsPcmAvailable)
    {
        if (XR_FAILED(xrGetInstanceProcAddr(
            Instance,
            "xrGetDeviceSampleRateFB",
            (PFN_xrVoidFunction*)(&xrGetDeviceSampleRateFB))))
        {
            UE_LOG(LogWwiseMotionOpenXRExtension, Fatal, TEXT("Failed to bind OpenXR entry xrGetDeviceSampleRateFB."));
        }
        else
        {
            UE_LOG(LogWwiseMotionOpenXRExtension, Display, TEXT("Bound OpenXR entry xrGetDeviceSampleRateFB."));
        }
    }
}

XrPath FWwiseMotionOpenXRExtension::ControllerHandToPath(EControllerHand Hand) const
{
    int HandIndex = -1;
    switch (Hand)
    {
    case EControllerHand::Left:
        HandIndex = 0;
        break;
    case EControllerHand::Right:
        HandIndex = 1;
        break;
    default:
        UE_LOG(LogWwiseMotionOpenXRExtension, Error, TEXT("No action defined for %s."), *UEnum::GetValueAsString(Hand));
        return XR_NULL_PATH;
    }

    return XrPathBothHands[HandIndex];
}

void FWwiseMotionOpenXRExtension::CreateActionSet()
{
    UE_LOG(LogWwiseMotionOpenXRExtension, Display, TEXT("FWwiseMotionOpenXRExtension::CreateActionSet"));

    XrActionSetCreateInfo ActionSetCreateInfo = {
        .type = XR_TYPE_ACTION_SET_CREATE_INFO,
        .next = nullptr,
        .actionSetName = "pcm-haptics-action-set",
        .localizedActionSetName = "PCMHapticsActionSet",
    };

    XR_ENSURE(xrCreateActionSet(Instance, &ActionSetCreateInfo, &ActionSet));
    XR_ENSURE(xrStringToPath(Instance, "/user/hand/left", &XrPathLeftHand));
    XR_ENSURE(xrStringToPath(Instance, "/user/hand/left/output/haptic", &XrPathLeftHandHaptics));
    XR_ENSURE(xrStringToPath(Instance, "/user/hand/right", &XrPathRightHand));
    XR_ENSURE(xrStringToPath(Instance, "/user/hand/right/output/haptic", &XrPathRightHandHaptics));
    XrPathBothHands[0] = XrPathLeftHand;
    XrPathBothHands[1] = XrPathRightHand;

    XrActionCreateInfo hapticActionCreateInfo = {
        .type = XR_TYPE_ACTION_CREATE_INFO,
        .next = nullptr,
        .actionName = "wwise-motion-pcm-haptics-action",
        .actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT,
        .countSubactionPaths = sizeof(XrPathBothHands) / sizeof(XrPath),
        .subactionPaths = XrPathBothHands,
        .localizedActionName = "WwiseMotionPCMHapticsAction",
    };

    XR_ENSURE(xrCreateAction(ActionSet, &hapticActionCreateInfo, &Action));
}

void FWwiseMotionOpenXRExtension::PostCreateSession(XrSession InSession)
{
    Session = InSession;
}

bool FWwiseMotionOpenXRExtension::GetSuggestedBindings(XrPath InInteractionProfile, TArray<XrActionSuggestedBinding>& OutBindings)
{
    if (ActionSet == XR_NULL_HANDLE)
    {
        return false;
    }

    OutBindings.Add({ Action, XrPathLeftHandHaptics });
    OutBindings.Add({ Action, XrPathRightHandHaptics });

    return true;
}

void FWwiseMotionOpenXRExtension::AttachActionSets(TSet<XrActionSet>& OutActionSets)
{
    if (ActionSet != XR_NULL_HANDLE)
    {
        OutActionSets.Add(ActionSet);
    }

    if (FModuleManager::Get().IsModuleLoaded(TEXT("OpenXRHMD")))
    {
#if PLATFORM_WINDOWS
        FString libMotionPath = FPaths::Combine(FAkPlatform::GetWwiseSoundEnginePluginDirectory(), TEXT(WWISE_MOTION_LIB_PATH));
#elif PLATFORM_ANDROID_ARM64
        FString libMotionPath = TEXT("libAkMotion.so");
#endif
        void* MotionDllHandle = FPlatformProcess::GetDllHandle(*libMotionPath);
        if (MotionDllHandle)
        {
            typedef void(*_akmotionSetOpenXRHapticActionFunc)(XrInstance in_instance, XrSession in_session, XrAction in_hapticAction);
                _akmotionSetOpenXRHapticActionFunc _akmotion_pSetOpenXRHapticActionFn = reinterpret_cast<_akmotionSetOpenXRHapticActionFunc>(reinterpret_cast<void*>(
                    FPlatformProcess::GetDllExport(MotionDllHandle, TEXT("AkMotionSetOpenXRHapticAction"))
                    ));
                if (_akmotion_pSetOpenXRHapticActionFn)
                {
                    _akmotion_pSetOpenXRHapticActionFn(
                        Instance,
                        Session,
                        Action);
                }
                else
                {
                    UE_LOG(LogWwiseMotionOpenXRExtension, Error, TEXT("AkMotionSetOpenXRHapticAction definition missing in AkMotion lib"));
                }
        }
    }
}

void FWwiseMotionOpenXRExtension::GetActiveActionSetsForSync(TArray<XrActiveActionSet>& OutActiveSets)
{
    if (ActionSet != XR_NULL_HANDLE)
    {
        OutActiveSets.Add({ ActionSet, XR_NULL_PATH });
    }
}

bool FWwiseMotionOpenXRExtension::GetInteractionProfile(XrInstance InInstance, FString& OutKeyPrefix, XrPath& OutPath, bool& OutHasHaptics)
{
    if (ActionSet != XR_NULL_HANDLE)
    {
        DestroyActionSet();
    }

    CreateActionSet();

    return true;
}

void FWwiseMotionOpenXRExtension::DestroyActionSet()
{
    if (ActionSet != XR_NULL_HANDLE)
    {
        xrDestroyActionSet(ActionSet);
        Action = XR_NULL_HANDLE;
        ActionSet = XR_NULL_HANDLE;
    }
}
#endif // WWISE_MOTION_OPENXR_SUPPORT
