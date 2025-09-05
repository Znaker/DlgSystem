#pragma once

UENUM(BlueprintType)
enum class EPlayerAnswerIntend : uint8
{
	BINT_Default	=0			UMETA(DisplayName = "Default", ToolTip="Using in base dialogue for default options or as dialogue classifier for not bits"),
	BINT_Any					UMETA(DisplayName = "Any", ToolTip="Intercepts any phrase, with exceptions"),
	BINT_Rest					UMETA(DisplayName = "Rest", ToolTip="React for no sense player answers, if others option check failed"),
	BINT_Answer					UMETA(DisplayName = "Answer", ToolTip="For custom option"),
	BINT_Agree					UMETA(DisplayName = "Agree"),
	BINT_Refuse					UMETA(DisplayName = "Refuse"),
	BINT_Flirt					UMETA(DisplayName = "Flirt"),
	BINT_Insult					UMETA(DisplayName = "Insult"),
	BINT_Question				UMETA(DisplayName = "Question"),
	BINT_Apology    			UMETA(DisplayName = "Apology"),
	BINT_Please    				UMETA(DisplayName = "Please"),
	BINT_Leave					UMETA(DisplayName = "Leave"),
	BINT_Return					UMETA(DisplayName = "Return"),
	BINT_Fatal					UMETA(DisplayName = "Fatal"),
};

