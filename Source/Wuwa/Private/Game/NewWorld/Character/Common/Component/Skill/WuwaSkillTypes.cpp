#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"

namespace WuwaSkill
{
	int32 ParseSkillId(FName RowName)
	{
		const FString Text = RowName.ToString();
		if (Text.IsEmpty() || !Text.IsNumeric() || Text.Contains(TEXT(".")) || Text.StartsWith(TEXT("-"))) return 0;
		return FCString::Atoi(*Text);
	}
}
