// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DlgSystem/Nodes/DlgNode.h"

#include "DlgNode_Start.generated.h"


/**
 * Possible entry point of the Dialogue.
 * Does not have text, the first satisfied child is picked if there is any.
 * Start nodes are evaluated from left to right.
 */
UCLASS(BlueprintType, ClassGroup = "Dialogue")
class DLGSYSTEM_API UDlgNode_Start : public UDlgNode
{
	GENERATED_BODY()

public:
	// Begin UObject Interface.

	/** @return a one line description of an object. */
	FString GetDesc() override;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Start", AssetRegistrySearchable, meta = (DisplayName = "Branch tag"))
	FName BranchTag = FName();
	UPROPERTY(EditAnywhere, Category = "Dialogue|Start", meta = (DeprecatedProperty, DisplayName = "Branch index"))
	int32 BranchNumber = -1;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Start")
	bool bExcludeFromDefaultStart = false;

	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;

	const FName& GetBranchTag() const { return BranchTag; }

	static FName GetMemberNameBranchNumber() { return GET_MEMBER_NAME_CHECKED(UDlgNode_Start, BranchNumber); }
	static FName GetMemberNameBranchTag() { return GET_MEMBER_NAME_CHECKED(UDlgNode_Start, BranchTag); }
	static FName GetMemberNameExcludeFromDefaultStart() { return GET_MEMBER_NAME_CHECKED(UDlgNode_Start, bExcludeFromDefaultStart); }

#if WITH_EDITOR
	FString GetNodeTypeString() const override { return TEXT("Start"); }
#endif
};
