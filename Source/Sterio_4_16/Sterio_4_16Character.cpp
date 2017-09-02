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

void ASterio_4_16Character::BeginPlay()
{
	Super::BeginPlay();
	LeftCam->bUseCustomProjectionMatrix = true;
	RightCam->bUseCustomProjectionMatrix = true;
}

FMatrix ASterio_4_16Character::GeneralizedPerspectiveProjection(FVector pe)
{
	float zoff = 0.0f;
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

	float SumRL = (right + left);
	float SumTB = (top + bottom);
	float InvRL = (1.0f / (right - left));
	float InvTB = (1.0f / (top - bottom));

	projectionM.M[0][0] = 2.f * NearClipPlane * InvRL * M_0_0; // Connect to Z
	projectionM.M[0][1] = 0.f;
	projectionM.M[0][2] = 0.f;
	projectionM.M[0][3] = 0.f + M_0_3;

	projectionM.M[1][0] = 0.f;
	projectionM.M[1][1] = 2.f * NearClipPlane * InvTB * M_1_1; // Connect to Z
	projectionM.M[1][2] = 0.f;
	projectionM.M[1][3] = 0.f + M_1_3;

	projectionM.M[2][0] = SumRL * InvRL;
	projectionM.M[2][1] = SumTB * InvTB;
	projectionM.M[2][2] = 0.0f;
	// I guess no.
	//projectionM.M[2][2] = -(SumFN * InvFN); // OpenGL
	//projectionM.M[2][2] = (NearClipPlane == FarClipPlane) ? 1.0f : FarClipPlane / (FarClipPlane - NearClipPlane); // FPerspectiveMatrix
	//projectionM.M[2][2] = ((NearClipPlane == FarClipPlane) ? 0.0f : NearClipPlane / (NearClipPlane - FarClipPlane)); // FReversedZPerspectiveMatrix
	//projectionM.M[2][3] = 1.f * pe.Z / 100.f;//-1.0f; My example connect to Z
	projectionM.M[2][3] = 1.0f;

	projectionM.M[3][0] = 0.f;
	projectionM.M[3][1] = 0.f;
	// I guess no.
	//projectionM.M[3][2] = -(2.f * MulFN * InvFN); // OpenGL
	//projectionM.M[3][2] = -NearClipPlane * ((NearClipPlane == FarClipPlane) ? 1.0f : FarClipPlane / (FarClipPlane - NearClipPlane)); // FPerspectiveMatrix
	//projectionM.M[3][2] = ((NearClipPlane == FarClipPlane) ? NearClipPlane : -FarClipPlane * NearClipPlane / (NearClipPlane - FarClipPlane));//FReversedZPerspectiveMatrix
	projectionM.M[3][2] = NearClipPlane;
	projectionM.M[3][3] = 0.f;

	return projectionM;
}

FMatrix ASterio_4_16Character::GeneralizedPerspectiveProjection1(FVector pe)
{
	float zoff = 0.0f;
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

	//UE_LOG(LogTemp, Warning, TEXT("left %f"), left);
	//UE_LOG(LogTemp, Warning, TEXT("right %f"), right);
	//UE_LOG(LogTemp, Warning, TEXT("bottom %f"), bottom);
	//UE_LOG(LogTemp, Warning, TEXT("top %f"), top);

	// Have to flip left/right and top/bottom to match UE4 expectations
	float Right = -FPlatformMath::Tan(left);
	float Left = -FPlatformMath::Tan(right);
	float Bottom = -FPlatformMath::Tan(top);
	float Top = -FPlatformMath::Tan(bottom);

	//UE_LOG(LogTemp, Warning, TEXT("Left %f"), Left);
	//UE_LOG(LogTemp, Warning, TEXT("Right %f"), Right);
	//UE_LOG(LogTemp, Warning, TEXT("Bottom %f"), Bottom);
	//UE_LOG(LogTemp, Warning, TEXT("Top %f"), Top);

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

	projectionM.M[0][0] = 2.f * NearClipPlane * InvRL * M_0_0; // Connect to Z
	projectionM.M[0][1] = 0.f;
	projectionM.M[0][2] = 0.f;
	projectionM.M[0][3] = 0.f + M_0_3;

	projectionM.M[1][0] = 0.f;
	projectionM.M[1][1] = 2.f * NearClipPlane * InvTB * M_1_1; // Connect to Z
	projectionM.M[1][2] = 0.f;
	projectionM.M[1][3] = 0.f + M_1_3;

	projectionM.M[2][0] = SumRL * InvRL;
	projectionM.M[2][1] = SumTB * InvTB;
	projectionM.M[2][2] = 0.0f;
	// I guess no.
	//projectionM.M[2][2] = -(SumFN * InvFN); // OpenGL
	//projectionM.M[2][2] = (NearClipPlane == FarClipPlane) ? 1.0f : FarClipPlane / (FarClipPlane - NearClipPlane); // FPerspectiveMatrix
	//projectionM.M[2][2] = ((NearClipPlane == FarClipPlane) ? 0.0f : NearClipPlane / (NearClipPlane - FarClipPlane)); // FReversedZPerspectiveMatrix
	//projectionM.M[2][3] = 1.f * pe.Z / 100.f;//-1.0f; My example connect to Z
	projectionM.M[2][3] = 1.0f;

	projectionM.M[3][0] = 0.f;
	projectionM.M[3][1] = 0.f;
	// I guess no.
	//projectionM.M[3][2] = -(2.f * MulFN * InvFN); // OpenGL
	//projectionM.M[3][2] = -NearClipPlane * ((NearClipPlane == FarClipPlane) ? 1.0f : FarClipPlane / (FarClipPlane - NearClipPlane)); // FPerspectiveMatrix
	//projectionM.M[3][2] = ((NearClipPlane == FarClipPlane) ? NearClipPlane : -FarClipPlane * NearClipPlane / (NearClipPlane - FarClipPlane));//FReversedZPerspectiveMatrix
	projectionM.M[3][2] = NearClipPlane;
	projectionM.M[3][3] = 0.f;

	return projectionM;
}

void ASterio_4_16Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FMatrix FinalProjectionMatrix_L = GeneralizedPerspectiveProjection(LeftEye);
	FMatrix GenerateOffAxisMatrix_R = GeneralizedPerspectiveProjection1(RightEye);

	LeftCam->SetRelativeLocation(FVector(-LeftEye.Z, LeftEye.X, LeftEye.Y));
	RightCam->SetRelativeLocation(FVector(-RightEye.Z, RightEye.X, RightEye.Y));

	LeftCam->CustomProjectionMatrix = FinalProjectionMatrix_L;
	RightCam->CustomProjectionMatrix = GenerateOffAxisMatrix_R;

	UE_LOG(LogTemp, Warning, TEXT("GeneralizedPerspectiveProjection(RightEye) %s"), *GeneralizedPerspectiveProjection(RightEye).ToString());
	UE_LOG(LogTemp, Warning, TEXT("GeneralizedPerspectiveProjection1(RightEye) %s"), *GeneralizedPerspectiveProjection1(RightEye).ToString());

	//UE_LOG(LogTemp, Warning, TEXT("LeftCam->CustomProjectionMatrix l %s"), *LeftCam->CustomProjectionMatrix.ToString());
	//UE_LOG(LogTemp, Warning, TEXT("RightCam->CustomProjectionMatrix l %s"), *RightCam->CustomProjectionMatrix.ToString());
}


//FMatrix FGoogleVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType, const float FOV) const
//{
//#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
//
//	check(ActiveViewportList);
//	check(gvr_buffer_viewport_list_get_size(ActiveViewportList) == 2);
//	switch(StereoPassType)
//	{
//		case EStereoscopicPass::eSSP_LEFT_EYE:
//			gvr_buffer_viewport_list_get_item(ActiveViewportList, 0, ScratchViewport);
//			break;
//		case EStereoscopicPass::eSSP_RIGHT_EYE:
//			gvr_buffer_viewport_list_get_item(ActiveViewportList, 1, ScratchViewport);
//			break;
//		default:
//			// We shouldn't got here.
//			check(false);
//			break;
//	}
//
//	gvr_rectf EyeFov = gvr_buffer_viewport_get_source_fov(ScratchViewport);
//	//UE_LOG(LogHMD, Log, TEXT("Eye %d FOV: Left - %f, Right - %f, Top - %f, Botton - %f"), StereoPassType, EyeFov.left, EyeFov.right, EyeFov.top, EyeFov.bottom);
//
//	// Have to flip left/right and top/bottom to match UE4 expectations
//	float Right = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.left));
//	float Left = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.right));
//	float Bottom = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.top));
//	float Top = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.bottom));
//
//	float ZNear = GNearClippingPlane;
//
//	float SumRL = (Right + Left);
//	float SumTB = (Top + Bottom);
//	float InvRL = (1.0f / (Right - Left));
//	float InvTB = (1.0f / (Top - Bottom));
//
//#if LOG_VIEWER_DATA_FOR_GENERATION
//
//	FPlane Plane0 = FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f);
//	FPlane Plane1 = FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f);
//	FPlane Plane2 = FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f);
//	FPlane Plane3 = FPlane(0.0f, 0.0f, ZNear, 0.0f);
//
//	const TCHAR* EyeString = StereoPassType == eSSP_LEFT_EYE ? TEXT("Left") : TEXT("Right");
//	UE_LOG(LogHMD, Log, TEXT("===== Begin Projection Matrix Eye %s"), EyeString);
//	UE_LOG(LogHMD, Log, TEXT("const FMatrix %sStereoProjectionMatrix = FMatrix("), EyeString);
//	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  0.0f, 0.0f, 0.0f),"), Plane0.X);
//	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, %ff,  0.0f, 0.0f),"), Plane1.Y);
//	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  %ff,  0.0f, 1.0f),"), Plane2.X, Plane2.Y);
//	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, 0.0f, %ff,  0.0f)"), Plane3.Z);
//	UE_LOG(LogHMD, Log, TEXT(");"));
//	UE_LOG(LogHMD, Log, TEXT("===== End Projection Matrix Eye %s"));
//
//	return FMatrix(
//		Plane0,
//		Plane1,
//		Plane2,
//		Plane3
//		);
//
//#else
//
//	return FMatrix(
//		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
//		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
//		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
//		FPlane(0.0f, 0.0f, ZNear, 0.0f)
//		);
//
//#endif // LOG_VIEWER_DATA_FOR_GENERATION
//
//#else //!GOOGLEVRHMD_SUPPORTED_PLATFORMS
//
//	if(GetPreviewViewerType() == EVP_None)
//	{
//		// Test data copied from SimpleHMD
//		const float ProjectionCenterOffset = 0.151976421f;
//		const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;
//
//		const float HalfFov = 2.19686294f / 2.f;
//		const float InWidth = 640.f;
//		const float InHeight = 480.f;
//		const float XS = 1.0f / tan(HalfFov);
//		const float YS = InWidth / tan(HalfFov) / InHeight;
//
//		const float InNearZ = GNearClippingPlane;
//		return FMatrix(
//			FPlane(XS,                      0.0f,								    0.0f,							0.0f),
//			FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
//			FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
//			FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))
//
//			* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
//	}
//	else
//	{
//		return GetPreviewViewerStereoProjectionMatrix(StereoPassType);
//	}
//
//#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
//}