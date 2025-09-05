// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgNode_End.h"

FString UDlgNode_End::GetDesc()
{
	return TEXT("Node ending the Dialogue.\nDoes not have text, if it is entered the Dialogue is over.\nEvents and enter conditions are taken into account.");
}
void UDlgNode_End::PostInitProperties()
{
	Super::PostInitProperties();

	fTimeToNextSpeech = 0.f;
}

#if WITH_EDITOR
void UDlgNode_End::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
