#include "VLabSystemAsset.h"
#include "Misc/FileHelper.h"

bool UVLabSystemAsset::LoadFromFile(const FString& FilePath)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		return false;
	}
	VlabText = Text;
	SourceFile = FilePath;
	MarkPackageDirty();
	return true;
}
