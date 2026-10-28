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

#pragma once
#if (defined(WWISE_OPENXR_SUPPORT) && WWISE_OPENXR_SUPPORT && \
     defined(WWISE_MOTION_SUPPORT) && WWISE_MOTION_SUPPORT)
#define WWISE_MOTION_OPENXR_SUPPORT
#include "CoreMinimal.h"
#include "IOpenXRExtensionPlugin.h"

class FWwiseMotionOpenXRExtension
    : public IOpenXRExtensionPlugin
{
public:
    void Init();

    /* IOpenXRExtensionPlugin implementation */
    virtual bool GetOptionalExtensions(TArray<const ANSICHAR*>& OutExtensions) override;
    virtual const void* OnCreateInstance(class IOpenXRHMDModule* InModule, const void* InNext) override;
    virtual void PostCreateInstance(XrInstance InInstance) override;
    virtual void PostCreateSession(XrSession InSession) override;
    virtual bool GetSuggestedBindings(XrPath InInteractionProfile, TArray<XrActionSuggestedBinding>& OutBindings) override;
    virtual void AttachActionSets(TSet<XrActionSet>& OutActionSets) override;
    virtual void GetActiveActionSetsForSync(TArray<XrActiveActionSet>& OutActiveSets) override;
    virtual bool GetInteractionProfile(XrInstance InInstance, FString& OutKeyPrefix, XrPath& OutPath, bool& OutHasHaptics) override;

private:
    void CreateActionSet();
    void InitOpenXRFunctions();
    void DestroyActionSet();
    XrPath ControllerHandToPath(EControllerHand hand) const;

    bool bExtFBHapticsPcmAvailable = false;
    PFN_xrGetDeviceSampleRateFB xrGetDeviceSampleRateFB = nullptr;

    XrInstance Instance = XR_NULL_HANDLE;
    XrSession Session = XR_NULL_HANDLE;
    XrActionSet ActionSet = XR_NULL_HANDLE;
    XrAction Action = XR_NULL_HANDLE;

    XrPath XrPathLeftHand = XR_NULL_PATH;
    XrPath XrPathLeftHandHaptics = XR_NULL_PATH;
    XrPath XrPathRightHand = XR_NULL_PATH;
    XrPath XrPathRightHandHaptics = XR_NULL_PATH;
    XrPath XrPathBothHands[2] = { XR_NULL_PATH, XR_NULL_PATH };
};
#endif