// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgDialogue.h"

#include "UObject/DevObjectVersion.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/DataValidation.h"
#endif

#include "DlgSystemModule.h"
#include "IO/DlgConfigParser.h"
#include "IO/DlgConfigWriter.h"
#include "IO/DlgJsonWriter.h"
#include "IO/DlgJsonParser.h"
#include "Nodes/DlgNode_Speech.h"
#include "Nodes/DlgNode_End.h"
#include "Nodes/DlgNode_Start.h"
#include "DlgManager.h"
#include "Logging/DlgLogger.h"
#include "DlgHelper.h"

#if WITH_EDITOR
#include "Nodes/DlgNode_Proxy.h"
#endif

#define LOCTEXT_NAMESPACE "DlgDialogue"

// Unique DlgDialogue Object version id, generated with random
const FGuid FDlgDialogueObjectVersion::GUID(0x2B8E5105, 0x6F66348F, 0x2A8A0B25, 0x9047A071);
// Register Dialogue custom version with Core
FDevVersionRegistration GRegisterDlgDialogueObjectVersion(FDlgDialogueObjectVersion::GUID,
														  FDlgDialogueObjectVersion::LatestVersion, TEXT("Dev-DlgDialogue"));


// Update dialogue up to the ConvertedNodesToUObject version
void UpdateDialogueToVersion_ConvertedNodesToUObject(UDlgDialogue* Dialogue)
{
	// No Longer supported, get data from text file, and reconstruct everything
	Dialogue->InitialSyncWithTextFile();
#if WITH_EDITOR
	// Force clear the old graph
	Dialogue->ClearGraph();
#endif
}

// Update dialogue up to the UseOnlyOneOutputAndInputPin version
void UpdateDialogueToVersion_UseOnlyOneOutputAndInputPin(UDlgDialogue* Dialogue)
{
#if WITH_EDITOR
	Dialogue->GetDialogueEditorAccess()->UpdateDialogueToVersion_UseOnlyOneOutputAndInputPin(Dialogue);
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin UObject interface
#if NY_ENGINE_VERSION >= 500
void UDlgDialogue::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);
#else
void UDlgDialogue::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#endif

	Name = GetDialogueFName();
	bWasLoaded = true;
	OnPreAssetSaved();
}

void UDlgDialogue::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FDlgDialogueObjectVersion::GUID);
	Super::Serialize(Ar);
	const int32 DialogueVersion = Ar.CustomVer(FDlgDialogueObjectVersion::GUID);
	if (DialogueVersion < FDlgDialogueObjectVersion::ConvertedNodesToUObject)
	{
		// No Longer supported
		return;
	}
}

void UDlgDialogue::PostLoad()
{
	Super::PostLoad();
	const int32 DialogueVersion = GetLinkerCustomVersion(FDlgDialogueObjectVersion::GUID);
	// Old files, UDlgNode used to be a FDlgNode
	if (DialogueVersion < FDlgDialogueObjectVersion::ConvertedNodesToUObject)
	{
		UpdateDialogueToVersion_ConvertedNodesToUObject(this);
	}

	// Simplified and reduced the number of pins (only one input/output pin), used for the new visualization
	if (DialogueVersion < FDlgDialogueObjectVersion::UseOnlyOneOutputAndInputPin)
	{
		UpdateDialogueToVersion_UseOnlyOneOutputAndInputPin(this);
	}

	// Simply the number of nodes, VirtualParent Node is merged into Speech Node and SelectRandom and SelectorFirst are merged into one Selector Node
	if (DialogueVersion < FDlgDialogueObjectVersion::MergeVirtualParentAndSelectorTypes)
	{
		FDlgLogger::Get().Warningf(
			TEXT("Dialogue = `%s` with Version MergeVirtualParentAndSelectorTypes will not be converted. See https://gitlab.com/snippets/1691704 for manual conversion"),
			*GetTextFilePathName()
		);
	}

	// Refresh the data, so that it is valid after loading.
	if (DialogueVersion < FDlgDialogueObjectVersion::AddTextFormatArguments ||
		DialogueVersion < FDlgDialogueObjectVersion::AddCustomObjectsToParticipantsData)
	{
		UpdateAndRefreshData();
	}

	if (DialogueVersion < FDlgDialogueObjectVersion::AddSupportForMultipleStartNodes && StartNode_DEPRECATED != nullptr)
	{
		StartNodes.Add(StartNode_DEPRECATED);
	}

	// Create thew new GUID
	if (!HasGUID())
	{
		RegenerateGUID();
		FDlgLogger::Get().Debugf(
			TEXT("Creating new GUID = `%s` for Dialogue = `%s` because of of invalid GUID."),
			*GUID.ToString(), *GetPathName()
		);
	}

#if WITH_EDITOR
	const bool bHasDialogueEditorModule = GetDialogueEditorAccess().IsValid();
	// If this is false it means the graph nodes are not even created? Check for old files that were saved
	// before graph editor was even implemented. The editor will popup a prompt from FDlgEditorUtilities::TryToCreateDefaultGraph
	if (bHasDialogueEditorModule && !GetDialogueEditorAccess()->AreDialogueNodesInSyncWithGraphNodes(this))
	{
		return;
	}
#endif

	// Check Nodes for validity
	const int32 NodesNum = Nodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < NodesNum; NodeIndex++)
	{
		UDlgNode* Node = Nodes[NodeIndex];
#if WITH_EDITOR
		if (bHasDialogueEditorModule)
		{
			checkf(Node->GetGraphNode(), TEXT("Expected DialogueVersion = %d to have a valid GraphNode for Node index = %d :("), DialogueVersion, NodeIndex);
		}
#endif
		// Check children point to the right Node
		const TArray<FDlgEdge>& NodeEdges = Node->GetNodeChildren();
		const int32 EdgesNum = NodeEdges.Num();
		for (int32 EdgeIndex = 0; EdgeIndex < EdgesNum; EdgeIndex++)
		{
			const FDlgEdge& Edge = NodeEdges[EdgeIndex];
			if (!Edge.IsValid())
			{
				continue;
			}

			if (!Nodes.IsValidIndex(Edge.TargetIndex))
			{
				UE_LOG(
					LogDlgSystem,
					Fatal,
					TEXT("Node with index = %d does not have a valid Edge index = %d with TargetIndex = %d"),
					NodeIndex, EdgeIndex, Edge.TargetIndex
				);
			}
		}
	}

	bWasLoaded = true;
}

void UDlgDialogue::PostInitProperties()
{
	Super::PostInitProperties();

	// Ignore these cases
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		return;
	}

	const int32 DialogueVersion = GetLinkerCustomVersion(FDlgDialogueObjectVersion::GUID);

#if WITH_EDITOR
	// Wait for the editor module to be set by the editor in UDialogueGraph constructor
	if (GetDialogueEditorAccess().IsValid())
	{
		CreateGraph();
	}
#endif // #if WITH_EDITOR

	// Keep Name in sync with the file name
	Name = GetDialogueFName();

	// Used when creating new Dialogues
	// Initialize with a valid GUID
	if (DialogueVersion >= FDlgDialogueObjectVersion::AddGUID && !HasGUID())
	{
		RegenerateGUID();
		FDlgLogger::Get().Debugf(
			TEXT("Creating new GUID = `%s` for Dialogue = `%s` because of new created Dialogue."),
			*GUID.ToString(), *GetPathName()
		);
	}
}

void UDlgDialogue::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);
	Name = GetDialogueFName();
}

void UDlgDialogue::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Used when duplicating dialogues.
	// Make new guid for this copied Dialogue.
	RegenerateGUID();
	FDlgLogger::Get().Debugf(
		TEXT("Creating new GUID = `%s` for Dialogue = `%s` because Dialogue was copied."),
		*GUID.ToString(), *GetPathName()
	);
}

void UDlgDialogue::PostEditImport()
{
	Super::PostEditImport();

	// Used when duplicating dialogues.
	// Make new guid for this copied Dialogue
	RegenerateGUID();
	FDlgLogger::Get().Debugf(
		TEXT("Creating new GUID = `%s` for Dialogue = `%s` because Dialogue was copied."),
		*GUID.ToString(), *GetPathName()
	);
}

#if WITH_EDITOR
TSharedPtr<IDlgEditorAccess> UDlgDialogue::DialogueEditorAccess = nullptr;

bool UDlgDialogue::Modify(bool bAlwaysMarkDirty)
{
	if (!CanModify())
	{
		return false;
	}

	const bool bWasSaved = Super::Modify(bAlwaysMarkDirty);
	// if (StartNode)
	// {
	// 	bWasSaved = bWasSaved && StartNode->Modify(bAlwaysMarkDirty);
	// }

	// for (UDlgNode* Node : Nodes)
	// {
	// 	bWasSaved = bWasSaved && Node->Modify(bAlwaysMarkDirty);
	// }

	return bWasSaved;
}

void UDlgDialogue::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Signal to the listeners
	check(OnDialoguePropertyChanged.IsBound());
	OnDialoguePropertyChanged.Broadcast(PropertyChangedEvent);

	if(PropertyChangedEvent.GetPropertyName() == "DialogueType")
	{
		if (DialogueType == EDialogueType::DLG_Base)
		{
			BitIntend = EPlayerAnswerIntend::BINT_Default;
		}
	}
	if(PropertyChangedEvent.GetPropertyName() == "BitIntend")
	{
		if (BitIntend == EPlayerAnswerIntend::BINT_Leave)
		{
			ReturnToMainOnEnd = false;
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == "ParticipantsData")
	{
		if (!ParticipantsData.IsEmpty() && !ParticipantsData.begin().Key().IsNone())
		{
			MainParticipantName = ParticipantsData.begin().Key();
		}
	}
}

void UDlgDialogue::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	UpdateAndRefreshData();

	const auto* ActiveMemberNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	const auto* ActivePropertyNode = PropertyChangedEvent.PropertyChain.GetActiveNode();
	const FName MemberPropertyName = ActiveMemberNode && ActiveMemberNode->GetValue() ? ActiveMemberNode->GetValue()->GetFName() : NAME_None;
	const FName PropertyName = ActivePropertyNode && ActivePropertyNode->GetValue() ? ActivePropertyNode->GetValue()->GetFName() : NAME_None;

	// Check if the participant UClass implements our interface
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, ParticipantsClasses))
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FDlgParticipantClass, ParticipantClass))
		{
			//const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberPropertyName.ToString());
			for (FDlgParticipantClass& Participant : ParticipantsClasses)
			{
				if (!IsValid(Participant.ParticipantClass))
				{
					continue;
				}

				if (!Participant.ParticipantClass->ImplementsInterface(UDlgDialogueParticipant::StaticClass()))
				{
					Participant.ParticipantClass = nullptr;
				}
			}
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UDlgDialogue::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	// Add the graph to the list of referenced objects
	UDlgDialogue* This = CastChecked<UDlgDialogue>(InThis);
	Collector.AddReferencedObject(This->DlgGraph, This);
	Super::AddReferencedObjects(InThis, Collector);
}

#endif

// End UObject interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin AssetUserData interface


void UDlgDialogue::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != nullptr)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != nullptr)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UDlgDialogue::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return nullptr;
}

void UDlgDialogue::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UDlgDialogue::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}
// End AssetUserData interface
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions

#if WITH_EDITOR

void UDlgDialogue::CreateGraph()
{
	// The Graph will only be null if this is the first time we are creating the graph for the Dialogue.
	// After the Dialogue asset is saved, the Dialogue will get the dialogue from the serialized uasset.
	if (DlgGraph != nullptr)
	{
		return;
	}

	if (StartNodes.Num() == 0 || !IsValid(StartNodes[0]))
	{
		StartNodes.Add(ConstructDialogueNode<UDlgNode_Start>());
	}

	FDlgLogger::Get().Debugf(TEXT("Creating graph for Dialogue = `%s`"), *GetPathName());
	DlgGraph = GetDialogueEditorAccess()->CreateNewDialogueGraph(this);

	// Give the schema a chance to fill out any required nodes
	DlgGraph->GetSchema()->CreateDefaultNodesForGraph(*DlgGraph);
	MarkPackageDirty();
}

void UDlgDialogue::ClearGraph()
{
	if (!IsValid(DlgGraph))
	{
		return;
	}

	FDlgLogger::Get().Debugf(TEXT("Clearing graph for Dialogue = `%s`"), *GetPathName());
	GetDialogueEditorAccess()->RemoveAllGraphNodes(this);

	// Give the schema a chance to fill out any required nodes
	DlgGraph->GetSchema()->CreateDefaultNodesForGraph(*DlgGraph);
	MarkPackageDirty();
}

void UDlgDialogue::CompileDialogueNodesFromGraphNodes()
{
	if (!bCompileDialogue)
	{
		return;
	}

	FDlgLogger::Get().Infof(TEXT("Compiling Dialogue = `%s` (Graph data -> Dialogue data)`"), *GetPathName());
	GetDialogueEditorAccess()->CompileDialogueNodesFromGraphNodes(this);
}
#endif // #if WITH_EDITOR

void UDlgDialogue::ImportFromFile()
{
	// Simply ignore reloading
	const EDlgDialogueTextFormat TextFormat = GetDefault<UDlgSystemSettings>()->DialogueTextFormat;
	if (TextFormat == EDlgDialogueTextFormat::None)
	{
		UpdateAndRefreshData();
		return;
	}

	ImportFromFileFormat(TextFormat);
}

void UDlgDialogue::ImportFromFileFormat(EDlgDialogueTextFormat TextFormat)
{
	const bool bHasExtension = UDlgSystemSettings::HasTextFileExtension(TextFormat);
	const FString& TextFileName = GetTextFilePathName(TextFormat);

	// Nothing to do
	IFileManager& FileManager = IFileManager::Get();
	if (!bHasExtension)
	{
		// Useful For debugging
		if (TextFormat == EDlgDialogueTextFormat::All)
		{
			// Import from all
			const int32 TextFormatsNum = static_cast<int32>(EDlgDialogueTextFormat::NumTextFormats);
			for (int32 TextFormatIndex = static_cast<int32>(EDlgDialogueTextFormat::StartTextFormats);
					   TextFormatIndex < TextFormatsNum; TextFormatIndex++)
			{
				const EDlgDialogueTextFormat CurrentTextFormat = static_cast<EDlgDialogueTextFormat>(TextFormatIndex);
				const FString& CurrentTextFileName = GetTextFilePathName(CurrentTextFormat);
				if (FileManager.FileExists(*CurrentTextFileName))
				{
					ImportFromFileFormat(CurrentTextFormat);
				}
			}
		}
		return;
	}

	// File does not exist abort
	if (!FileManager.FileExists(*TextFileName))
	{
		FDlgLogger::Get().Errorf(TEXT("Reloading data for Dialogue = `%s` FROM file = `%s` FAILED, because the file does not exist"), *GetPathName(), *TextFileName);
		return;
	}

	// Clear data first
	StartNode_DEPRECATED = nullptr;
	Nodes.Empty();
	StartNodes.Empty();

	// TODO handle Name == NAME_None or invalid filename
	FDlgLogger::Get().Infof(TEXT("Reloading data for Dialogue = `%s` FROM file = `%s`"), *GetPathName(), *TextFileName);

	// TODO(vampy): Check for errors
	check(TextFormat != EDlgDialogueTextFormat::None);
	switch (TextFormat)
	{
		case EDlgDialogueTextFormat::JSON:
		{
			FDlgJsonParser JsonParser;
			JsonParser.InitializeParser(TextFileName);
			JsonParser.ReadAllProperty(GetClass(), this, this);
			break;
		}
		case EDlgDialogueTextFormat::DialogueDEPRECATED:
		{
			FDlgConfigParser Parser(TEXT("Dlg"));
			Parser.InitializeParser(TextFileName);
			Parser.ReadAllProperty(GetClass(), this, this);
			break;
		}
		default:
			checkNoEntry();
			break;
	}

	if (IsValid(StartNode_DEPRECATED))
	{
		StartNodes.Add(StartNode_DEPRECATED);
	}


	if (StartNodes.Num() == 0)
	{
		StartNodes.Add(ConstructDialogueNode<UDlgNode_Speech>());
	}

	// TODO(vampy): validate if data is legit, indicies exist and that sort.
	// Check if Guid is not a duplicate
	const TArray<UDlgDialogue*> DuplicateDialogues = UDlgManager::GetDialoguesWithDuplicateGUIDs();
	if (DuplicateDialogues.Num() > 0)
	{
		if (DuplicateDialogues.Contains(this))
		{
			// found duplicate of this Dialogue
			RegenerateGUID();
			FDlgLogger::Get().Warningf(
				TEXT("Creating new GUID = `%s` for Dialogue = `%s` because the input file contained a duplicate GUID."),
				*GUID.ToString(), *GetPathName()
			);
		}
		else
		{
			// We have bigger problems on our hands
			FDlgLogger::Get().Errorf(
				TEXT("Found Duplicate Dialogue that does not belong to this Dialogue = `%s`, DuplicateDialogues.Num = %d"),
				*GetPathName(),  DuplicateDialogues.Num()
			);
		}
	}

	Name = GetDialogueFName();
	UpdateAndRefreshData(true);
}

void UDlgDialogue::OnPreAssetSaved()
{
#if WITH_EDITOR
	// Compile, graph data -> dialogue data
	CompileDialogueNodesFromGraphNodes();
#endif

	// Save file, dialogue data -> text file (.dlg)
	UpdateAndRefreshData(true);
	ExportToFile();
}

void UDlgDialogue::ExportToFile() const
{
	const EDlgDialogueTextFormat TextFormat = GetDefault<UDlgSystemSettings>()->DialogueTextFormat;
	if (TextFormat == EDlgDialogueTextFormat::None)
	{
		// Simply ignore saving
		return;
	}

	ExportToFileFormat(TextFormat);
}

void UDlgDialogue::ExportToFileFormat(EDlgDialogueTextFormat TextFormat) const
{
	// TODO(vampy): Check for errors
	const bool bHasExtension = UDlgSystemSettings::HasTextFileExtension(TextFormat);
	const FString& TextFileName = GetTextFilePathName(TextFormat);
	if (bHasExtension)
	{
		FDlgLogger::Get().Infof(TEXT("Exporting data for Dialogue = `%s` TO file = `%s`"), *GetPathName(), *TextFileName);
	}

	switch (TextFormat)
	{
		case EDlgDialogueTextFormat::JSON:
		{
			FDlgJsonWriter JsonWriter;
			JsonWriter.Write(GetClass(), this);
			JsonWriter.ExportToFile(TextFileName);
			break;
		}
		case EDlgDialogueTextFormat::DialogueDEPRECATED:
		{
			FDlgConfigWriter DlgWriter(TEXT("Dlg"));
			DlgWriter.Write(GetClass(), this);
			DlgWriter.ExportToFile(TextFileName);
			break;
		}
		case EDlgDialogueTextFormat::All:
		{
			// Useful for debugging
			// Export to all  formats
			const int32 TextFormatsNum = static_cast<int32>(EDlgDialogueTextFormat::NumTextFormats);
			for (int32 TextFormatIndex = static_cast<int32>(EDlgDialogueTextFormat::StartTextFormats);
					   TextFormatIndex < TextFormatsNum; TextFormatIndex++)
			{
				const EDlgDialogueTextFormat CurrentTextFormat = static_cast<EDlgDialogueTextFormat>(TextFormatIndex);
				ExportToFileFormat(CurrentTextFormat);
			}
			break;
		}
		default:
			// It Should not have any extension
			check(!bHasExtension);
			break;
	}
}

FDlgParticipantData& UDlgDialogue::GetParticipantDataEntry(FName ParticipantName, FName FallbackParticipantName, bool bCheckNone, const FString& ContextMessage)
{
	// Used to ignore some participants
	static FDlgParticipantData BlackHoleParticipant;

	// If the Participant Name is not set, it adopts the Node Owner Name
	const FName& ValidParticipantName = ParticipantName == NAME_None ? FallbackParticipantName : ParticipantName;

	// Parent/child is not valid, simply do nothing
	if (bCheckNone && ValidParticipantName == NAME_None)
	{
		FDlgLogger::Get().Warningf(
			TEXT("Ignoring ParticipantName = None, Context = `%s`. Either your node participant name is None or your participant name is None."),
			*ContextMessage
		);
		return BlackHoleParticipant;
	}

	return ParticipantsData.FindOrAdd(ValidParticipantName);
}

void UDlgDialogue::AddConditionsDataFromNodeEdges(const UDlgNode* Node, int32 NodeIndex)
{
	const FString NodeContext = FString::Printf(TEXT("Node %s"), NodeIndex > INDEX_NONE ? *FString::FromInt(NodeIndex) : TEXT("Start") );
	const FName FallbackParticipantName = Node->GetNodeParticipantName();

	for (const FDlgEdge& Edge : Node->GetNodeChildren())
	{
		const int32 TargetIndex = Edge.TargetIndex;

		for (const FDlgCondition& Condition : Edge.Conditions)
		{
			if (Condition.IsParticipantInvolved())
			{
				const FString ContextMessage = FString::Printf(TEXT("Adding Edge primary condition data from %s to Node %d"), *NodeContext, TargetIndex);
				GetParticipantDataEntry(Condition.ParticipantName, FallbackParticipantName, true, ContextMessage)
					.AddConditionPrimaryData(Condition);
			}
			if (Condition.IsSecondParticipantInvolved())
			{
				const FString ContextMessage = FString::Printf(TEXT("Adding Edge secondary condition data from %s to Node %d"), *NodeContext, TargetIndex);
				GetParticipantDataEntry(Condition.OtherParticipantName, FallbackParticipantName, true, ContextMessage)
					.AddConditionSecondaryData(Condition);
			}
		}
	}
}

void UDlgDialogue::RebuildAndUpdateNode(UDlgNode* Node, const UDlgSystemSettings& Settings, bool bUpdateTextsNamespacesAndKeys)
{
	static constexpr bool bEdges = true;
	static constexpr bool bUpdateGraphNode = false;

	// Rebuild & Update
	// NOTE: this can do a dialogue data -> graph node data update
	Node->RebuildTextArguments(bEdges, bUpdateGraphNode);
	Node->UpdateTextsValuesFromDefaultsAndRemappings(Settings, bEdges, bUpdateGraphNode);
	if (bUpdateTextsNamespacesAndKeys)
	{
		Node->UpdateTextsNamespacesAndKeys(Settings, bEdges, bUpdateGraphNode);
	}

	// Sync with the editor aka bUpdateGraphNode = true
	Node->UpdateGraphNode();
}

void UDlgDialogue::UpdateAndRefreshData(bool bUpdateTextsNamespacesAndKeys)
{
	FDlgLogger::Get().Infof(TEXT("Refreshing data for Dialogue = `%s`"), *GetPathName());

	const UDlgSystemSettings* Settings = GetDefault<UDlgSystemSettings>();
	ParticipantsData.Empty();
	AllSpeakerStates.Empty();

	// do not forget about the edges of the Root/Start Node
	for (UDlgNode* StartNode : StartNodes)
	{
		AddConditionsDataFromNodeEdges(StartNode, INDEX_NONE);
		RebuildAndUpdateNode(StartNode, *Settings, bUpdateTextsNamespacesAndKeys);
	}

	// Regular Nodes
	const int32 NodesNum = Nodes.Num();
	for (int32 NodeIndex = 0; NodeIndex < NodesNum; NodeIndex++)
	{
		const FString NodeContext = FString::Printf(TEXT("Node %d"), NodeIndex);
		UDlgNode* Node = Nodes[NodeIndex];
		const FName NodeParticipantName = Node->GetNodeParticipantName();

		// Rebuild & Update
		RebuildAndUpdateNode(Node, *Settings, bUpdateTextsNamespacesAndKeys);

		// participant names
		TArray<FName> Participants;
		Node->GetAssociatedParticipants(Participants);
		for (const FName& Participant : Participants)
		{
			if (!ParticipantsData.Contains(Participant))
			{
				ParticipantsData.Add(Participant);
			}
		}

		// gather SpeakerStates
		Node->AddAllSpeakerStatesIntoSet(AllSpeakerStates);

		// Conditions from nodes
		for (const FDlgCondition& Condition : Node->GetNodeEnterConditions())
		{
			if (Condition.IsParticipantInvolved())
			{
				const FString ContextMessage = FString::Printf(TEXT("Adding primary condition data for %s"), *NodeContext);
				GetParticipantDataEntry(Condition.ParticipantName, NodeParticipantName, true, ContextMessage)
					.AddConditionPrimaryData(Condition);
			}
			if (Condition.IsSecondParticipantInvolved())
			{
				const FString ContextMessage = FString::Printf(TEXT("Adding secondary condition data for %s"), *NodeContext);
				GetParticipantDataEntry(Condition.OtherParticipantName, NodeParticipantName, true, ContextMessage)
					.AddConditionSecondaryData(Condition);
			}
		}

		// Gather Edge Data
		AddConditionsDataFromNodeEdges(Node, NodeIndex);

		// Walk over edges of speaker nodes
		// NOTE: for speaker sequence nodes, the inner edges are handled by AddAllSpeakerStatesIntoSet
		// so no need to special case handle it
		const int32 NumNodeChildren = Node->GetNumNodeChildren();
		for (int32 EdgeIndex = 0; EdgeIndex < NumNodeChildren; EdgeIndex++)
		{
			const FDlgEdge& Edge = Node->GetNodeChildAt(EdgeIndex);
			const int32 TargetIndex = Edge.TargetIndex;

			// Speaker states
			AllSpeakerStates.Add(Edge.SpeakerState);

			// Text arguments are rebuild from the Node
			for (const FDlgTextArgument& TextArgument : Edge.GetTextArguments())
			{
				const FString ContextMessage = FString::Printf(TEXT("Adding Edge text arguments data from %s, to Node %d"), *NodeContext, TargetIndex);
				GetParticipantDataEntry(TextArgument.ParticipantName, NodeParticipantName, true, ContextMessage)
					.AddTextArgumentData(TextArgument);
			}
		}

		// Events
		for (const FDlgEvent& Event : Node->GetNodeEnterEvents())
		{
			const FString ContextMessage = FString::Printf(TEXT("Adding events data for %s"), *NodeContext);
			GetParticipantDataEntry(Event.ParticipantName, NodeParticipantName, true, ContextMessage)
				.AddEventData(Event);
		}

		// Text arguments
		for (const FDlgTextArgument& TextArgument : Node->GetTextArguments())
		{
			const FString ContextMessage = FString::Printf(TEXT("Adding text arguments data for %s"), *NodeContext);
			GetParticipantDataEntry(TextArgument.ParticipantName, NodeParticipantName, true, ContextMessage)
				.AddTextArgumentData(TextArgument);
		}
	}

	// Remove default values
	AllSpeakerStates.Remove(FName(NAME_None));

	//
	// Fill ParticipantClasses
	//
	TSet<FName> Participants = GetParticipantNames();

	// 1. remove outdated entries
	for (int32 Index = ParticipantsClasses.Num() - 1; Index >= 0; --Index)
	{
		const FName ExaminedName = ParticipantsClasses[Index].ParticipantName;
		if (!Participants.Contains(ExaminedName) || ExaminedName.IsNone())
		{
			ParticipantsClasses.RemoveAtSwap(Index);
		}

		Participants.Remove(ExaminedName);
	}

	// 2. add new entries
	for (const FName& Participant : Participants)
	{
		if (Participant != NAME_None)
		{
			ParticipantsClasses.Add({ Participant, nullptr });
		}
		else
		{
			FDlgLogger::Get().Warning(TEXT("Trying to fill ParticipantsClasses, got a Participant name = None. Ignoring!"));
		}
	}

	// 3. Set auto default participant classes
	if (bWasLoaded && Settings->bAutoSetDefaultParticipantClasses)
	{
		TArray<UClass*> NativeClasses;
		TArray<UClass*> BlueprintClasses;
		FDlgHelper::GetAllClassesImplementingInterface(UDlgDialogueParticipant::StaticClass(), NativeClasses, BlueprintClasses);

		const TMap<FName, TArray<FDlgClassAndObject>> NativeClassesMap = FDlgHelper::ConvertDialogueParticipantsClassesIntoMap(NativeClasses);
		const TMap<FName, TArray<FDlgClassAndObject>> BlueprintClassesMap = FDlgHelper::ConvertDialogueParticipantsClassesIntoMap(BlueprintClasses);

		for (FDlgParticipantClass& Struct : ParticipantsClasses)
		{
			// Participant Name is not set or Class is set, ignore
			if (Struct.ParticipantName == NAME_None || Struct.ParticipantClass != nullptr)
			{
				continue;
			}

			// Blueprint
			if (BlueprintClassesMap.Contains(Struct.ParticipantName))
			{
				const TArray<FDlgClassAndObject>& Array = BlueprintClassesMap.FindChecked(Struct.ParticipantName);
				if (Array.Num() == 1)
				{
					Struct.ParticipantClass = Array[0].Class;
				}
			}

			// Native last resort
			if (Struct.ParticipantClass == nullptr && NativeClassesMap.Contains(Struct.ParticipantName))
			{
				const TArray<FDlgClassAndObject>& Array = NativeClassesMap.FindChecked(Struct.ParticipantName);
				if (Array.Num() == 1)
				{
					Struct.ParticipantClass = Array[0].Class;
				}
			}
		}
	}
}

FGuid UDlgDialogue::GetNodeGUIDForIndex(int32 NodeIndex) const
{
	if (IsValidNodeIndex(NodeIndex))
	{
		return Nodes[NodeIndex]->GetGUID();
	}

	// Invalid GUID
	return FGuid{};
}

int32 UDlgDialogue::GetNodeIndexForGUID(const FGuid& NodeGUID) const
{
	if (const int32* NodeIndexPtr = NodesGUIDToIndexMap.Find(NodeGUID))
	{
		return *NodeIndexPtr;
	}

	return INDEX_NONE;
}

void UDlgDialogue::SetStartNodes(TArray<UDlgNode*> InStartNodes)
{
	StartNodes = InStartNodes;
	// UpdateGUIDToIndexMap(StartNode, INDEX_NONE);
}

void UDlgDialogue::SetNodes(const TArray<UDlgNode*>& InNodes)
{
	Nodes = InNodes;
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		UpdateGUIDToIndexMap(Nodes[NodeIndex], NodeIndex);
	}
}

void UDlgDialogue::SetNode(int32 NodeIndex, UDlgNode* InNode)
{
	if (!IsValidNodeIndex(NodeIndex) || !InNode)
	{
		return;
	}

	Nodes[NodeIndex] = InNode;
	UpdateGUIDToIndexMap(InNode, NodeIndex);
}

void UDlgDialogue::UpdateGUIDToIndexMap(const UDlgNode* Node, int32 NodeIndex)
{
	if (!Node || !IsValidNodeIndex(NodeIndex) || !Node->HasGUID())
	{
		return;
	}

	NodesGUIDToIndexMap.Add(Node->GetGUID(), NodeIndex);
}

bool UDlgDialogue::IsEndNode(int32 NodeIndex) const
{
	if (!Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	return Nodes[NodeIndex]->IsA<UDlgNode_End>();
}

FString UDlgDialogue::GetTextFilePathName(bool bAddExtension/* = true*/) const
{
	return GetTextFilePathName(GetDefault<UDlgSystemSettings>()->DialogueTextFormat, bAddExtension);
}

FString UDlgDialogue::GetTextFilePathName(EDlgDialogueTextFormat TextFormat, bool bAddExtension/* = true*/) const
{
	// Extract filename from path
	// NOTE: this is not a filesystem path, it is an unreal path 'Outermost.[Outer:]Name'
	// Usually GetPathName works, but the path name might be weird.
	// FSoftObjectPath(this).ToString(); which does call this function GetPathName() but it returns a legit clean path
	// if it is in the wrong format
	FString TextFileName = GetTextFilePathNameFromAssetPathName(FSoftObjectPath(this).ToString());
	if (bAddExtension)
	{
		// Modify the extension of the base text file depending on the extension
		TextFileName += UDlgSystemSettings::GetTextFileExtension(TextFormat);
	}

	return TextFileName;
}

bool UDlgDialogue::DeleteTextFileForTextFormat(EDlgDialogueTextFormat TextFormat) const
{
	return DeleteTextFileForExtension(UDlgSystemSettings::GetTextFileExtension(TextFormat));
}

bool UDlgDialogue::DeleteTextFileForExtension(const FString& FileExtension) const
{
	const FString TextFilePathName = GetTextFilePathName(false);
	if (TextFilePathName.IsEmpty())
	{
		// Memory corruption? tread carefully here
		FDlgLogger::Get().Errorf(
			TEXT("Can't delete text file for Dialogue = `%s` because the file path name is empty :O"),
			*GetPathName()
		);
		return false;
	}

	const FString FullPathName = TextFilePathName + FileExtension;
	return FDlgHelper::DeleteFile(FullPathName);
}

bool UDlgDialogue::DeleteAllTextFiles() const
{
	bool bStatus = true;
	for (const FString& FileExtension : GetDefault<UDlgSystemSettings>()->GetAllTextFileExtensions())
	{
		bStatus &= DeleteTextFileForExtension(FileExtension);
	}
	return bStatus;
}

bool UDlgDialogue::IsInProjectDirectory() const
{
	return FDlgHelper::IsPathInProjectDirectory(GetPathName());
}

FString UDlgDialogue::GetTextFilePathNameFromAssetPathName(const FString& AssetPathName)
{
	static const TCHAR* Separator = TEXT("/");

	// Get rid of the extension from `filename.extension` from the end of the path
	FString PathName = FPaths::GetBaseFilename(AssetPathName, false);

	// Get rid of the first folder, Game/ or Name/ (if in the plugins dir) from the beginning of the path.
	// Are we in the game directory?
	FString ContentDir = FPaths::ProjectContentDir();
	if (!PathName.RemoveFromStart(TEXT("/Game/")))
	{
		// We are in the plugins dir
		TArray<FString> PathParts;
		PathName.ParseIntoArray(PathParts, Separator);
		if (PathParts.Num() > 0)
		{
			const FString PluginName = PathParts[0];
			const FString PluginDir = FPaths::ProjectPluginsDir() / PluginName;

			// Plugin exists
			if (FPaths::DirectoryExists(PluginDir))
			{
				ContentDir = PluginDir / TEXT("Content/");
			}

			// remove plugin name
			PathParts.RemoveAt(0);
			PathName = FString::Join(PathParts, Separator);
		}
	}

	return ContentDir + PathName;
}


// End own functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef LOCTEXT_NAMESPACE

/*
 * TallValidation
 */
#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "DlgDialogueValidation"
/*copied from UTallEditorValidationSubsystem*/
TMap<FName, FString> /*UTallEditorValidationSubsystem::*/GetAllEnumPropertiesAsStrings(const UObject* Object, const FString SubString)
{
	if (!Object)
		return {};

	TRACE_CPUPROFILER_EVENT_SCOPE(UDlgDialogue::GetAllEnumPropertiesAsStrings)

	TMap<FName, FString> OutStrings;
	for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			FString PropertyName = EnumProperty->GetName();

			// Retrieve the enum type (UEnum object) associated with this property
			UEnum* Enum = EnumProperty->GetEnum();
			if (!Enum)
				continue;

			// Get the enum value as an integer
			void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(const_cast<UObject*>(Object));
			int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);

			// Get the enum value as a string
			FString EnumValueString = Enum->GetNameStringByValue(EnumValue);

			// Output the enum property name and value
			// UE_LOG(LogTemp, Log, TEXT("Enum Property: %s, Value: %s, Is Valid: %s"), *PropertyName, *EnumValueString, Enum->IsValidEnumValue(EnumValue) ? TEXT("True") : TEXT("False"));
			if(SubString.IsEmpty())
				OutStrings.Add(FName(PropertyName), EnumValueString);
			else if(EnumValueString.Contains(SubString, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
				OutStrings.Add(FName(PropertyName), EnumValueString);

		}
	}
	return OutStrings;
}




EDataValidationResult UDlgDialogue::IsDataValid(class FDataValidationContext& Context) const
{
	if(Context.GetValidationUsecase() != EDataValidationUsecase::Manual)
		return UObject::IsDataValid(Context);


	//TODO IMPORTANT: move validation into separate file with static functions to be able to call from other classes

	//TODO: to run IsDataValid on all conditions in this dialogue. I won't do it now because no one is using IsDataValid yet
	TArray<UDlgConditionCustom> AllCustomConditions;
	TArray<UDlgEventCustom> AllCustomEvents;

	//Check enums on dialogue
	for (const auto& InvalidEnumProp : GetAllEnumPropertiesAsStrings(this, "_MAX"))
	{
		Context.AddWarning(FText::Format(INVTEXT("Invalid Enum Property {0} ({1})."),
			FText::FromString(InvalidEnumProp.Key.ToString()),
			FText::FromString(InvalidEnumProp.Value)));
	}


	for (const auto Node : GetNodes())
	{
		//Check Nodes
		//Check Proxy Nodes
		if(auto ProxyNode = Cast<UDlgNode_Proxy>(Node))
		{
			if(!IsValidNodeIndex(ProxyNode->GetTargetNodeIndex()))
			{
				Context.AddError(FText::Format(INVTEXT("Invalid Target Index {0} on Proxy node {1} ({2})."),
				ProxyNode->GetTargetNodeIndex(),
				GetNodeIndexForGUID(Node->GetGUID()),
				FText::FromString(Node->GetGUID().ToString())));
			}
		}


		//Check Conditions
		TArray<FDlgCondition> EnterConditions = Node->GetNodeEnterConditions();
		for (int i = 0; i < EnterConditions.Num(); i++)
		{
			const FDlgCondition& Condition = EnterConditions[i];
			if(Condition.ConditionType == EDlgConditionType::Custom)
			{
				if(Condition.CustomCondition == nullptr)
				{
					Context.AddWarning(FText::Format(INVTEXT("Null Custom Condition on node {0} ({1})."),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString())));
					continue;
				};
				for (const auto& InvalidEnumProp : GetAllEnumPropertiesAsStrings(Condition.CustomCondition, "_MAX"))
				{
					Context.AddWarning(FText::Format(INVTEXT("Invalid Enum Property {0} ({1}) on Custom Condition {2} (Node {3} ({4}))."),
						FText::FromString(InvalidEnumProp.Key.ToString()),
						FText::FromString(InvalidEnumProp.Value),
						FText::FromString(Condition.CustomCondition->GetName()),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString())
						));
				}
			}
			//if want to check any class variable
			else if (Condition.ConditionType == EDlgConditionType::ClassBoolVariable ||
				Condition.ConditionType == EDlgConditionType::ClassFloatVariable ||
				Condition.ConditionType == EDlgConditionType::ClassIntVariable ||
				Condition.ConditionType == EDlgConditionType::ClassNameVariable)
			{
				auto Class = GetParticipantClass(Condition.ParticipantName);
				if(Class && !Class->FindPropertyByName(Condition.CallbackName))
				{
					Context.AddWarning(FText::Format(INVTEXT("Property \"{0}\" doesn't exist in class {1} on Condition {2} (Node {3} ({4}))."),
						FText::FromString(Condition.CallbackName.ToString()),
						FText::FromString(Class->GetName()),
						FText::FromString(Condition.ConditionTypeToString(Condition.ConditionType)),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString())
					));
				}
			}

		}

		//Check Enter Events
		TArray<FDlgEvent> EnterEvents = Node->GetNodeEnterEvents();
		for (const FDlgEvent& Event : EnterEvents)
		{

			if (Event.CustomEvent)
			{
				Event.CustomEvent->IsDataValid(Context);

				for (const auto& InvalidEnumProp : GetAllEnumPropertiesAsStrings(Event.CustomEvent, "_MAX"))
				{
					Context.AddWarning(FText::Format(INVTEXT("Invalid Enum Property {0} ({1}) on Custom Event {2} (Node {3} ({4}))."),
						FText::FromString(InvalidEnumProp.Key.ToString()),
						FText::FromString(InvalidEnumProp.Value),
						FText::FromString(Event.CustomEvent->GetName()),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString())
						));
				}
			}
			else if (Event.EventType == EDlgEventType::Custom)
			{
				Context.AddWarning(FText::Format(INVTEXT("Custom Event is null on Node {0} ({1})."),
					GetNodeIndexForGUID(Node->GetGUID()),
					FText::FromString(Node->GetGUID().ToString())
				));
			}
			else if (Event.EventType == EDlgEventType::ModifyClassBoolVariable ||
				Event.EventType == EDlgEventType::ModifyClassFloatVariable ||
				Event.EventType == EDlgEventType::ModifyClassIntVariable ||
				Event.EventType == EDlgEventType::ModifyClassNameVariable)
			{
				auto Class = GetParticipantClass(Event.ParticipantName);
				if(Class)
				{

				}
				if(Class && (!Class->FindPropertyByName(Event.EventName) && !Class->FindFunctionByName(Event.EventName)))
				{
					Context.AddWarning(FText::Format(INVTEXT("Property (or event) \"{0}\" doesn't exist in class {1} on Event {2} (Node {3} ({4}))."),
						FText::FromString(Event.EventName.ToString()),
						FText::FromString(Class->GetName()),
						FText::FromString(Event.EventTypeToString(Event.EventType)),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString())
					));
				}
			}

		}

		//Check edges
		for(const FDlgEdge& Edge : Node->GetNodeChildren())
		{
			const auto EnumString = UEnum::GetValueAsString(Edge.EdgeIntend);
			if(EnumString.Contains("_MAX", ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				Context.AddWarning(FText::Format(INVTEXT("Invalid Intend on edge from {0} to {1}."),
					GetNodeIndexForGUID(Node->GetGUID()),
					Edge.TargetIndex));
			}
			for (const auto& InvalidEnumProp : GetAllEnumPropertiesAsStrings(Edge.EdgeData, "_MAX"))
			{
				Context.AddWarning(FText::Format(INVTEXT("Invalid Enum Property {0} ({1}) on edge from {2} (Node {3} ({4}))."),
					FText::FromString(InvalidEnumProp.Key.ToString()),
					FText::FromString(InvalidEnumProp.Value),
					FText::FromString(Edge.EdgeData->GetName()),
					GetNodeIndexForGUID(Node->GetGUID()),
					FText::FromString(Node->GetGUID().ToString())
					));
			}

			if (Edge.IsTextVisible(*Node) && !Edge.GetText().IsEmpty() && Edge.GetText().ToString() != "Finish")
			{
				if (!Edge.GetText().ShouldGatherForLocalization())
				{
					FText ErrorText= FText::Format(INVTEXT("NOT LOCALIZABLE text on edge from {0} (Node {1} ({2}))."),
						FText::FromString(Edge.EdgeData ? Edge.EdgeData->GetName() : "Edge data invalid"),
						GetNodeIndexForGUID(Node->GetGUID()),
						FText::FromString(Node->GetGUID().ToString()));
					// Context.AddError();

					TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
					Message->AddToken(FTextToken::Create(ErrorText));
					Message->AddToken(FActionToken::Create(
						FText::FromString("Open dialogue"),
						FText::FromString(""),
						FOnActionTokenExecuted::CreateLambda([&]()
							{
								GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset((this));
								//todo: open exact node
							})
						));

					Context.AddMessage(Message);

				}


			}

			// if (!Edge.EdgeData)
			// {
			// 	Context.AddWarning(FText::Format(INVTEXT("Edge data Invalid (Node {0} ({1}))."),
			// 		GetNodeIndexForGUID(Node->GetGUID()),
			// 		FText::FromString(Node->GetGUID().ToString())
			// 	));
			// }
		}
	}
	return (Context.GetNumErrors() > 0 || Context.GetNumWarnings() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#undef LOCTEXT_NAMESPACE
#endif


