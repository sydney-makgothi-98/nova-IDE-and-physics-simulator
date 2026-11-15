// VLabSystemAsset.h -- a design made in the Vibration Lab Suite desktop app (a .vlab file), stored as a project asset.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VLabSystemAsset.generated.h"

UCLASS(BlueprintType)
class VLABRUNTIME_API UVLabSystemAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The design, in the Vibration Lab text format. Paste the contents of a .vlab file here or use LoadFromFile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Lab", meta = (MultiLine = "true"))
	FString VlabText;

	/** Where the text came from (informational). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vibration Lab")
	FString SourceFile;

	/** Reads a .vlab file saved by the desktop app. Returns false if the file cannot be read. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Vibration Lab")
	bool LoadFromFile(const FString& FilePath);
};
