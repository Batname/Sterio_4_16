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

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

												   // sterio cam render
	LeftCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftCam"));
	LeftCam->SetupAttachment(FollowCamera);
	RightCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RightCam"));
	RightCam->SetupAttachment(FollowCamera);

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

static FMatrix MagicFix(FMatrix matrix)
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
	Result.M[2][0] = (right + left) / (right - left);
	Result.M[2][1] = (top + bottom) / (top - bottom);
	Result.M[2][2] = -farVal / (farVal - nearVal);
	Result.M[2][3] = -1.0f;
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


	result.M[2][2] = 0.0f;
	result.M[3][0] = 0.0f;
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

	//return GenerateOffAxisMatrix_Internal(ScreenWidth, ScreenHeight, eyePosition);
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

static FMatrix PerspectiveOffCenter(float left, float right, float bottom, float top, float near, float far)
{
	float x = 2.0F * near / (right - left);
	float y = 2.0F * near / (top - bottom);
	float a = (right + left) / (right - left);
	float b = (top + bottom) / (top - bottom);
	float c = -(far + near) / (far - near);
	float d = -(2.0F * far * near) / (far - near);
	float e = -1.0F;

	FMatrix M;
	M.SetIdentity();

	M.M[0][0] = x;
	M.M[0][1] = 0;
	M.M[0][2] = a;
	M.M[0][3] = 0;
	M.M[1][0] = 0;
	M.M[1][1] = y;
	M.M[1][2] = b;
	M.M[1][3] = 0;
	M.M[2][0] = 0;
	M.M[2][1] = 0;
	M.M[2][2] = c;
	M.M[2][3] = d;
	M.M[3][0] = 0;
	M.M[3][1] = 0;
	M.M[3][2] = -e;
	M.M[3][3] = 0;
	return M;
}

FMatrix ASterio_4_16Character::GeneralizedPerspectiveProjection(FVector pe)
{
	// 	pe.Z = -pe.Z;

	float zoff = 0.0f;// EyeLeft.z/10.0f;
	FVector pa = FVector(-width / 2.0f, -height / 2.0f, -zoff);
	FVector pb = FVector(width / 2.0f, -height / 2.0f, -zoff);
	FVector pc = FVector(-width / 2.0f, height / 2.0f, -zoff);

	FVector va, vb, vc;
	FVector vr, vu, vn;

	float left, right, bottom, top, eyedistance;

	FMatrix transformMatrix;
	FMatrix projectionM;
	FMatrix eyeTranslateM;
	FMatrix finalProjection;

	///Calculate the orthonormal for the screen (the screen coordinate system
	vr = pb - pa;
	vr.Normalize();
	vu = pc - pa;
	vu.Normalize();
	vn = FVector::CrossProduct(vr, vu);
	vn.Normalize();

	//Calculate the vector from eye (pe) to screen corners (pa, pb, pc)
	va = pa - pe;
	vb = pb - pe;
	vc = pc - pe;

	//Get the distance;; from the eye to the screen plane
	eyedistance = -(FVector::DotProduct(va, vn));

	//Get the varaibles for the off center projection
	left = (FVector::DotProduct(vr, va) * NearClipPlane) / eyedistance;
	right = (FVector::DotProduct(vr, vb) * NearClipPlane) / eyedistance;
	bottom = (FVector::DotProduct(vu, va) * NearClipPlane) / eyedistance;
	top = (FVector::DotProduct(vu, vc) * NearClipPlane) / eyedistance;

	//Get this projection
	projectionM = PerspectiveOffCenter(left, right, bottom, top, NearClipPlane, FarClipPlane);

	//Fill in the transform matrix
	transformMatrix.M[0][0] = vr.X;
	transformMatrix.M[0][1] = vr.Y;
	transformMatrix.M[0][2] = vr.Z;
	transformMatrix.M[0][3] = 0.0f;
	transformMatrix.M[1][0] = vu.X;
	transformMatrix.M[1][1] = vu.Y;
	transformMatrix.M[1][2] = vu.Z;
	transformMatrix.M[1][3] = 0.0f;
	transformMatrix.M[2][0] = vn.X;
	transformMatrix.M[2][1] = vn.Y;
	transformMatrix.M[2][2] = vn.Z;
	transformMatrix.M[2][3] = 0.0f;
	transformMatrix.M[3][0] = 0.0f;
	transformMatrix.M[3][1] = 0.0f;
	transformMatrix.M[3][2] = 0.0f;
	transformMatrix.M[3][3] = 1.0f;

	//Now for the eye transform
	eyeTranslateM.M[0][0] = 1.0f;
	eyeTranslateM.M[0][1] = 0.0f;
	eyeTranslateM.M[0][2] = 0.0f;
	eyeTranslateM.M[0][3] = -pe.X;
	eyeTranslateM.M[1][0] = 0.0f;
	eyeTranslateM.M[1][1] = 1.0f;
	eyeTranslateM.M[1][2] = 0.0f;
	eyeTranslateM.M[1][3] = -pe.Y;
	eyeTranslateM.M[2][0] = 0.0f;
	eyeTranslateM.M[2][1] = 0.0f;
	eyeTranslateM.M[2][2] = 1.0f;
	eyeTranslateM.M[2][3] = -pe.Z;
	eyeTranslateM.M[3][0] = 0.0f;
	eyeTranslateM.M[3][1] = 0.0f;
	eyeTranslateM.M[3][2] = 0.0f;
	eyeTranslateM.M[3][3] = 1.0f;

	//Multiply all together
	finalProjection = projectionM * transformMatrix;//eyeTranslateM;

	UE_LOG(LogTemp, Warning, TEXT("finalProjection %s"), *finalProjection.ToString());

	return finalProjection;
}

FMatrix ASterio_4_16Character::TransformToUEProjection(FMatrix M)
{
	FMatrix Result = M;

	Result.M[2][3] = 1.f; // Rotate z location
	Result.M[2][2] = 0.0f; // min z to 0

	// https://github.com/EpicGames/UnrealEngine/pull/912/commits/0b6058a78ebb8304efaa22b7e0bf3b554bcd0b29#diff-cf8bd908a6763f87c070608b9ee0376bR128
	// Transpose x znd y in matrix, it possible to get blur effect
	// Maybe it better to set
	//Result.M[0][2] = 0.0f; // remove xy from final matrix
	//Result.M[1][2] = 0.0f; // remove xy from final matrix

	float temp1 = M.M[0][2];
	float temp2 = M.M[1][2];

	Result.M[0][2] = Result.M[2][0];
	Result.M[1][2] = Result.M[2][1];

	Result.M[2][0] = temp1;
	Result.M[2][1] = temp2;

	Result = Result * (1.0f / Result.M[0][0]); // COnvert to UE matrix format
	Result.M[2][3] = 1.0f - Result.M[2][3]; // Flip z scale
	Result.M[3][2] = NearClipPlane;

	/*
		matrix format
		return FMatrix(
			FPlane(XS, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, YS, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, 0.0f, 1.0f),
			FPlane(0.0f, 0.0f, NearZ, 0.0f));
	*/


	Result.M[0][0] += M_0_0;
	Result.M[0][1] += M_0_1;
	Result.M[0][2] += M_0_2;
	Result.M[0][3] += M_0_3;

	Result.M[1][0] += M_1_0;
	Result.M[1][1] += M_1_1;
	Result.M[1][2] += M_1_2;
	Result.M[1][3] += M_1_3;

	Result.M[2][0] += M_2_0;
	Result.M[2][1] += M_2_1;
	Result.M[2][2] += M_2_2;
	Result.M[2][3] += M_2_3;

	Result.M[3][0] += M_3_0;
	Result.M[3][1] += M_3_1;
	Result.M[3][2] += M_3_2;
	Result.M[3][3] += M_3_3;

	return Result;
}


void ASterio_4_16Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FMatrix FinalProjectionMatrix_L = TransformToUEProjection(GeneralizedPerspectiveProjection(LeftEye));
	FMatrix GenerateOffAxisMatrix_R = TransformToUEProjection(GeneralizedPerspectiveProjection(RightEye));

	LeftCam->SetRelativeLocation(FVector(-LeftEye.Z, LeftEye.X, LeftEye.Y));
	RightCam->SetRelativeLocation(FVector(-RightEye.Z, RightEye.X, RightEye.Y));

	LeftCam->CustomProjectionMatrix = FinalProjectionMatrix_L;
	RightCam->CustomProjectionMatrix = GenerateOffAxisMatrix_R;

	UE_LOG(LogTemp, Warning, TEXT("LeftCam->CustomProjectionMatrix l %s"), *LeftCam->CustomProjectionMatrix.ToString());
	UE_LOG(LogTemp, Warning, TEXT("MagicFix_R 2 %s"), *RightCam->CustomProjectionMatrix.ToString());
}