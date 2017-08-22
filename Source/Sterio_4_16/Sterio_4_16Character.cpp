// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sterio_4_16Character.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// ASterio_4_16Character

ASterio_4_16Character::ASterio_4_16Character()
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

	// sterio cam render
	LeftCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftCam"));
	LeftCam->SetupAttachment(CameraBoom);
	RightCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RightCam"));
	RightCam->SetupAttachment(CameraBoom);

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void ASterio_4_16Character::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ASterio_4_16Character::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASterio_4_16Character::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ASterio_4_16Character::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ASterio_4_16Character::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ASterio_4_16Character::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ASterio_4_16Character::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ASterio_4_16Character::OnResetVR);
}


void ASterio_4_16Character::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ASterio_4_16Character::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void ASterio_4_16Character::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void ASterio_4_16Character::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ASterio_4_16Character::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ASterio_4_16Character::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ASterio_4_16Character::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

static FMatrix MagicFix(FMatrix& matrix)
{
	FMatrix result(matrix);

	//result.M[1][2] = 0.0f;
	result.M[2][3] = 1.0f;
	result.M[2][2] = 0.0f;
	result.M[3][0] = 0.0f;
	result.M[3][1] = 0.0f;

	result.M[2][2] = 0.0f;
	result.M[3][0] = 0.0f;
	result.M[3][1] = 0.0f;

	result *= 1.0f / result.M[0][0];
	result.M[3][2] = GNearClippingPlane;

	return result;
}

static FMatrix FrustumMatrix(float left, float right, float bottom, float top, float nearVal, float farVal)
{
	FMatrix Result;
	Result.SetIdentity();
	Result.M[0][0] = (2.0f * nearVal) / (right - left);
	Result.M[1][1] = (2.0f * nearVal) / (top - bottom);
	Result.M[2][0] = -(right + left) / (right - left);
	Result.M[2][1] = -(top + bottom) / (top - bottom);
	Result.M[2][2] = farVal / (farVal - nearVal);
	Result.M[2][3] = 1.0f;
	Result.M[3][2] = -(farVal * nearVal) / (farVal - nearVal);
	Result.M[3][3] = 0;

	return Result;
}

static FMatrix GenerateOffAxisMatrix_Internal(float ScreenWidth, float ScreenHeight, const FVector& wcsPtHead)
{
	float heightFromWidth = ScreenHeight / ScreenWidth;

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.VisageHeadTracker.RealScreenWidth"));
	float wcsWidth = CVar ? CVar->GetValueOnAnyThread() : 520.0f; //(ScreenWidth / 75.0f) * 2.54f; // DPI to cm 

	float wcsHeight = heightFromWidth * wcsWidth;

	FVector wcsPtTopLeftScreen(-wcsWidth / 2.f, -wcsHeight / 2.f, 0);
	FVector wcsPtBottomRightScreen(wcsWidth / 2.f, wcsHeight / 2.f, 0);
	float camZNear = GNearClippingPlane;
	float wcsZFar = 12000.0f;

	FVector camPtTopLeftScreen = wcsPtTopLeftScreen - wcsPtHead;
	FVector camPtTopLeftNear = camZNear / camPtTopLeftScreen.Z * camPtTopLeftScreen;
	FVector camPtBottomRightScreen = wcsPtBottomRightScreen - wcsPtHead;
	FVector camPtBottomRightNear = camPtBottomRightScreen / camPtBottomRightScreen.Z * camZNear;
	float camZFar = wcsZFar - wcsPtHead.Z;

	FMatrix OffAxisProjectionMatrix = FrustumMatrix(camPtTopLeftNear.X, camPtBottomRightNear.X,
		-camPtBottomRightNear.Y, -camPtTopLeftNear.Y, camZNear, camZFar);

	FMatrix matFlipZ;
	matFlipZ.SetIdentity();
	matFlipZ.M[2][2] = -1.0f;
	matFlipZ.M[3][2] = 1.0f;

	FMatrix result = FScaleMatrix(FVector(1, -1, 1)) *
		FTranslationMatrix(-wcsPtHead) *
		FScaleMatrix(FVector(1, -1, 1)) *
		OffAxisProjectionMatrix *
		matFlipZ;


	//result.M[2][2] = 0.0f;
	//result.M[3][0] = 0.0f;
	result.M[3][1] = 0.0f;

	result *= 1.0f / result.M[0][0];
	result.M[3][2] = GNearClippingPlane;

	return result;
}

FMatrix GenerateOffAxisMatrix(float ScreenWidth, float ScreenHeight, FVector eyePosition)
{
	float headX = eyePosition.X / ScreenHeight;
	float headY = eyePosition.Y / ScreenHeight;
	float headDist = eyePosition.Z / ScreenHeight;
	float screenAspect = ScreenWidth / ScreenHeight;

	float nearPlane = GNearClippingPlane;
	FMatrix resultM = FrustumMatrix(nearPlane*(-.5f * screenAspect - headX) / headDist,
		nearPlane*(.5f * screenAspect - headX) / headDist,
		nearPlane*(-.5f + headY) / headDist,
		nearPlane*(.5f + headY) / headDist,
		nearPlane, 1000.0f);

	return MagicFix(resultM);

	return GenerateOffAxisMatrix_Internal(ScreenWidth, ScreenHeight, eyePosition);
}

static FMatrix _AdjustProjectionMatrixForRHI(const FMatrix& InProjectionMatrix)
{
	const float GMinClipZ = 0.0f;
	const float GProjectionSignY = 1.0f;

	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, GProjectionSignY, 1.0f - GMinClipZ));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, GMinClipZ));
	return InProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate;
}

void ASterio_4_16Character::BeginPlay()
{
	Super::BeginPlay();
	LeftCam->bUseCustomProjectionMatrix = true;
	RightCam->bUseCustomProjectionMatrix = true;

}


void ASterio_4_16Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FMatrix GenerateOffAxisMatrix_L = GenerateOffAxisMatrix(160.f, 100.f, FVector(-2.f, 0.f, -161.f));
	FMatrix GenerateOffAxisMatrix_R = GenerateOffAxisMatrix(160.f, 100.f, FVector(2.f, 0.f, -159.f));

	UE_LOG(LogTemp, Warning, TEXT("GenerateOffAxisMatrix_L %s"), *GenerateOffAxisMatrix_L.ToString());
	UE_LOG(LogTemp, Warning, TEXT("GenerateOffAxisMatrix_R %s"), *GenerateOffAxisMatrix_R.ToString());

	UE_LOG(LogTemp, Warning, TEXT("MagicFix_R %s"), *MagicFix(GenerateOffAxisMatrix_L).ToString());
	UE_LOG(LogTemp, Warning, TEXT("MagicFix_R %s"), *MagicFix(GenerateOffAxisMatrix_R).ToString());

	LeftCam->CustomProjectionMatrix = MagicFix(GenerateOffAxisMatrix_L);
	RightCam->CustomProjectionMatrix = MagicFix(GenerateOffAxisMatrix_R);
}