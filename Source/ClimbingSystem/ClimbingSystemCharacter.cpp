// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingSystemCharacter.h"

#include "DrawDebugHelpers.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

//////////////////////////////////////////////////////////////////////////
// AClimbingSystemCharacter

AClimbingSystemCharacter::AClimbingSystemCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Set the allowed angles for climbing
	MinClimbAngle = -75.f;
	MaxClimbAngle = 45.f;
	MaxTurnAngle = 65.f;
	bClimbing = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void AClimbingSystemCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Delegates initialization
	// IMPORTANT: HAS TO BE DONE HERE, INSIDE THE CONSTRUCTOR CAUSES ERRORS!
	LandedDelegate.AddDynamic(this, &AClimbingSystemCharacter::CharacterOnLand);
	OnReachedJumpApex.AddDynamic(this, &AClimbingSystemCharacter::CharacterReachedJumpApex);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimbingSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AClimbingSystemCharacter::JumpStart);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AClimbingSystemCharacter::JumpStop);

	PlayerInputComponent->BindAxis("MoveForward", this, &AClimbingSystemCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AClimbingSystemCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AClimbingSystemCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AClimbingSystemCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AClimbingSystemCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AClimbingSystemCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AClimbingSystemCharacter::OnResetVR);

	// Handle wall grab/drop
	PlayerInputComponent->BindAction("GrabWall", IE_Pressed, this, &AClimbingSystemCharacter::HandleWallGrab);
}

void AClimbingSystemCharacter::AttachToWall()
{
	// Check if there is a wall in front of the character
	FHitResult WallTraceHitResult;
	const FVector Start = GetCapsuleComponent()->GetComponentLocation();
	const FVector End = Start + (GetCapsuleComponent()->GetScaledCapsuleRadius()*3.f*GetActorForwardVector());
	if (!WallTrace(WallTraceHitResult, Start, End, FColor::Green))
	{
		// If there is not a wall in front of the character, make sure to detach
		DetachFromWall();
		return;
	}

	// There is a wall in front of the character, set movement mode to Flying,
	// stop movement and stop orienting the rotation to the movement
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->bOrientRotationToMovement = false;

	const FVector TargetPosition =
		GetCapsuleComponent()->GetScaledCapsuleRadius() * WallTraceHitResult.Normal + WallTraceHitResult.Location;
	const FRotator TargetRotation =
		UKismetMathLibrary::MakeRotFromX(-WallTraceHitResult.Normal);

	FLatentActionInfo LatentInfo;
	LatentInfo.CallbackTarget = this;

	// Move the character to the wall
	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), TargetPosition, TargetRotation,
	                                      false, false, 0.2f, false, EMoveComponentAction::Move, LatentInfo);
}

void AClimbingSystemCharacter::DetachFromWall()
{
	bClimbing = false;
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
	{
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	}

	GetCharacterMovement()->bOrientRotationToMovement = true;
	ResetRotation();
}

void AClimbingSystemCharacter::HandleWallGrab()
{
	const EMovementMode MovementMode = GetCharacterMovement()->MovementMode;

	// If the character is not attached to a wall, try to find one, else detach from it
	if (MovementMode == EMovementMode::MOVE_Walking || MovementMode == EMovementMode::MOVE_Falling)
	{
		AttachToWall();
	}
	else if (MovementMode == EMovementMode::MOVE_Flying)
	{
		DetachFromWall();
	}
}

bool AClimbingSystemCharacter::WallTrace(FHitResult& OutHit, FVector Start, FVector End, FColor Color)
{
	DrawDebugLine(GetWorld(), Start, End, Color, false, 0.3f, 0, 2.f);
	return GetWorld()->SweepSingleByChannel(
			OutHit, Start, End,
			FQuat::Identity, ECollisionChannel::ECC_Visibility,
			FCollisionShape::MakeBox(FVector(0.01f, 0.01f, 0.01f)));
}

void AClimbingSystemCharacter::ResetRotation()
{
	// Need to add a gradual interpolation, with a timer or something like that
	const FRotator Rotation = GetActorRotation();
	SetActorRotation(FRotator{0.f, Rotation.Yaw, Rotation.Roll}, ETeleportType::None);
}

void AClimbingSystemCharacter::WallClimbMovement(float Axis, FVector Direction,bool bVertical)
{
	// If there is no input, there is no movement
	if (Axis == 0) return;

	float Offset = (bVertical ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*1.f : GetCapsuleComponent()->GetScaledCapsuleRadius()*1.5f);
	float RayLength = 80.f;
	
	// Do a boxcast in the direction of the character movement
	FVector Start1 = Offset * Direction * Axis + GetActorLocation();
	FVector End1 = Start1 + (GetCapsuleComponent()->GetScaledCapsuleRadius() * 2  * GetActorForwardVector());

	FVector Start2 = GetActorLocation();
	FVector End2 = Start2 + RayLength * GetActorForwardVector();

	bool bHit1, bHit2;
	FHitResult OutHit1, OutHit2;
	
	bHit1 = WallTrace(OutHit1, Start1, End1, FColor::Purple);
	
	bHit2 = WallTrace(OutHit2, Start2, End2, FColor::Red);
	
	if ( bHit1)
	{
		FVector N1 = OutHit1.Normal;
		FVector N2 = OutHit2.Normal;
		N1.Normalize();
		N2.Normalize();
		
		// The angle between the wall in front of the character, and the wall in the direction the
		// character is moving: if this is higher than MaxTurnAngle, movement is stopped
		float Angle1F = UKismetMathLibrary::Abs(
			UKismetMathLibrary::DegAcos(
				UKismetMathLibrary::Dot_VectorVector(
					N1, N2)));

		//UE_LOG(LogTemp, Warning, TEXT("Walls Angle1R: %f, 1 Angle1F: %f"), Angle1R, Angle1F);

		// Check for lateral movement maximum angle
		if(Angle1F > MaxTurnAngle)
			return;

		float ClimbAngle = UKismetMathLibrary::MakeRotFromX(-OutHit1.Normal).Pitch;

		if(ClimbAngle > MaxClimbAngle || ClimbAngle < MinClimbAngle)
		{
			UE_LOG(LogTemp, Warning, TEXT("Climb Angle Invalid: %f"), ClimbAngle);
			return;
		}

		FVector Target = OutHit1.Location - GetActorLocation();

		// Finally movement and rotation can be applied
		FVector WorldDirection =
			UKismetMathLibrary::GetDirectionUnitVector(
				GetActorLocation(),
				OutHit1.Normal * GetCapsuleComponent()->GetScaledCapsuleRadius() + OutHit1.Location)
			* UKismetMathLibrary::SignOfFloat(Axis);

		// Final movement added to character
		AddMovementInput(WorldDirection, Axis);

		// Also character rotation is adjusted
		FRotator TargetWallRotation;
		TargetWallRotation = UKismetMathLibrary::MakeRotFromX(-OutHit1.Normal);
		const FRotator NewRotation = UKismetMathLibrary::RInterpTo(
			GetActorRotation(), TargetWallRotation, GetWorld()->GetDeltaSeconds(),
			5.f);
		SetActorRotation(NewRotation, ETeleportType::None);
		bClimbing = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Wall not found!"));
		// Vault?
		// Check if the character can vault up
		// Is the input upwards relative to character?
		if(Direction == GetActorUpVector() && Axis > 0 && bClimbing)
		{
			Start1 = GetActorUpVector() * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2.f )+ GetActorLocation()
			+GetActorForwardVector()*GetCapsuleComponent()->GetScaledCapsuleRadius()*2.f;
			End1 = Start1;
			FHitResult OutHitVault;
			// If this capsule cast with the size of the character, does not find anything above it, the character will climb on top
			DrawDebugLine(GetWorld(), Start1, End1, FColor::Green, false, 0.3f, 0, 2.f);
			DrawDebugCapsule(GetWorld(),Start1,GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), GetCapsuleComponent()->GetScaledCapsuleRadius(),
				FQuat::Identity,FColor::Emerald, false,2.f,3,1.f);
			if(!GetWorld()->SweepSingleByChannel(OutHitVault,Start1,End1,FQuat::Identity, ECollisionChannel::ECC_Visibility,
								 FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight())))
			{
				FVector FirstLoc, SecondLoc;
				// SecondLoc is basically the position that has been checked to be free, where the character
				// can move, while FirstLoc is that same position but with a small offset backwards, so that
				// in VaulUp(), the character is moved first "Up", and then a bit forward
				SecondLoc = Start1+ GetActorForwardVector()*30.f;
				FirstLoc = SecondLoc +GetCapsuleComponent()->GetScaledCapsuleRadius() * -2.f * GetActorForwardVector();
				VaultUp(FirstLoc, SecondLoc);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Can't vault"));
				return;
			}
		}
	}
}

void AClimbingSystemCharacter::JumpUpWall()
{
	// Set movement mode to walking to use gravity
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);

	// Launch character up to simulate a jump
	LaunchCharacter(GetActorUpVector() * 600.f, false, false);

	// Only when the character traveled the max distance, will be able to reattach to a wall
	bCheckForApex = true;
}

void AClimbingSystemCharacter::VaultUp(FVector FirstLoc, FVector SecondLoc)
{
	DetachFromWall();
	FLatentActionInfo LatentInfo;
	LatentInfo.CallbackTarget = this;

	// Move the character up, and then a bit forward to simulate the vault

	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), FirstLoc, GetActorRotation(),
	                                      false, true, 0.5f, false, EMoveComponentAction::Move, LatentInfo);

	/*UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), SecondLoc, GetActorRotation(),
	                                      false, true, 0.35f, false, EMoveComponentAction::Move, LatentInfo);*/
}

void AClimbingSystemCharacter::JumpStart()
{
	GetCharacterMovement()->bNotifyApex = true;
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Walking)
	{
		ACharacter::Jump();
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
	{
		JumpUpWall();
	}
}

void AClimbingSystemCharacter::JumpStop()
{
	ACharacter::StopJumping();
}

void AClimbingSystemCharacter::OnResetVR()
{
	// If ClimbingSystem is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in ClimbingSystem.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AClimbingSystemCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AClimbingSystemCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AClimbingSystemCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AClimbingSystemCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AClimbingSystemCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// Check if normal movement or climbing
		const EMovementMode MovementMode = GetCharacterMovement()->MovementMode;
		if (MovementMode == EMovementMode::MOVE_Walking || MovementMode == EMovementMode::MOVE_Falling)
		{
			AddMovementInput(Direction, Value);
		}
		else if (MovementMode == EMovementMode::MOVE_Flying)
		{
			WallClimbMovement(Value, GetActorUpVector(), true);
		}
	}
}

void AClimbingSystemCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Check if normal movement or climbing
		const EMovementMode MovementMode = GetCharacterMovement()->MovementMode;
		if (MovementMode == EMovementMode::MOVE_Walking || MovementMode == EMovementMode::MOVE_Falling)
		{
			AddMovementInput(Direction, Value);
		}
		else if (MovementMode == EMovementMode::MOVE_Flying)
		{
			WallClimbMovement(Value, GetActorRightVector(), false);
		}
	}
}

void AClimbingSystemCharacter::CharacterReachedJumpApex()
{
	if (bCheckForApex)
	{
		AttachToWall();
	}
}

void AClimbingSystemCharacter::CharacterOnLand(const FHitResult& Hit)
{
	bCheckForApex = false;
}
