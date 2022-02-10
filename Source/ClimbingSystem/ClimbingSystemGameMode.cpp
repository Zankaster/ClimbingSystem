// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemGameMode.h"
#include "ClimbingSystemCharacter.h"
#include "UObject/ConstructorHelpers.h"

AClimbingSystemGameMode::AClimbingSystemGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
