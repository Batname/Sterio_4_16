// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Sterio_4_16Character.generated.h"

UCLASS(config=Game)
class ASterio_4_16Character : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	ASterio_4_16Character();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

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

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera)
	USceneCaptureComponent2D* LeftCam;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Camera)
	USceneCaptureComponent2D* RightCam;


	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	FVector LeftEye = FVector(2.f, 1.f, 161.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	FVector RightEye = FVector(-2.f, 1.f, 159.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	float width = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	float height = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	float NearClipPlane= 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
    float FarClipPlane= 600000.f;


	FMatrix GeneralizedPerspectiveProjection(FVector pe);
	FMatrix GeneralizedPerspectiveProjection1(FVector pe);


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
	float M_0_0 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_0_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_0_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_0_3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_1_0 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_1_1 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_1_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_1_3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_2_0 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_2_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_2_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_2_3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_3_0 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_3_1 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_3_2 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SterioCam)
		float M_3_3 = 0.0f;
};

