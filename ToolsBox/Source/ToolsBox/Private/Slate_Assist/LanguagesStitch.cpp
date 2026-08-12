#include "Slate_Assist/LanguagesStitch.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Serialization/JsonWriter.h"


LanguagesStitch::LanguagesStitch()
{
	// 1. 先填充下拉选项（显示名），这里存的是一组独立的 TSharedPtr 对象
#define ARRAY_GEN(name, display) LanguageOptions.Add(MakeShared<FString>(display));
	LANGUAGE_LIST(ARRAY_GEN)
#undef ARRAY_GEN

	// 2. 读出 uplugin 里保存的语言「代码」（如 "ja"），转成「显示名」（如 "日本語"）
	FString StoredCode = GetLanguageFromUnplugin();
	if (StoredCode.IsEmpty())
	{
		StoredCode = TEXT("zh-Hans");
	}
	FString DisplayName = GetDisplayFromName(StoredCode);

	// 关键：必须让 SelectedLanguage 直接指向 LanguageOptions 里「同名」的那个指针对象，
	// 否则 SComboBox 用指针比较匹配 InitialSelectedItem 时会失败，选中集合为空、
	// 打开下拉框时回调收到无效的 NewSelected 而崩溃。
	for (const TSharedPtr<FString>& Opt : LanguageOptions)
	{
		if (Opt.IsValid() && *Opt == DisplayName)
		{
			SelectedLanguage = Opt;
			break;
		}
	}
	// 兜底：万一没匹配上（比如字段被改坏），默认选中第一项
	if (!SelectedLanguage.IsValid() && LanguageOptions.Num() > 0)
	{
		SelectedLanguage = LanguageOptions[0];
	}

	// 3. 按保存的语言加载对应翻译（locres），保证窗口一打开就是正确语言
	LoadPluginLocRes(StoredCode);
}

void LanguagesStitch::SwitchLanguage(const FString& Language)
{
	SetLanguageInUnplugin(GetNameFromDisplay(Language));
	//加载语言文件
	LoadPluginLocRes(GetNameFromDisplay(Language));
}

bool LanguagesStitch::SetLanguageInUnplugin(const FString& NewLanguage)
{
	FString JsonString;
	// 1. 先读出当前内容，确保不破坏文件其他结构
	if (!FFileHelper::LoadFileToString(JsonString, *(FPaths::ProjectPluginsDir() + TEXT("ToolsBox/ToolsBox.uplugin"))))
	{
		return false;
	}
 
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
 
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// 2. 更新或添加字段
		JsonObject->SetStringField(TEXT("Language"), NewLanguage);
 
		// 3. 序列化回字符串
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
		{
		// 4. 写回文件（注意：必须是完整的 .uplugin 路径，不能只写到插件目录）
		return FFileHelper::SaveStringToFile(OutputString, *(FPaths::ProjectPluginsDir() + TEXT("ToolsBox/ToolsBox.uplugin")));
		}
	}
 
	return false;
}

FString LanguagesStitch::GetLanguageFromUnplugin()
{
	FString JsonString;
	// 1. 读取文件到字符串
	if (!FFileHelper::LoadFileToString(JsonString, *(FPaths::ProjectPluginsDir() + TEXT("ToolsBox/ToolsBox.uplugin"))))
	{
		UE_LOG(LogTemp, Error, TEXT("无法读取文件: %s"),*FPaths::ProjectPluginsDir());
		return TEXT("");
	}
 
	// 2. 解析 JSON
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
 
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		// 3. 尝试读取 Language 字段
		FString OutLanguage;
		if (JsonObject->TryGetStringField(TEXT("Language"), OutLanguage))
		{
			return OutLanguage;
		}
	}
 
	return TEXT("");
}

void LanguagesStitch::LoadPluginLocRes(const FString& LanguageCode)
{
	// 1. 构建 .locres 的路径

	FString LocResPath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), 
		TEXT("ToolsBox/Resources/Languages")
	);


	LocResPath += FString::Printf(TEXT("/%s/ToolsBox.locres"), *LanguageCode);

	if (!FPaths::FileExists(LocResPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("LocRes file not found: %s"), *LocResPath);
		return;
	}
 
	// 2. 手动读取 LocRes 二进制数据并注入到引擎的内存翻译表
	// 这种方法会覆盖对应 Namespace 下的现有翻译，而不改变全局 Culture
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(LocResPath);
    
	UE_LOG(LogTemp, Log, TEXT("Manually loaded plugin localization: %s"), *LanguageCode);
}

FString LanguagesStitch::GetNameFromDisplay(const FString& InDisplay)
{
	// 使用静态 TMap 保证只初始化一次，消除 (Elimination) 重复计算开销
	static TMap<FString, FString> DisplayToNameMap;
 
	if (DisplayToNameMap.Num() == 0)
	{
		// 自动通过宏填充 Map
		#define MAP_GEN(name, display) \
		{ \
		FString InternalName = TEXT(#name); \
		DisplayToNameMap.Add(display, InternalName.Replace(TEXT("_"), TEXT("-"))); \
		}
        
		LANGUAGE_LIST(MAP_GEN)
		#undef MAP_GEN
	}
 
	// 根据 Display 查找 Name，如果没找到返回空字符串
	return DisplayToNameMap.FindRef(InDisplay);
}

FString LanguagesStitch::GetDisplayFromName(const FString& InName)
{
	// 反向映射：代码 -> 显示名（"ja" -> "日本語"），用于把 uplugin 里存的代码转成下拉框显示名
	static TMap<FString, FString> NameToDisplayMap;

	if (NameToDisplayMap.Num() == 0)
	{
		#define MAP_GEN2(name, display) \
		{ \
		FString InternalName = TEXT(#name); \
		NameToDisplayMap.Add(InternalName.Replace(TEXT("_"), TEXT("-")), display); \
		}

		LANGUAGE_LIST(MAP_GEN2)
		#undef MAP_GEN2
	}

	return NameToDisplayMap.FindRef(InName);
}


