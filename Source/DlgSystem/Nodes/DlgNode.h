// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Build.h"
#include "UObject/Object.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"
#endif

#include "DlgSystem/DlgEdge.h"
#include "DlgSystem/DlgCondition.h"
#include "DlgSystem/DlgEvent.h"
#include "DlgSystem/DlgNodeData.h"
#include "DlgNode.generated.h"


class UDlgSystemSettings;
class UDlgContext;
class UDlgNode;
class USoundBase;
class USoundWave;
class UDialogueWave;
struct FDlgTextArgument;
class UDlgDialogue;


UENUM(BlueprintType)
enum class EDlgEntryRestriction : uint8
{
	// Node can be entered multiple times
	None				UMETA(DisplayName = "None"),

	// Node can only be entered once per context (same as WasNodeAlreadyVisited check in local memory)
	OncePerContext		UMETA(DisplayName = "Once per Context"),

	// Node can only be entered once globally (same as WasNodeAlreadyVisited check in global memory (Dialogue history))
	Once				UMETA(DisplayName = "Only Once")
};

USTRUCT(BlueprintType)
struct FNodeInterruptInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bInterruptible = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDlgDialogue* DialogueOnTryInterrupt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EPlayerAnswerIntend> InterruptExceptions;
};

USTRUCT(BlueprintType)
struct FDialogueItem
{
	GENERATED_USTRUCT_BODY()

	FDialogueItem(){}
	FDialogueItem(const FName InItemName, int32 InCount, UDlgDialogue* InDialogue, FGuid InTargetNodeID, int32 InMinMood)
		: ItemName(InItemName),
		Count(InCount),
		TargetDialogue(InDialogue),
		TargetNodeID(InTargetNodeID),
		MinMood(InMinMood)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemName = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Count = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UDlgDialogue* TargetDialogue = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid TargetNodeID = FGuid();
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinMood = 0;
};


/**
 *  Abstract base class for Dialogue nodes
 *  Depending on the implementation in the child class the dialogue node can contain one or more lines of one or more participants,
 *  or simply some logic to go on in the UDlgNode graph
 */
UCLASS(BlueprintType, Abstract, EditInlineNew, ClassGroup = "Dialogue")
class DLGSYSTEM_API UDlgNode : public UObject
{
	GENERATED_BODY()

public:
	//
	// Begin UObject Interface.
	//

	void Serialize(FArchive& Ar) override;
	FString GetDesc() override { return TEXT("INVALID DESCRIPTION"); }
	void PostLoad() override;
	void PostInitProperties() override;
	void PostDuplicate(bool bDuplicateForPIE) override;
	void PostEditImport() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

#if WITH_EDITOR
	/**
	 * Called when a property on this object has been modified externally
	 *
	 * @param PropertyChangedEvent the property that was modified
	 */
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * This alternate version of PostEditChange is called when properties inside structs are modified.  The property that was actually modified
	 * is located at the tail of the list.  The head of the list of the FStructProperty member variable that contains the property that was modified.
	 */
	void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

	//
	// Begin own function
	//

	// Used internally by the Dialogue editor:
	virtual FString GetNodeTypeString() const { return TEXT("INVALID"); }
#endif //WITH_EDITOR

	virtual void OnCreatedInEditor() {};

#if WITH_EDITOR
	void SetGraphNode(UEdGraphNode* InNode) { GraphNode = InNode; }
	void ClearGraphNode() { GraphNode = nullptr; }
	UEdGraphNode* GetGraphNode() const { return GraphNode; }
#endif

	/** Broadcasts whenever a property of this dialogue changes. */
	DECLARE_EVENT_TwoParams(UDlgNode, FDialogueNodePropertyChanged, const FPropertyChangedEvent& /* PropertyChangedEvent */, int32 /* EdgeIndexChanged */);
	FDialogueNodePropertyChanged OnDialogueNodePropertyChanged;

	virtual bool HandleNodeEnter(UDlgContext& Context, TSet<const UDlgNode*> NodesEnteredWithThisStep);
	virtual bool ReevaluateChildren(UDlgContext& Context, TSet<const UDlgNode*> AlreadyEvaluated);

	virtual bool CheckNodeEnterConditions(const UDlgContext& Context, TSet<const UDlgNode*> AlreadyVisitedNodes) const;
	bool HasAnySatisfiedChild(const UDlgContext& Context, TSet<const UDlgNode*> AlreadyVisitedNodes) const;

	// if bFromAll = true it uses all the options (even unsatisfied)
	// if bFromAll = false it only uses the satisfied options.
	virtual bool OptionSelected(int32 OptionIndex, bool bFromAll, UDlgContext& Context);

	//
	// Getters/Setters:
	//

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	FGuid GetGUID() const { return NodeGUID; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	bool HasGUID() const { return NodeGUID.IsValid(); }

	void RegenerateGUID()
	{
		NodeGUID = FGuid::NewGuid();
		Modify();
	}

	//
	// For the ParticipantName
	//

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual FName GetNodeParticipantName() const { return OwnerName; }

	virtual void SetNodeParticipantName(FName InName) { OwnerName = InName; }

	//
	// For the EnterConditions
	//

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual bool HasAnyEnterConditions() const { return GetNodeEnterConditions().Num() > 0 || EnterRestriction != EDlgEntryRestriction::None; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const TArray<FDlgCondition>& GetNodeEnterConditions() const { return EnterConditions; }

	virtual void SetNodeEnterConditions(const TArray<FDlgCondition>& InEnterConditions) { EnterConditions = InEnterConditions; }

	// Gets the mutable enter condition at location EnterConditionIndex.
	virtual FDlgCondition* GetMutableEnterConditionAt(int32 EnterConditionIndex)
	{
		check(EnterConditions.IsValidIndex(EnterConditionIndex));
		return &EnterConditions[EnterConditionIndex];
	}

	//
	// For the EnterEvents
	//

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual bool HasAnyEnterEvents() const { return GetNodeEnterEvents().Num() > 0; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const TArray<FDlgEvent>& GetNodeEnterEvents() const { return EnterEvents; }

	virtual void SetNodeEnterEvents(const TArray<FDlgEvent>& InEnterEvents) { EnterEvents = InEnterEvents; }


	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual bool HasNextSpeechTimer() const { return fTimeToNextSpeech > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual const float GetTimeToNextSpeech() const { return fTimeToNextSpeech; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual const FGuid GetNodeGUIDToReturn() const { return NodeToReturnGUID; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual const bool IsNodeCustomInterrupt() const { return bCustomInterrupt; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	FNodeInterruptInfo GetNodeInterruptInfo() const { return InterruptInfo; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const FDialogueItem GetGivingItemName() const { return GivingItem; }

	virtual void SetTimeToNextSpeech(const float Time) { fTimeToNextSpeech = Time; }

	//
	// For the NextSpeechTimer
	//

	//
	// For the Children
	//

	/// Gets this nodes children (edges) as a const/mutable array

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const TArray<FDlgEdge>& GetNodeChildren() const { return Children; }
	virtual void SetNodeChildren(const TArray<FDlgEdge>& InChildren) { Children = InChildren; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual int32 GetNumNodeChildren() const { return Children.Num(); }

	//virtual bool HasChildWithIntend(const EPlayerAnswerIntend CompareIntend) const;

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const FDlgEdge& GetNodeChildAt(int32 EdgeIndex) const { return Children[EdgeIndex]; }

	// Adds an Edge to the end of the Children Array.
	virtual void AddNodeChild(const FDlgEdge& InChild) { Children.Add(InChild); }

	// Removes the Edge at the specified EdgeIndex location.
	virtual void RemoveChildAt(int32 EdgeIndex)
	{
		check(Children.IsValidIndex(EdgeIndex));
		Children.RemoveAt(EdgeIndex);
	}

	// Removes all edges/children
	virtual void RemoveAllChildren() { Children.Empty(); }

	// Gets the mutable edge/child at location EdgeIndex.
	virtual FDlgEdge* GetSafeMutableNodeChildAt(int32 EdgeIndex)
	{
		check(Children.IsValidIndex(EdgeIndex));
		return &Children[EdgeIndex];
	}

	// Unsafe version, can be null
	virtual FDlgEdge* GetMutableNodeChildAt(int32 EdgeIndex)
	{
		return Children.IsValidIndex(EdgeIndex) ? &Children[EdgeIndex] : nullptr;
	}

	// Gets the mutable Edge that corresponds to the provided TargetIndex or nullptr if nothing was found.
	virtual FDlgEdge* GetMutableNodeChildForTargetIndex(int32 TargetIndex);

	// Gets all the edges (children) indices that DO NOT have a valid TargetIndex (is negative).
	const TArray<int32> GetNodeOpenChildren_DEPRECATED() const;

	// Gathers associated participants, they are only added to the array if they are not yet there
	virtual void GetAssociatedParticipants(TArray<FName>& OutArray) const;

	// Updates the value of the texts from the default values or the remappings (if any)
	virtual void UpdateTextsValuesFromDefaultsAndRemappings(
		const UDlgSystemSettings& Settings, bool bEdges, bool bUpdateGraphNode = true
	);

	// Updates the namespace and key of all the texts depending on the settings
	virtual void UpdateTextsNamespacesAndKeys(const UDlgSystemSettings& Settings, bool bEdges, bool bUpdateGraphNode = true);

	// Rebuilds ConstructedText
	virtual void RebuildTextArguments(bool bEdges, bool bUpdateGraphNode = true);
	virtual void RebuildTextArgumentsFromPreview(const FText& Preview) {}

	// Constructs the ConstructedText.
	virtual void RebuildConstructedText(const UDlgContext& Context) {}

	// Gets the text arguments for this Node (if any). Used for FText::Format
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const TArray<FDlgTextArgument>& GetTextArguments() const
	{
		static TArray<FDlgTextArgument> EmptyArray;
		return EmptyArray;
	};

	// Gets the Text of this Node. This can be the final formatted string.
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const FText& GetNodeText() const { return FText::GetEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual bool GetCheckChildrenOnEvaluation() const { return bCheckChildrenOnEvaluation; }

	/**
	 * Gets the Raw unformatted Text of this Node. Usually the same as GetNodeText but in case the node supports formatted string this
	 * is the raw form with all the arguments intact. To get the text arguments call GetTextArguments.
	 */
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual const FText& GetNodeUnformattedText() const { return GetNodeText(); }

	// Gets the voice of this Node as a SoundWave.
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	USoundWave* GetNodeVoiceSoundWave() const;

	// Gets the voice of this Node as a SoundWave.
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual USoundBase* GetNodeVoiceSoundBase() const { return nullptr; }

	// Gets the voice of this Node as a DialogueWave. Only the first Dialogue context in the wave should be used.
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual UDialogueWave* GetNodeVoiceDialogueWave() const { return nullptr; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual class UFMODEvent*  GetNodeFMODEvent() const { return nullptr; }

	// Gets the speaker state ordered to this node (can be used e.g. for icon selection)
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual FName GetSpeakerState() const { return NAME_None; }
	virtual void AddAllSpeakerStatesIntoSet(TSet<FName>& OutStates) const {};

	// Gets the generic data asset of this Node.
	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual UObject* GetNodeGenericData() const { return nullptr; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
	virtual UDlgNodeData* GetNodeData() const { return nullptr; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual const bool HasTypewriterEffect() const { return false; }

	UFUNCTION(BlueprintPure, Category = "Dialogue|Node")
		virtual const float GetTypewriterTypingDelay() const { return 0.0f; }

	// Helper method to get directly the Dialogue (which is our parent)
	UDlgDialogue* GetDialogue() const;

	// Helper functions to get the names of some properties. Used by the DlgSystemEditor module.
	static FName GetMemberNameOwnerName() { return GET_MEMBER_NAME_CHECKED(UDlgNode, OwnerName); }
	static FName GetMemberNameCheckChildrenOnEvaluation() { return GET_MEMBER_NAME_CHECKED(UDlgNode, bCheckChildrenOnEvaluation); }
	static FName GetMemberNameEnterConditions() { return GET_MEMBER_NAME_CHECKED(UDlgNode, EnterConditions); }
	static FName GetMemberNameEnterRestriction() { return GET_MEMBER_NAME_CHECKED(UDlgNode, EnterRestriction); }
	static FName GetMemberNameEnterEvents() { return GET_MEMBER_NAME_CHECKED(UDlgNode, EnterEvents); }
	static FName GetMemberNameNextSpeechTimer() { return GET_MEMBER_NAME_CHECKED(UDlgNode, fTimeToNextSpeech); }
	static FName GetMemberNameCustomReturn() { return GET_MEMBER_NAME_CHECKED(UDlgNode, bCustomReturn); }
	static FName GetMemberNameNodeToReturnGUID() { return GET_MEMBER_NAME_CHECKED(UDlgNode, NodeToReturnGUID); }
	static FName GetMemberNameNodeToReturnIndex() { return GET_MEMBER_NAME_CHECKED(UDlgNode, NodeToReturnIndex); }
	static FName GetMemberNameNodeIsCustomInterrupt() { return GET_MEMBER_NAME_CHECKED(UDlgNode, bCustomInterrupt); }
	static FName GetMemberNameNodeInterruptInfo() { return GET_MEMBER_NAME_CHECKED(UDlgNode, InterruptInfo); }
	static FName GetMemberGivingNameItemName() { return GET_MEMBER_NAME_CHECKED(UDlgNode, GivingItem); }
	static FName GetMemberGettingNameItemName() { return GET_MEMBER_NAME_CHECKED(UDlgNode, RequestItem); }
	static FName GetMemberNameChildren() { return GET_MEMBER_NAME_CHECKED(UDlgNode, Children); }
	static FName GetMemberNameGUID() { return GET_MEMBER_NAME_CHECKED(UDlgNode, NodeGUID); }

	// Syncs the GraphNode Edges with our edges
	void UpdateGraphNode();

	// Fires this Node enter Events
	void FireNodeEnterEvents(UDlgContext& Context);

	UFUNCTION(BlueprintCallable)
	void ChooseOption(UDlgContext* Context, int32 OptionIndex);

	UPROPERTY(EditAnywhere, Category="Dialogue|Node", meta=(InlineEditConditionToggle))
	bool bCustomTimer = false;
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta = (DisplayName = "Timer to next node", EditCondition=bCustomTimer))
    float fTimeToNextSpeech = 3.f;

	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta = (DisplayName = "Custom return node"))
	bool bCustomReturn = false;

	//Node to return in this dialogue if it was interrupted on this node
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta = (EditCondition = bCustomReturn, EditConditionHides))
	FGuid NodeToReturnGUID;

	UPROPERTY(VisibleAnywhere, Category = "Dialogue|Node", meta = (EditConditionHides))
	int32 NodeToReturnIndex;


	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	bool bCustomInterrupt = false;

	//Cannot start another dialogue if not interruptible
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", meta = (EditCondition = "bCustomInterrupt", EditConditionHides))
	FNodeInterruptInfo InterruptInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Node")
	FDialogueItem GivingItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Node")
	FDialogueItem RequestItem;


	virtual void RemapOldIndicesWithNew(const TMap<int32, int32>& OldToNewIndexMap);


protected:
#if WITH_EDITORONLY_DATA
	// Node's Graph representation, used to get position.
	UPROPERTY(Meta = (DlgNoExport))
	TObjectPtr<UEdGraphNode> GraphNode = nullptr;

	// Used to build the change event and broadcast it
	int32 BroadcastPropertyEdgeIndexChanged = INDEX_NONE;
#endif // WITH_EDITORONLY_DATA

	// Name of a participant (speaker) associated with this node.
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node", Meta = (DisplayName = "Participant Name"))
	FName OwnerName;

	/**
	 *  If it is set the node is only satisfied if at least one of its children is
	 *  Should not be used if entering this node can modify the condition results of its children.
	 */
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	bool bCheckChildrenOnEvaluation = false;

	// Conditions necessary to enter this node
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	TArray<FDlgCondition> EnterConditions;

	// Additional restriction on node enter
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	EDlgEntryRestriction EnterRestriction;

	// Events fired when the node is reached in the dialogue
	UPROPERTY(EditAnywhere, Category = "Dialogue|Node")
	TArray<FDlgEvent> EnterEvents;


	// The Unique identifier for each Node. This is much safer than a Node Index.
	// Compile/Save Asset to generate this
	UPROPERTY(VisibleAnywhere, Category = "Dialogue|Node", AdvancedDisplay)
	FGuid NodeGUID;
	// NOTE: For some reason if this is named GUID the details panel does not work all the time for this, wtf unreal?

	// Edges that point to Children of this Node
	UPROPERTY(VisibleAnywhere, EditFixedSize, AdvancedDisplay, Category = "Dialogue|Node")
	TArray<FDlgEdge> Children;
};
