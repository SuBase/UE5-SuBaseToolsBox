#include "OpenToolsBox_Command.h"

#define LOCTEXT_NAMESPACE "OpenToolsBox_Command"

void FOpenToolsBox_Command::RegisterCommands()
{

	UI_COMMAND(ToolsBox_OpenToolsBox, "ToolsBox", "点击打开功能窗口", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE