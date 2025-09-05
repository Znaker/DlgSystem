// All rights reserved by Tallboys

#pragma once

#include "CoreMinimal.h"
#include "Editor.h"
#include "EditorUtilityWidget.h"
#include "Modules/ModuleManager.h"
#include "DlgSystemEditor/DlgEditorUtilities.h"
#include "TempUtilDialogues.generated.h"


class UDlgDialogue;

USTRUCT(BlueprintType)
struct FDialogueAndNodes
{
	GENERATED_BODY()

	FDialogueAndNodes(): Dialogue(nullptr)
	{
	};
	FDialogueAndNodes(UDlgDialogue* InDialogue, const TArray<FString>& InNodeGUIDs):Dialogue(InDialogue), NodeGUIDs(InNodeGUIDs){}
	UPROPERTY(BlueprintReadWrite)
	UDlgDialogue* Dialogue;

	UPROPERTY(BlueprintReadWrite)
	TArray<FString> NodeGUIDs;
};

USTRUCT(BlueprintType)
struct FNodeWithText
{
	GENERATED_BODY()

	FNodeWithText(){}
	FNodeWithText(const FString& InNodeGUIDs, const FText& InText): NodeGUID(InNodeGUIDs), Text(InText){}

	UPROPERTY(BlueprintReadWrite)
	FString NodeGUID;

	UPROPERTY(BlueprintReadWrite)
	FText Text;
};

USTRUCT(BlueprintType)
struct FDialogueAndNodesWithText
{
	GENERATED_BODY()

	FDialogueAndNodesWithText(): Dialogue(nullptr)
	{
	};
	FDialogueAndNodesWithText(UDlgDialogue* InDialogue, const TArray<FNodeWithText>& InNodeGUIDs):Dialogue(InDialogue), NodesWithText(InNodeGUIDs){}
	UPROPERTY(BlueprintReadWrite)
	UDlgDialogue* Dialogue;

	UPROPERTY(BlueprintReadWrite)
	TArray<FNodeWithText> NodesWithText;
};

/**
 *
 */
UCLASS()
class DLGSYSTEMEDITOR_API UTempUtilDialogues : public UEditorUtilityWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	TArray<FDialogueAndNodes> IterateDialogues();

	UFUNCTION(BlueprintCallable)
	TArray<FDialogueAndNodesWithText> FindTextInDialogues(FText TextToSearch, bool bFindExact = false);

	UFUNCTION(BlueprintCallable)
	void ReplaceTextInFoundNodes(
		const TArray<FDialogueAndNodesWithText>& FoundData,
		const FText& TextToFind, const FText& ReplacementText
	);

	UFUNCTION(BlueprintCallable)
	FText ReplaceTextInSingleNode(UDlgDialogue* Dialogue, const FNodeWithText& NodeEntry, const FText& TextToFind, const FText& ReplacementText);

	UFUNCTION(BlueprintCallable)
	FText ReplaceTextToTableString(const UDlgDialogue* Dialogue, const FNodeWithText& NodeEntry, FName TableName, const FString& Key);

	UFUNCTION(BlueprintCallable)
	void JumpToNode(UDlgDialogue* Dialogue, FString NodeGuid);

	UPROPERTY(BlueprintReadWrite)
	TArray<FDialogueAndNodes> FoundedNodesInDialogues;

	TArray<FDialogueAndNodesWithText> Result;
};

UCLASS()
class DLGSYSTEMEDITOR_API UTextReplaceItem : public UUserWidget
{
	GENERATED_BODY()
public:
	};
