// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ClimbingSystemCharacter.generated.h"

UCLASS(config=Game)
class AClimbingSystemCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	AClimbingSystemCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Min angle allowed to be able to climb while moving on walls */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=Movement, meta=(AllowPrivateAccess = "true"))
	float MinClimbAngle;

	/** Max angle allowed to be able to climb while moving on walls */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=Movement, meta=(AllowPrivateAccess = "true"))
	float MaxClimbAngle;
	
	/** Max angle allowed to be able to turn while moving on walls */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=Movement, meta=(AllowPrivateAccess = "true"))
	float MaxTurnAngle;

protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

	virtual void BeginPlay() override;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
	
	/** Try to attach the character to the wall */
	void AttachToWall();

	/** Detaches the character from the wall if he's climbing */
	void DetachFromWall();

	/** Handles the GrabWall button press */
	void HandleWallGrab();

	/** Performs a box trace in front of the character */
	bool WallTrace(FHitResult& OutHit, FVector Start, FVector End, FColor Color);

	/** Resets character Pitch when dropping from a wall */
	void ResetRotation();

	/** Engine of the climbing system movement
	 * @param Axis Ranges from -1 to 1, it is the Axis input
	 * @param Direction Vector specifying which direction the character is moving
	 * @param bVertical True when evaluating vertical movement, false otherwise
	 */
	void WallClimbMovement(float Axis, FVector Direction, bool bVertical);

	/** Used to jump while climbing a wall */
	void JumpUpWall();

	/** When the character reaches the top of a wall, if there is enough
	 *  space he vaults on top, with a first movement straight up and
	 *  then a second movement forward
	 *  @param FirstLoc First movement location, usually just upwards
	 *  @param SecondLoc Second movement location, usually it is equal to FirstLoc
	 *  plus a small forward offset
	 */ 
	void VaultUp(FVector FirstLoc, FVector SecondLoc);

	/** Handles jump mechanics, with different behavior if the character is walking
	 *  or if he is climbing a wall */
	void JumpStart();

	/** Handles jump stop mechanics. But at the moment it just calls the normal StopJumping()
	 *  from ACharacter */
	void JumpStop();

	/** Called by OnReachedJumpApex when character is at maximum height in a jump,
	 *  it is used to make the character grab a wall if he was climbing
	 *  before the jump */
	UFUNCTION()
	void CharacterReachedJumpApex();

	/** Called by LandedDelegate when the character lands. It is used to
	 *  reset the boolean that triggers the delegate for jump apex, bCheckForApex */
	UFUNCTION()
	void CharacterOnLand(const FHitResult& Hit);
	
	/** Should a delegate be triggered when the character reaches jump apex? */
	bool bCheckForApex;

	/** Is the character climbing? This is set to true after the character attached
	 *  to a wall and did some movement, to avoid he tries to climb when he just
	 *  attached */
	bool bClimbing;

};

