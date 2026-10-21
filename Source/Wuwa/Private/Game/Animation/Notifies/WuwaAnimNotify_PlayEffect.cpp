#include "Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"

FString UWuwaAnimNotify_PlayEffect::GetNotifyName_Implementation() const
{
	return Model ? FString::Printf(TEXT("Effect: %s"), *Model->GetName()) : TEXT("Play Effect");
}

void UWuwaAnimNotify_PlayEffect::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
	if (!MeshComp || !Model) return;

	// 首版仅在游戏世界创建系统，动画编辑器预览不发起播放。
	UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(MeshComp);
	if (!Effects) return;

	FWuwaEffectSpawnRequest Request;
	Request.Model = Model;
	const FTransform Offset(RotationOffset, LocationOffset, Scale);
	// 偏移按「不带缩放的挂点空间」算：2.0 模型的 FBX 包装骨骼缩放是 100，Root 插槽的世界缩放跟着是 100，
	// 直接乘会把原作 25cm 的偏移放到 25m 外、特效放大 100 倍。原作偏移以厘米、不随骨骼缩放为准。
	const FTransform Socket = MeshComp->GetSocketTransform(SocketName);
	if (bFollow)
	{
		// 挂上去后父级缩放仍会作用在相对变换上，所以先除掉（统一缩放下等价于按无缩放挂点放置）
		const FVector SocketScale = Socket.GetScale3D();
		const FVector Undo(FMath::IsNearlyZero(SocketScale.X) ? 1.f : 1.f / SocketScale.X,
			FMath::IsNearlyZero(SocketScale.Y) ? 1.f : 1.f / SocketScale.Y,
			FMath::IsNearlyZero(SocketScale.Z) ? 1.f : 1.f / SocketScale.Z);
		Request.AttachTo = MeshComp;
		Request.SocketName = SocketName;
		Request.Transform = Offset * FTransform(FQuat::Identity, FVector::ZeroVector, Undo);
	}
	else
	{
		Request.Transform = Offset * FTransform(Socket.GetRotation(), Socket.GetLocation());
	}

	// 此通知用于短表现：自然播完后由系统回收，本通知不持有播放句柄。
	Effects->SpawnEffect(Request);
}
