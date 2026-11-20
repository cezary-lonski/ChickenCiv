#include "Player/StrategyPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "InputCoreTypes.h"

AStrategyPawn::AStrategyPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->SetRelativeRotation(FRotator(-55.f, 0.f, 0.f));
	Arm->TargetArmLength = 1100.f;
	Arm->bDoCollisionTest = false;
	Arm->bUsePawnControlRotation = false;
	Arm->bInheritPitch = false;
	Arm->bInheritYaw = false;
	Arm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);
}

float AStrategyPawn::GetZoom() const
{
	return Arm->TargetArmLength;
}

void AStrategyPawn::ZoomBy(float Steps)
{
	Arm->TargetArmLength = FMath::Clamp(Arm->TargetArmLength * FMath::Pow(0.85f, Steps), MinZoom, MaxZoom);
}

void AStrategyPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	FVector2D Dir(0.f, 0.f);
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    Dir.X += 1.f;
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  Dir.X -= 1.f;
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) Dir.Y += 1.f;
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  Dir.Y -= 1.f;

	if (!Dir.IsNearlyZero())
	{
		Dir.Normalize();
		const float Speed = Arm->TargetArmLength * 0.9f; // dalej = szybciej
		FVector Loc = GetActorLocation() + FVector(Dir.X, Dir.Y, 0.f) * Speed * DeltaSeconds;
		if (bHasBounds)
		{
			Loc.X = FMath::Clamp(Loc.X, (double)MapBounds.Min.X, (double)MapBounds.Max.X);
			Loc.Y = FMath::Clamp(Loc.Y, (double)MapBounds.Min.Y, (double)MapBounds.Max.Y);
		}
		SetActorLocation(Loc);
	}
}
