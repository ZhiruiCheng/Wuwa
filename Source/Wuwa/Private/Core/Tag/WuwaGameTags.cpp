#include "Core/Tag/WuwaGameTags.h"
#include "GameplayTagsManager.h"

//声明类静态成员变量必须有这个
FWuwaGameTags FWuwaGameTags::WuwaGamePlayTags;

namespace WuwaFrontendGamePlayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Fronted_WidgetStack_Modal,"Fronted.WidgetStack.Modal");
	UE_DEFINE_GAMEPLAY_TAG(Fronted_WidgetStack_GameMenu,"Fronted.WidgetStack.GameMenu");
	UE_DEFINE_GAMEPLAY_TAG(Fronted_WidgetStack_GameHud,"Fronted.WidgetStack.GameHud");
	UE_DEFINE_GAMEPLAY_TAG(Fronted_WidgetStack_Frontend,"Fronted.WidgetStack.Frontend");
	
	
}


void FWuwaGameTags::InitializeGameTags()
{
	WuwaGamePlayTags.Input_Combat_Attack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Combat.Attack"), FString("通用攻击输入，由条件规则决定具体技能"));
	WuwaGamePlayTags.Input_Combat_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Combat.Dash"), FString("闪避输入（长按进入冲刺），由条件规则决定具体技能"));

	WuwaGamePlayTags.Skill_Type_NormalAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Skill.Type.NormalAttack"), FString("技能分类：普攻（原作 技能通用标识.普攻）"));
	WuwaGamePlayTags.Skill_Type_Dodge = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Skill.Type.Dodge"), FString("技能分类：闪避"));
	WuwaGamePlayTags.Skill_Type_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Skill.Type.Dash"), FString("技能分类：前冲 / 冲刺类（原作 技能通用标识.闪避.前冲）"));
	WuwaGamePlayTags.Skill_Role_Changli = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Skill.Role.Changli"), FString("技能分类：长离的技能"));
	WuwaGamePlayTags.Skill_Role_Jiabei = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Skill.Role.Jiabei"), FString("技能分类：嘉贝的技能"));
	
	WuwaGamePlayTags.Input_Route_Ability = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Route.Ability"),
		FString("能力系统输入句柄")
		);
	WuwaGamePlayTags.Input_Route_Movement = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Route.Movement"),
		FString("玩家移动输入句柄")
		);
	WuwaGamePlayTags.Input_Route_Camera = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Route.Camera"),
		FString("玩家相机输入句柄")
		);
	WuwaGamePlayTags.Player_Common_Movement_Move = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Common.Movement.Move"),
		FString("玩家移动")
		);
	WuwaGamePlayTags.Player_Common_Movement_WalkRun = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Common.Movement.WalkRun"),
		FString("请求切换走路/跑步，不表示当前步态")
		);
	WuwaGamePlayTags.State_Position_Ground = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Position.Ground"), FString("位置状态：地面（原作 行为状态.位置状态.地面）"));
	WuwaGamePlayTags.State_Position_Air = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Position.Air"), FString("位置状态：空中（原作 行为状态.位置状态.空中）"));
	WuwaGamePlayTags.State_Position_Climb = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Position.Climb"), FString("位置状态：攀爬（原作 行为状态.位置状态.攀爬）"));
	WuwaGamePlayTags.State_Position_Water = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Position.Water"), FString("位置状态：水中（原作 行为状态.位置状态.水中）"));
	WuwaGamePlayTags.State_Formation_OnField = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Formation.OnField"), FString("在前台（原作 行为状态.逻辑状态.前台）：AWuwaCharacter::ApplyOnFieldState 按 bOnField 加 / 去"));
	WuwaGamePlayTags.State_Formation_SwitchForbid = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Formation.SwitchForbid"), FString("不能切人（原作 大招 TsAnimNotifyStateBurst 不能切人）：当前角色有它时切人请求被拒绝"));
	WuwaGamePlayTags.State_Combat_Invincible = UGameplayTagsManager::Get().AddNativeGameplayTag(FName("State.Combat.Invincible"), FString("无敌（原作 功能.逻辑状态标识.无敌.通用无敌 / 大招无敌）：被子弹打中不受伤害、不受击"));
	WuwaGamePlayTags.Player_Common_Movement_Jump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Common.Movement.Jump"),
		FString("请求跳跃（原作 JumpPress），由移动输入配置交给 WuwaMoveInputAction_Jump")
		);
	
	WuwaGamePlayTags.Player_Common_Camera_Rotate = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Player.Common.Camera.Rotate"),
	FString("旋转玩家视角")
	);
	WuwaGamePlayTags.Player_Common_Camera_Zoom = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Player.Common.Camera.Zoom"),
	FString("调整玩家相机距离")
	);
	
	WuwaGamePlayTags.Player_UI_ShowMouse = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Player.UI.ShowMouse"),
	FString("Alt按下显示鼠标")
	);
	
	WuwaGamePlayTags.Player_UI_CloseUI = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Player.UI.CloseUI"),
	FString("ESC按下收起当前UI"));
	
	WuwaGamePlayTags.Input_Route_UI = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Input.Route.UI"),
	FString("UI Router")
	);
	
	WuwaGamePlayTags.Input_Route_Formation = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Input.Route.Formation"),
	FString("切人输入句柄（按键 1/2/3）")
	);
	
	WuwaGamePlayTags.Role_Changli = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Role.Changli"),
	FString("UI Router")
	);
	WuwaGamePlayTags.Role_Jiabei = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Role.Jiabei"),
	FString("UI Router")
	);

	WuwaGamePlayTags.Formation_SwitchtoOne = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Formation.SwitchtoOne"),
	FString("SwitchtoOne")
	);WuwaGamePlayTags.Formation_SwitchtoTwo = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Formation.SwitchtoTwo"),
	FString("SwitchtoTwo")
	);WuwaGamePlayTags.Formation_SwitchtoThree = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Formation.SwitchtoThree"),
	FString("SwitchtoThree")
	);
	/*// Element
	WuwaGamePlayTags.Element_Fusion = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Fusion"), FString("热熔"));
	WuwaGamePlayTags.Element_Glacio = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Glacio"), FString("冷凝"));
	WuwaGamePlayTags.Element_Electro = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Electro"), FString("导电"));
	WuwaGamePlayTags.Element_Aero = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Aero"), FString("气动"));
	WuwaGamePlayTags.Element_Spectro = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Spectro"), FString("衍射"));
	WuwaGamePlayTags.Element_Havoc = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Element.Havoc"), FString("湮灭"));*/
}
