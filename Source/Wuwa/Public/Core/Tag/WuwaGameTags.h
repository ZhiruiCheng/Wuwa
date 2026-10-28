#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

//两种不同的初始化方法,第一种比第二种更加靠前初始化
namespace WuwaFrontendGamePlayTags
{
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fronted_WidgetStack_Modal);
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fronted_WidgetStack_GameMenu);
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fronted_WidgetStack_GameHud);
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Fronted_WidgetStack_Frontend);

	
}

//当前使用的输入是输入指令触发后会根据InputAction查找对应的标签然后讲输入信息打包根据Handler标签进行转发对应职能的模块
struct WUWA_API FWuwaGameTags
{
	const static  FWuwaGameTags Get() {return WuwaGamePlayTags;}
	
	static void InitializeGameTags();
	
	//GA
	FGameplayTag Input_Combat_Attack;
	FGameplayTag Input_Combat_Dash;

	//Skill 分类（技能表 SkillTags；技能身份是技能 ID，不用 Tag）
	FGameplayTag Skill_Type_NormalAttack;
	FGameplayTag Skill_Type_Dodge;
	FGameplayTag Skill_Type_Dash;
	FGameplayTag Skill_Role_Changli;
	FGameplayTag Skill_Role_Jiabei;
	
	
	//State_Common
	FGameplayTag Player_Common_Movement_Move;
	FGameplayTag Player_Common_Movement_WalkRun;
	FGameplayTag Player_Common_Movement_Jump;

	//位置状态（原作 行为状态.位置状态.*）：运动状态提交时同步成角色身上的 loose tag，给 GE 移除条件、输入条件用
	FGameplayTag State_Position_Ground;
	FGameplayTag State_Position_Air;
	FGameplayTag State_Position_Climb;
	FGameplayTag State_Position_Water;
	FGameplayTag State_Formation_OnField;
	FGameplayTag State_Formation_SwitchForbid;
	FGameplayTag State_Combat_Invincible;
	FGameplayTag Player_Common_Camera_Rotate;
	FGameplayTag Player_Common_Camera_Zoom;
	
	
	//UI
	FGameplayTag Player_UI_ShowMouse;
	FGameplayTag Player_UI_CloseUI;
	
	
	//InputHandler
	FGameplayTag Input_Route_Ability;
	FGameplayTag Input_Route_Movement;
	FGameplayTag Input_Route_Camera;
	FGameplayTag Input_Route_UI;
	FGameplayTag Input_Route_Formation;
	
	//Character
	FGameplayTag Role_Changli;
	FGameplayTag Role_Jiabei;
	
	//SwitchCharacterSystem
	FGameplayTag Formation_SwitchtoOne;
	FGameplayTag Formation_SwitchtoTwo;
	FGameplayTag Formation_SwitchtoThree;
	
	/*//Element
	FGameplayTag Element_Fusion;   // 热熔
	FGameplayTag Element_Glacio;   // 冷凝
	FGameplayTag Element_Electro;  // 导电
	FGameplayTag Element_Aero;     // 气动
	FGameplayTag Element_Spectro;  // 衍射
	FGameplayTag Element_Havoc;    // 湮灭*/

	
public: 
	static FWuwaGameTags WuwaGamePlayTags;
};
