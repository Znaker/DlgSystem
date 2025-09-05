// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgNode_Start.h"

#include "TallProject/JsonLoader/JsonLoader.h"
#include "UObject/AssetRegistryTagsContext.h"

FString UDlgNode_Start::GetDesc()
{
	return TEXT("Possible entry point.\nDoes not have text, the first satisfied child is picked if there is any.\nStart nodes are evaluated from left to right.");
}

void UDlgNode_Start::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	//JsonLoader::SaveAssetDataValuesArrayToJsonFile("DlgStartBranches", "BranchTag", { BranchTag.ToString() });
	Context.AddTag(FAssetRegistryTag("BranchTag", BranchTag.ToString(), FAssetRegistryTag::TT_Alphabetical));
}
