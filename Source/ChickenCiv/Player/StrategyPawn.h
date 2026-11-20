#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "StrategyPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/** Kamera strategiczna z gory pod katem: WASD/strzalki przesuwaja, kolko myszy przybliza. */
UCLASS()
class CHICKENCIV_API AStrategyPawn : public APawn
{
	GENERATED_BODY()

public:
	AStrategyPawn();

	virtual void Tick(float DeltaSeconds) override;

	void SetMapBounds(const FBox2D& InBounds) { MapBounds = InBounds; bHasBounds = true; }
	void ZoomBy(float Steps);

	float GetZoom() const;

	UPROPERTY(EditAnywhere, Category = "Camera") float MinZoom = 450.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float MaxZoom = 7000.f;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;

	FBox2D MapBounds = FBox2D(ForceInit);
	bool bHasBounds = false;
};
