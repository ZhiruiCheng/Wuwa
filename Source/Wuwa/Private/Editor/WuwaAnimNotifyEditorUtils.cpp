#include "Editor/WuwaAnimNotifyEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimSequenceBase.h"

void WuwaAnimNotifyEditor::RefreshTriggerOffsets(UAnimSequenceBase* Animation)
{
	if (!Animation) return;
	for (FAnimNotifyEvent& Notify : Animation->Notifies)
	{
		Notify.RefreshTriggerOffset(Animation->CalculateOffsetForNotify(Notify.GetTime()));
		if (Notify.GetDuration() > 0.f) Notify.RefreshEndTriggerOffset(Animation->CalculateOffsetForNotify(Notify.GetTime() + Notify.GetDuration()));
	}
}
#endif
