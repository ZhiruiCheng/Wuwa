#include "WuwaCombatEditorLibrary.h"

#include "Engine/DataTable.h"
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"

// WuwaCombatEditorModule.cpp
bool WuwaOpenBulletEditor(UDataTable* Table, FName RowName);

bool UWuwaCombatEditorLibrary::OpenBulletEditor(UDataTable* Table, FName RowName)
{
	return WuwaOpenBulletEditor(Table, RowName);
}

bool UWuwaCombatEditorLibrary::StartPIEInNewWindow(int32 Width, int32 Height, int32 X, int32 Y)
{
	if (!GEditor || GEditor->IsPlaySessionInProgress() || GEditor->IsPlaySessionRequestQueued()) return false;
	FRequestPlaySessionParams Params;
	Params.WorldType = EPlaySessionWorldType::PlayInEditor;
	// 不给 DestinationSlateViewport：PIE 开在新窗口里；窗口设置用一份临时副本，不改用户的编辑器偏好
	ULevelEditorPlaySettings* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
	Settings->NewWindowWidth = Width;
	Settings->NewWindowHeight = Height;
	Settings->NewWindowPosition = FIntPoint(X, Y);
	Settings->CenterNewWindow = false;
	Params.EditorPlaySettings = Settings;
	GEditor->RequestPlaySession(Params);
	return true;
}
