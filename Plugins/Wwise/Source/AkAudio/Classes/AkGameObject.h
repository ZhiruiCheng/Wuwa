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

/*=============================================================================
	AkGameObject.h:
=============================================================================*/

#pragma once

#include "AkAudioDevice.h"
#include "AkDynamicSequence.h"
#include "AkGameplayTypes.h"
#include "Components/SceneComponent.h"
#include "AkGameObject.generated.h"


class UAkDialogueEvent;
class UAkDynamicSequence;

UCLASS(ClassGroup=Audiokinetic, BlueprintType, Blueprintable, hidecategories=(Transform,Rendering,Mobility,LOD,Component,Activation), AutoExpandCategories=AkComponent, meta=(BlueprintSpawnableComponent))
class AKAUDIO_API UAkGameObject: public USceneComponent
{
	GENERATED_BODY()

public:
	UAkGameObject(const class FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintGetter, Category = "Audiokinetic|AkEvent")
	float GetAttenuationScalingFactor() const;

	/** Modifies the attenuation computations of the emitter on this game object to simulate sounds with a larger or smaller area of effect. */
	UPROPERTY(EditAnywhere, BlueprintSetter = SetAttenuationScalingFactor, BlueprintGetter = GetAttenuationScalingFactor, Category = "AkEvent", meta = (ClampMin = 0.f))
	float AttenuationScalingFactor = 1.0f;

	/** Sets the attenuation scaling factor, which modifies the attenuation computations of the emitter on this game object to simulate sounds with a larger or smaller area of effect. */
	UFUNCTION(BlueprintSetter, Category = "Audiokinetic|AkEvent")
	void SetAttenuationScalingFactor(float InAttenuationScalingFactor);

	/** Associated Wwise Event to be posted on this game object */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AkEvent")
	TObjectPtr<UAkAudioEvent> AkAudioEvent = nullptr;

	/** Associated dynamic sequence, as generated through OpenDynamicSequence */
	UPROPERTY(Transient)
	TObjectPtr<UAkDynamicSequence> AkDynamicSequence = nullptr;

	/** Stop sound when owner is destroyed? */
	UPROPERTY(EditAnywhere, DisplayName="Stop When Owner is Destroyed", Category="AkGameObject")
	bool StopWhenOwnerDestroyed = false;

	/** The time in ms by which a playing sound object fades out after it is destroyed*/
	UPROPERTY(EditAnywhere, DisplayName="Stop Fadeout Time (ms)", Category="AkGameObject", meta=(EditCondition="StopWhenOwnerDestroyed", ClampMin=0, ClampMax=1000000))
	int StopFadeoutTime = 0;

	/**
	 * Posts this game object's AkAudioEvent to Wwise, using this as the game object source
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "2", AutoCreateRefTerm = "PostEventCallback,ExternalSources"))
	virtual int32 PostAssociatedAkEvent(
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AutoCreateRefTerm = "PostEventCallback,ExternalSources", Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
	virtual void PostAssociatedAkEventAsync(const UObject* WorldContextObject,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback,
		FLatentActionInfo LatentInfo,
		int32& PlayingID);

	/**
	 * Posts an event to Wwise, using this as the game object source
	 *
	 * @param AkEvent			The event to post
	 * @param CallbackMask		Mask of desired callbacks
	 * @param PostEventCallback	Blueprint Event to execute on callback
	 *
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "1", AutoCreateRefTerm = "PostEventCallback,ExternalSources"))
	virtual int32 PostAkEvent(
		class UAkAudioEvent * AkEvent,
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) int32 CallbackMask,
		const FOnAkPostEventCallback& PostEventCallback
	);

	virtual AkPlayingID PostAkEvent(UAkAudioEvent* AkEvent, AkUInt32 Flags = 0, AkCallbackFunc UserCallback = nullptr, void* UserCookie = nullptr);

	/**
	 * Post a Dialogue Event to the associated dynamic sequence.
	 * @param AkDialogueEvent		The Dialogue Event to post
	 * @param Arguments				The arguments to use on the Dialogue Event
	 * @param bOrderedPath		Whether the arguments are ordered or not. See UAkDialogueEvent::ResolveOrderedArguments.
	 * @param bPlayImmediately		Issues a Play command to the Dynamic Sequence.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject")
	virtual void PostAkDialogueEvent(
		UAkDialogueEvent* AkDialogueEvent,
		const TArray<UAkGroupValue*>& Arguments,
		bool bOrderedPath = false,
		bool bPlayImmediately = true
	);
	
	/**
	 * Posts an event to Wwise, using this as the game object source
	 *
	 * @param AkEvent		The event to post
	 * @param CallbackMask	Mask of desired callbacks
	 * @param PostEventCallback	Blueprint Event to execute on callback
	 *
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "3", AutoCreateRefTerm = "PostEventCallback,ExternalSources", Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
	virtual void PostAkEventAsync(const UObject* WorldContextObject,
			class UAkAudioEvent* AkEvent,
			int32& PlayingID,
			UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) int32 CallbackMask,
			const FOnAkPostEventCallback& PostEventCallback,
			FLatentActionInfo LatentInfo
	);

	/**
	 * Stops playback using this game object as the game object to stop
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkComponent")
	virtual void Stop();

	/**
	* Sets an RTPC value, using this game object as the game object source
	*
	* @param RTPC			The name of the RTPC to set
	* @param Value			The value of the RTPC
	* @param InterpolationTimeMs - Duration during which the RTPC is interpolated towards Value (in ms)
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "3"))
	virtual void SetRTPCValue(class UAkRtpc const* RTPCValue, float Value, int32 InterpolationTimeMs, FString RTPC) const;

	/**
	* Gets an RTPC value that was set on this game object as the game object source
	*
	* @param RTPC				The name of the RTPC to set
	* @param InputValueType		The input value type
	* @param Value				The value of the RTPC
	* @param OutputValueType	The output value type
	* @param PlayingID			The playing ID of the posted event (Set to zero to ignore)
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "RTPC"))
	void GetRTPCValue(class UAkRtpc const* RTPCValue, ERTPCValueType InputValueType, float& Value, ERTPCValueType& OutputValueType, FString RTPC, int32 PlayingID = 0) const;

	/**
	 * Posts a trigger to wwise, using this component as the game object source
	 *
	 * @param Trigger		The name of the trigger
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkComponent", meta = (AdvancedDisplay = "1"))
	void PostTrigger(class UAkTrigger const* TriggerValue);
	
	/**
	 * Sets a switch group in wwise, using this component as the game object source
	 *
	 * @param SwitchGroup	The name of the switch group
	 * @param SwitchState	The new state of the switch
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkComponent", meta = (AdvancedDisplay = "1"))
	virtual void SetSwitch(class UAkSwitchValue const* SwitchValue);
	
	/**
	 * Opens a dynamic sequence to Wwise, using this as the game object source.
	 *
	 * By default, the game object contains an associated dynamic sequence, and any new dialogue event will be added to this
	 * associated dynamic sequence. By asking for a new dynamic sequence instance, you are responsible for its lifetime.
	 *
	 * If the dynamic sequence was already created, the provided callback and sample accurate is ignored.
	 *
	 * @param CallbackMask		Mask of desired callbacks
	 * @param OpenSequenceCallback	Blueprint Event to execute on callback
	 * @param DefaultTransition	The Transition Parameters when Pausing, Playing, Stopping the Dynamic Sequence. 
	 * @param bSampleAccurate	Sample accurate Dynamic Sequence. Disallows playlist editing in specific cases.
	 * @param bNewInstance		Request a new instance, separated from the associated dynamic sequence.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject", meta = (AdvancedDisplay = "1", AutoCreateRefTerm = "OpenSequenceCallback"))
	virtual UAkDynamicSequence* OpenDynamicSequence(
		UPARAM(meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkCallbackType")) int32 CallbackMask,
		const FOnAkPostEventCallback& OpenSequenceCallback,
		const FAkDynamicSequenceTransition DefaultTransition = FAkDynamicSequenceTransition(),
		bool bSampleAccurate = false,
		bool bNewInstance = false
	);

	/**
	* Detaches the currently assigned dynamic sequence, resetting it.
*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Audiokinetic|AkGameObject")
	virtual UAkDynamicSequence* DetachDynamicSequence();

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if CPP
	bool VerifyEventName(const FString& InEventName) const;
	bool AllowAudioPlayback() const;
	virtual AkGameObjectID GetAkGameObjectID() const;
	virtual void UpdateObstructionAndOcclusion() {};
	bool HasActiveEvents() const;
#endif

	void SetRegisteredWithWwise(bool bInRegisteredWithWwise) { bIsRegisteredWithWwise = bInRegisteredWithWwise; }
	bool IsRegisteredWithWwise() const { return bIsRegisteredWithWwise; }
	void EventPosted() {bEventPosted = true;}
protected:
	// Whether an event was posted on the game object. Never reset to false. 
	bool bEventPosted;
	bool bIsRegisteredWithWwise = false;
	bool SetAttenuationScalingFactor();

#if WITH_EDITOR
	float PreviousAttenuationScalingFactor = 1.0f;
#endif

	UPROPERTY()
	bool bAttenuationScalingMigrated = false;
};
