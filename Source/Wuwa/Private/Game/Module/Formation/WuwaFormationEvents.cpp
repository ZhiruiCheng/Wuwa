#include "Game/Module/Formation/WuwaFormationEvents.h"

namespace WuwaFormationEvents
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Members, "Event.Formation.Members", "队员列表整体变了，参数 FWuwaFormationMembersEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Switched, "Event.Formation.Switched", "切人了，参数 FWuwaFormationSwitchedEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MemberHealth, "Event.Formation.MemberHealth", "队员血量变了，参数 FWuwaFormationMemberHealthEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MemberAttributes, "Event.Formation.MemberAttributes", "队员的等级、元素、能量变了，参数 FWuwaFormationMemberAttributesEvent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MemberBuffs, "Event.Formation.MemberBuffs", "队员身上带图标的 buff 变了，参数 FWuwaFormationMemberBuffsEvent");
}
