// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sterio_4_16GameMode.h"
#include "Sterio_4_16Character.h"
#include "UObject/ConstructorHelpers.h"

ASterio_4_16GameMode::ASterio_4_16GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
