// All rights reserved by Tallboys


#include "DlgSystemEditor/UtilityWidgets/TempUtilDialogues.h"
#include "DlgSystemEditor/DlgEditorAccess.h"
#include "DlgSystemEditor/DlgSystemEditorModule.h"
#include "DlgSystemEditor/DlgEditorUtilities.h"
#include "DlgSystem/DlgManager.h"
#include "DlgSystem/Nodes/DlgNode_Speech.h"
#include "DlgSystemEditor/Editor/Nodes/DialogueGraphNode_Base.h"

DEFINE_LOG_CATEGORY_STATIC(LogTallDialogueUtils, All, All)

TArray<FDialogueAndNodes> UTempUtilDialogues::IterateDialogues()
//TODO: - remake into test
{
	FoundedNodesInDialogues.Empty();
	UDlgManager::LoadAllDialoguesIntoMemory();
	TArray<UDlgDialogue*> Dialogues = UDlgManager::GetAllDialoguesFromMemory();

	struct FNodeInfo
	{
		FNodeInfo(FString InNodeGUID, TArray<FString> InConditions):NodeGuid(InNodeGUID), Conditions(InConditions){}

		FString NodeGuid;
		TArray<FString> Conditions;
	};
	for (UDlgDialogue* Dialogue : Dialogues)
	{
		TArray<FNodeInfo> CurrentDialogueFaultyNodes;
		for (auto Node : Dialogue->GetNodes())
		{
			TArray<FDlgCondition> EnterConditions = Node->GetNodeEnterConditions();
			if(EnterConditions.Num() > 0)
			{
				TArray<FString> FaultyConditions;
				for (int i = 0; i < EnterConditions.Num(); i++)
				{
					const FDlgCondition Condition = EnterConditions[i];
					if(Condition.ConditionType == EDlgConditionType::Custom)
					{
						if(Condition.CustomCondition == nullptr)
						{
							//add conditions indexes - probably redundant
							FaultyConditions.Add(FString::FromInt(i));
						};
					}
				}
				if(FaultyConditions.Num() > 0)
				{
					CurrentDialogueFaultyNodes.Add(FNodeInfo(Node->GetGUID().ToString(), FaultyConditions));
				}
			}
		}
		const bool NotPassed = CurrentDialogueFaultyNodes.Num() > 0;
		if(NotPassed)
		{
			TArray<FString> FaultyNodesGuids;
			UE_LOG(LogTallDialogueUtils, Display, TEXT("---Not valid Dialogue node Custom Condition found in Dialogue %s ---"), *Dialogue->GetDialogueName())
			for(auto NodeInfo : CurrentDialogueFaultyNodes)
			{
				FaultyNodesGuids.Add(NodeInfo.NodeGuid);
				FString Message = FString::Printf(TEXT("Node GUID: ")) + NodeInfo.NodeGuid;// + LINE_TERMINATOR;
				// for(auto Condition : NodeInfo.Conditions)
				// {
				// 	Message += FString::Printf(TEXT("Condition index: %s"))Condition
				// }
				UE_LOG(LogTallDialogueUtils, Display, TEXT("%s"), *Message)
			}
			UE_LOG(LogTallDialogueUtils, Display, TEXT("------"))
			FoundedNodesInDialogues.Add(FDialogueAndNodes(Dialogue, FaultyNodesGuids));
		}

	}
	return FoundedNodesInDialogues;
}

TArray<FDialogueAndNodesWithText> UTempUtilDialogues::FindTextInDialogues(const FText TextToSearch, bool bFindExact)
{
	UDlgManager::LoadAllDialoguesIntoMemory();
	TArray<UDlgDialogue*> Dialogues = UDlgManager::GetAllDialoguesFromMemory();

	Result.Empty();

	for (UDlgDialogue* Dialogue : Dialogues)
	{
		TArray<FNodeWithText> MatchingNodes;

		for (const UDlgNode* Node : Dialogue->GetNodes())
		{
			if (!Node)
			{
				continue;
			}

			FText NodeText = Node->GetNodeText();

			// Конвертация для поиска
			FString NodeTextStr = NodeText.ToString();
			FString SearchTextStr = TextToSearch.ToString();

			if (NodeTextStr.Contains(SearchTextStr, ESearchCase::IgnoreCase))
			{
				MatchingNodes.Add(FNodeWithText(Node->GetGUID().ToString(), NodeText));
			}
		}

		if (MatchingNodes.Num() > 0)
		{
			Result.Add(FDialogueAndNodesWithText(Dialogue, MatchingNodes));
		}
	}

	return Result;
}

void UTempUtilDialogues::ReplaceTextInFoundNodes(
	const TArray<FDialogueAndNodesWithText>& FoundData,
	const FText& TextToFind,
	const FText& ReplacementText
)
{
	const FString FindStr = TextToFind.ToString();
	const FString ReplaceStr = ReplacementText.ToString();

	for (const FDialogueAndNodesWithText& DialogueEntry : FoundData)
	{
		UDlgDialogue* Dialogue = DialogueEntry.Dialogue;
		if (!Dialogue)
		{
			continue;
		}

		bool bModified = false;

		for (const FNodeWithText& NodeEntry : DialogueEntry.NodesWithText)
		{
			const FString& NodeGUID = NodeEntry.NodeGUID;

			UDlgNode* TargetNode = nullptr;

			// Поиск нужной ноды по GUID
			for (UDlgNode* Node : Dialogue->GetNodes())
			{
				if (Node && Node->GetGUID() == FGuid(NodeGUID))
				{
					FString CurrentText = Node->GetNodeText().ToString();
					if (CurrentText.Contains(FindStr))
					{
						if (UDlgNode_Speech* SpeechNode = Cast<UDlgNode_Speech>(Node))
						{
							CurrentText = CurrentText.Replace(*FindStr, *ReplaceStr, ESearchCase::IgnoreCase);
							SpeechNode->SetNodeText(FText::FromString(CurrentText));
							bModified = true;
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("Node with GUID %s in Dialogue %s is not a SpeechNode."),
								*Node->GetGUID().ToString(),
								*Dialogue->GetPathName());
						}
					}
					break;
				}
			}
		}

		if (bModified)
		{
			Dialogue->MarkPackageDirty();
			Dialogue->Modify(true);
		}
	}
}

FText UTempUtilDialogues::ReplaceTextInSingleNode(UDlgDialogue* Dialogue, const FNodeWithText& NodeEntry, const FText& TextToFind, const FText& ReplacementText)
{
	if (!Dialogue)
	{
		return FText();
	}

	const FString FindStr = TextToFind.ToString();
	const FString ReplaceStr = ReplacementText.ToString();


	if(UDlgNode* Node = Dialogue->GetMutableNodeFromGUID(FGuid(NodeEntry.NodeGUID)))
	{
		FString CurrentText = Node->GetNodeText().ToString();
		if (CurrentText.Contains(FindStr))
		{
			if (UDlgNode_Speech* SpeechNode = Cast<UDlgNode_Speech>(Node))
			{
				CurrentText = CurrentText.Replace(*FindStr, *ReplaceStr, ESearchCase::IgnoreCase);
				SpeechNode->SetNodeText(FText::FromString(CurrentText));

				Dialogue->Modify(true);
				Dialogue->MarkPackageDirty();

				return SpeechNode->GetNodeText();
			}

			UE_LOG(LogTemp, Warning, TEXT("Node with GUID %s in Dialogue %s is not a SpeechNode."),
				*Node->GetGUID().ToString(),
				*Dialogue->GetPathName());
		}
	}

	return FText();
}

FText UTempUtilDialogues::ReplaceTextToTableString(const UDlgDialogue* Dialogue, const FNodeWithText& NodeEntry, const FName TableName, const FString& Key)
{
	if (!Dialogue)
	{
		return FText();
	}

	if(UDlgNode* Node = Dialogue->GetMutableNodeFromGUID(FGuid(NodeEntry.NodeGUID)))
	{
		if(UDlgNode_Speech* SpeechNode = Cast<UDlgNode_Speech>(Node))
		{
			SpeechNode->SetNodeText(FText::FromStringTable(TableName, Key));
		}
	}

	return FText();
}

void UTempUtilDialogues::JumpToNode(UDlgDialogue* Dialogue, FString NodeGuid)
{
	if(!Dialogue) return;

	FGuid TargetGuid;
	if(!FGuid::Parse(NodeGuid, TargetGuid))
	{
		UE_LOG(LogTallDialogueUtils, Display, TEXT("Couldn't conver GUID: %s"), *NodeGuid)
	};
	for(auto Node : Dialogue->GetNodes())
	{
		if(Node->GetGUID() == TargetGuid)
		{
			FDlgEditorUtilities::OpenEditorAndJumpToGraphNode(Node->GetGraphNode());
			break;
		}
	}
}
