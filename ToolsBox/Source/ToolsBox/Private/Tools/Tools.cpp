#include "Tools/Tools.h"
#include "Internationalization/Internationalization.h"
#include "Templates/SharedPointer.h"
#include "Tools/AutoPrefix/AutoPrefix.h"

#include "Tools/BlankTemplateTool/BlankTemplateTool.h"
#include "Tools/IntelligentImportOfModelsAndMaterials/Import_MM.h"
#include "Tools/MaterialTttributeTransfer/MaterialTttributeTransfer.h"
#include "Tools/Right-ClickOperationTool/AAU.h"

#include "Tools/SpawnMaterial/SpawnMaterial.h"
#include "Tools/VariableCopier/VariableCopier.h"
#include "Tools/PhysicsPlacer/PhysicsPlacer.h"

#define LOCTEXT_NAMESPACE "Tools"

const TSet<FTool> Tools::Get_ToolsData()
{
	static TSet<FTool> InternalTools = {
		
		/*FTool(
			LOCTEXT("ToolTitle_BlankTemplate", "空白模版"),            //可翻译标题
			LOCTEXT("ToolDetail_BlankTemplate", "开发者可复制此空白模版编写新工具"), //描述
			TEXT("ToolsBox.Image_Anon_1K"),						  //工具图标
			TEXT("SBlankTemplateToolTab"),						  //工具停靠栏名称
			TEXT("ToolsBox.Icon_Anon"),                           //停靠栏Icon
			[]() { return SNew(SBlankTemplateTool);}
			),*/
		
		FTool(
			LOCTEXT("ToolTitle_Import_MM", "批量导入模型和材质"),
			LOCTEXT("ToolDetail", "批量导入模型和材质，同文件下的模型以及附属的纹理贴图会自动连接材质球并赋予模型"),
			TEXT("ToolsBox.Image_KitaIkuyo"),
			TEXT("SImport_MMTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"), 
			[]() { return SNew(SImport_MM);}
			),

		FTool(
			LOCTEXT("ToolTitle_ContextMenu", "右键菜单操作脚本"),
			LOCTEXT("ToolDetail_ContextMenu", "右键菜单操作脚本"),
			TEXT("ToolsBox.Image_YamadaRyo"),
			TEXT("SAAUTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"), 
			[]() { return SNew(SAAU);}
			),

		FTool(
			LOCTEXT("ToolTitle_VariableCopier", "蓝图变量批量复制"),
			LOCTEXT("ToolDetail_VariableCopier", "多选两个蓝图的变量（函数，宏等），复制/剪切后一次性粘贴到另一个蓝图"),
			TEXT("ToolsBox.Image_Tomori"),
			TEXT("SVariableCopierTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"),
			[]() { return SNew(SVariableCopier);}
			),
		
		FTool(
			LOCTEXT("ToolTitle_SpawnMaterial", "批量材质球生成"),
			LOCTEXT("ToolDetail_SpawnMaterial", "选择若干纹理生成材质球或材质实例并赋予模型"),
			TEXT("ToolsBox.Image_IjichiNijika"),
			TEXT("SSpawnMaterialTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"), 
			[]() { return SNew(SSpawnMaterial);}
			),
		
		FTool(
			LOCTEXT("ToolTitle_MaterialTransfer", "材质属性转移"),
			LOCTEXT("ToolDetail_MaterialTransfer", "选择若干材质球或材质实例并通过变量命名将引用转移到新的材质球或材质实例"),
			TEXT("ToolsBox.Image_GotohHitori"),
			TEXT("SMaterialAttributeTransferTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"), 
			[]() { return SNew(SMaterialTttributeTransfer);}
			),
		FTool(
			LOCTEXT("ToolTitle_AutoPrefix", "自动前缀"),
			LOCTEXT("ToolDetail_AutoPrefix", "常见新蓝图类时自动添加该类前缀"),
			TEXT("ToolsBox.Image_Mustumi"),
			TEXT("SAutoPrefixTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"), 
			[]() { return SNew(SAutoPrefix);}
			),
		
		FTool(
			LOCTEXT("ToolTitle_PhysicsPlacer", "物理摆放"),
			LOCTEXT("ToolDetail_PhysicsPlacer", "选中多个场景物体，启动物理自由掉落摆放，并支持保存/回溯摆位"),
			TEXT("ToolsBox.Image_AnonBigHead"),
			TEXT("SPhysicsPlacerTab"),
			TEXT("ToolsBox.Icon_Anon2"),
			TEXT("https://space.bilibili.com/391627131/"),
			[]() { return SNew(SPhysicsPlacer);}
			),
		

	};
	return InternalTools;
}


#undef LOCTEXT_NAMESPACE