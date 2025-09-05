// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DlgSystem/Nodes/DlgNode.h"

#include "DlgNode_End.generated.h"


/**
 * Node ending the Dialogue.
 * Does not have text, if it is entered the Dialogue is over.
 * Events and enter conditions are taken into account.
 */
USTRUCT(BlueprintType)
struct DLGSYSTEM_API FDialogueTransition //todo: merget with FDlgInfo
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	bool MoveToNewDialogue = false;
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	UDlgDialogue* DialogueToMove = nullptr;
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta=(GetOptions="TallGameplayStatics.GetAllDialogueBranchNames"))
	FName DialogueBranchName;
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	FGuid StartNodeGuid;
};

UCLASS(BlueprintType, ClassGroup = "Dialogue")
class DLGSYSTEM_API UDlgNode_End : public UDlgNode
{
	GENERATED_BODY()

public:
	// Begin UObject Interface.

	/** @return a one line description of an object. */
	FString GetDesc() override;

	// Begin UDlgNode Interface.
	bool ReevaluateChildren(UDlgContext& Context, TSet<const UDlgNode*> AlreadyEvaluated) override { return false; }
	bool OptionSelected(int32 OptionIndex, bool bFromAll, UDlgContext& Context) override { return false; }
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	//float fTimeToNextSpeech = 0.f;

	//If true - all dialogue will be finish, without return to main dialogue or moving to another dialogue
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	bool bCustomReturnToMainOnEnd;

	//If true - all dialogue will be finish, without return to main dialogue or moving to another dialogue
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta = (EditCondition = "bCustomReturnToMainOnEnd", EditConditionHides))
	bool bReturnToMainOnEnd;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	FDialogueTransition DialogueTransition;

	static FName GetMemberNameDialogueTransition() { return GET_MEMBER_NAME_CHECKED(UDlgNode_End, DialogueTransition); }
	static FName GetMemberNameCustomReturnToMainOnEnd() { return GET_MEMBER_NAME_CHECKED(UDlgNode_End, bCustomReturnToMainOnEnd); }
	static FName GetMemberNameReturnToMainOnEnd() { return GET_MEMBER_NAME_CHECKED(UDlgNode_End, bReturnToMainOnEnd); }

#if WITH_EDITOR
	FString GetNodeTypeString() const override { return TEXT("End"); }
#endif
};
