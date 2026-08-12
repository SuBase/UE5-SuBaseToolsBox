#pragma once

#include "CoreMinimal.h"

#include "Materials/Material.h"


// 贴图通道键名 — 用作 LocalMatch 的 Key
namespace ChannelKey
{
	inline const FString BC  = TEXT("BC");
	inline const FString N   = TEXT("N");
	inline const FString EM  = TEXT("EM");
	inline const FString OP  = TEXT("OP");
	inline const FString ORM = TEXT("ORM");
	inline const FString AO  = TEXT("AO");
	inline const FString R   = TEXT("R");
	inline const FString M   = TEXT("M");
	inline const FString SP  = TEXT("SP");
}


// Switch 参数名 — 母材质 / MIC 中使用的 StaticSwitch 参数名
namespace SwitchParam
{
	inline const FName Use_BaseColor  = TEXT("Use_BaseColor");
	inline const FName Use_Normal     = TEXT("Use_Normal");
	inline const FName Use_Emissive   = TEXT("Use_Emissive");
	inline const FName Use_Opacity    = TEXT("Use_Opacity");
	inline const FName Use_ORM        = TEXT("Use_ORM");
	inline const FName Use_AO         = TEXT("Use_AO");
	inline const FName Use_Roughness  = TEXT("Use_Roughness");
	inline const FName Use_Metallic   = TEXT("Use_Metallic");
	inline const FName Use_Specular   = TEXT("Use_Specular");
	inline const FName Use_Anisotropy = TEXT("Use_Anisotropy");
}


// 贴图参数名 — MIC 中 SetTextureParameterValueEditorOnly 使用的默认参数名
namespace TextureParam
{
	inline const FName BaseColor         = TEXT("BaseColor");
	inline const FName Normal            = TEXT("Normal");
	inline const FName Emissive          = TEXT("Emissive");
	inline const FName Opacity           = TEXT("Opacity");
	inline const FName ORM               = TEXT("ORM");
	inline const FName AmbientOcclusion  = TEXT("AmbientOcclusion");
	inline const FName Roughness         = TEXT("Roughness");
	inline const FName Metallic          = TEXT("Metallic");
	inline const FName Specular          = TEXT("Specular");
	inline const FName Anisotropy        = TEXT("Anisotropy");
}


// 纹理文件名字关键词匹配 — 用于从文件名推断贴图通道类型
namespace TextureMatch
{
	inline const TArray<FString>& BC()
	{
		static const TArray<FString> Keywords = { TEXT("base"), TEXT("albedo"), TEXT("color"), TEXT("col"), TEXT("diffuse"), TEXT("diff") };
		return Keywords;
	}
	inline const TArray<FString>& N()
	{
		static const TArray<FString> Keywords = { TEXT("normal"), TEXT("_n") };
		return Keywords;
	}
	inline const TArray<FString>& EM()
	{
		static const TArray<FString> Keywords = { TEXT("emissive"), TEXT("_em") };
		return Keywords;
	}
	inline const TArray<FString>& OP()
	{
		static const TArray<FString> Keywords = { TEXT("opacity"), TEXT("alpha"), TEXT("mask") };
		return Keywords;
	}
	inline const TArray<FString>& ORM()
	{
		static const TArray<FString> Keywords = { TEXT("orm") };
		return Keywords;
	}
	inline const TArray<FString>& AO()
	{
		static const TArray<FString> Keywords = { TEXT("ao"), TEXT("occlusion") };
		return Keywords;
	}
	inline const TArray<FString>& Roughness()
	{
		static const TArray<FString> Keywords = { TEXT("rough") };
		return Keywords;
	}
	inline const TArray<FString>& Metallic()
	{
		static const TArray<FString> Keywords = { TEXT("metal") };
		return Keywords;
	}

	// 辅助：判断文件名是否命中某组关键词
	inline bool ContainsAny(const FString& LowerFilename, const TArray<FString>& Keywords)
	{
		for (const FString& KW : Keywords)
		{
			if (LowerFilename.Contains(KW)) return true;
		}
		return false;
	}

	// 专门处理 ORM 匹配：标准关键词 + OcclusionRoughnessMetallic 全名组合
	inline bool IsORMTexture(const FString& LowerFilename)
	{
		if (ContainsAny(LowerFilename, ORM())) return true;
		// 引擎常用全名：OcclusionRoughnessMetallic（三个词同时出现在同一文件名中）
		return LowerFilename.Contains(TEXT("occlusion"))
			&& LowerFilename.Contains(TEXT("roughness"))
			&& LowerFilename.Contains(TEXT("metallic"));
	}
}


// BaseColor 后缀模式 — 用于多材质组前缀提取
namespace BaseColorSuffixes
{
	inline const TArray<FString>& All()
	{
		static const TArray<FString> Suffixes = {
			TEXT("_basecolor"), TEXT("_albedo"), TEXT("_color"), TEXT("_col"), TEXT("_diffuse"), TEXT("_diff")
		};
		return Suffixes;
	}
}


// 引擎默认贴图资源路径 — OnCreateGenericMaterialClicked 使用
namespace EngineDefaults
{
	inline const TCHAR* NormalMapPath  = TEXT("/Engine/EngineMaterials/BaseFlattenNormalMap.BaseFlattenNormalMap");
	inline const TCHAR* BlackColorPath = TEXT("/Engine/EngineResources/Black.Black");
	inline const TCHAR* BlackLinearPath = TEXT("/UVEditor/Textures/UVEditorColorGrid_LinearColor.UVEditorColorGrid_LinearColor");
	inline const TCHAR* WhiteMaskPath = TEXT("/UVEditor/Textures/UVEditorColorGrid_Mask.UVEditorColorGrid_Mask");

	inline const TCHAR* MasterMatName  = TEXT("M_AdvancedBatchMaster_Final");
	inline const TCHAR* MasterInstName = TEXT("MI_AdvancedBatchMaster_Final");
}


// 贴图通道完整元数据 — 统一管理每个通道的所有属性
struct FChannelMeta
{
	FString         Key;                  // 通道键名 "BC","N",...
	FString         UIDisplayLabel;       // UI 显示标签 "基础颜色:","法线:",...
	FName           DefaultTextureParam;  // 默认贴图参数名 "BaseColor","Normal",...
	FName           SwitchParam;          // StaticSwitch 参数名
	bool            bSRGB;                // 是否 sRGB 颜色空间
	TEnumAsByte<TextureCompressionSettings> CompressionSettings; // TC_Default / TC_Normalmap / TC_Masks
	EMaterialSamplerType SamplerType;     // 采样器类型
	EMaterialProperty   MaterialProp;     // 材质属性引脚 MP_BaseColor / MP_Normal / ...
	int32            ORMChannelIndex;     // ORM 合并贴图中的通道索引（0=非ORM通道；1=R=AO, 2=G=Roughness, 3=B=Metallic）
	FString          ORMChannelLetter;    // ORM 合并贴图中的通道字母（"", "R", "G", "B"）
};

// 获取所有通道的元数据配置
inline const TArray<FChannelMeta>& GetAllChannelMeta()
{
	static const TArray<FChannelMeta> Meta = {
		// Key  UIDisplay       TexParam              Switch              sRGB   Compression    SamplerType              MaterialProp              ORMIdx  ORMLetter
		// 注意：顺序需与 UI 布局一致（SGridPanel 按行列索引排列）
		{ ChannelKey::BC, TEXT("基础颜色:"),  TextureParam::BaseColor,        SwitchParam::Use_BaseColor,  true,  TC_Default,     SAMPLERTYPE_Color,        MP_BaseColor,         0, TEXT("") },
		{ ChannelKey::N,  TEXT("法线:"),      TextureParam::Normal,           SwitchParam::Use_Normal,     false, TC_Normalmap,   SAMPLERTYPE_Normal,       MP_Normal,            0, TEXT("") },
		{ ChannelKey::ORM,TEXT("ORM:"),       TextureParam::ORM,              SwitchParam::Use_ORM,        false, TC_Masks,       SAMPLERTYPE_Masks,        MP_MAX,               0, TEXT("") },
		{ ChannelKey::EM, TEXT("自发光:"),    TextureParam::Emissive,         SwitchParam::Use_Emissive,   true,  TC_Default,     SAMPLERTYPE_Color,        MP_EmissiveColor,     0, TEXT("") },
		{ ChannelKey::OP, TEXT("透明度:"),    TextureParam::Opacity,          SwitchParam::Use_Opacity,    false, TC_Masks,       SAMPLERTYPE_Masks,        MP_OpacityMask,       0, TEXT("") },
		{ ChannelKey::AO, TEXT("环境光遮蔽:"),TextureParam::AmbientOcclusion, SwitchParam::Use_AO,         false, TC_Masks,       SAMPLERTYPE_Masks,        MP_AmbientOcclusion,  1, TEXT("R") },
		{ ChannelKey::R,  TEXT("粗糙度:"),    TextureParam::Roughness,        SwitchParam::Use_Roughness,  false, TC_Masks,       SAMPLERTYPE_Masks,        MP_Roughness,         2, TEXT("G") },
		{ ChannelKey::M,  TEXT("金属度:"),    TextureParam::Metallic,         SwitchParam::Use_Metallic,   false, TC_Masks,       SAMPLERTYPE_Masks,       MP_Metallic,          3, TEXT("B") },
		{ ChannelKey::SP, TEXT("高光度:"),    TextureParam::Specular,         SwitchParam::Use_Specular,   false, TC_Default,     SAMPLERTYPE_LinearColor, MP_Specular,          0, TEXT("") },
	};
	return Meta;
}

// 按 Key 查找单个通道的元数据
inline const FChannelMeta* FindChannelMeta(const FString& Key)
{
	for (const FChannelMeta& M : GetAllChannelMeta())
	{
		if (M.Key == Key) return &M;
	}
	return nullptr;
}
