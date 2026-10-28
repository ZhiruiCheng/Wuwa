#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"

bool UWuwaInputCondition::Evaluate_Implementation(const FWuwaInputCommandContext& Context) const
{
	// 派生类必须提供实际条件，未实现时不允许通过。
	return false;
}                 
