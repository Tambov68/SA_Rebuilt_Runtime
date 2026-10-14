#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SAMechanicsFilmDirector.generated.h"
class ACineCameraActor;
class ACharacter;
class APawn;
class APlayerController;
class USAVehicleInteractionComponent;
class UBMXInteractionComponent;
class UChaosWheeledVehicleMovementComponent;

// Used only by the cinematic map. Sequencer's time, rather than world time,
// keeps gameplay at its starting point during Movie Render Queue warm-up.
UCLASS()
class SA_BMXRUNTIME_API ASAMechanicsFilmDirector : public AActor
{
    GENERATED_BODY()
public:
    ASAMechanicsFilmDirector();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Film") float FilmTime = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Film") int32 ShotId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Film") TObjectPtr<ACineCameraActor> FilmCamera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Film") bool bPreview = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Film") FString Status;
    virtual void Tick(float DeltaSeconds) override;
private:
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<APawn> Car;
    TWeakObjectPtr<AActor> Bike;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<USAVehicleInteractionComponent> VehicleInteraction;
    TWeakObjectPtr<UBMXInteractionComponent> BMX;
    TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> Movement;
    bool bStaged = false, bEntered = false, bAction = false, bReported = false;
    float PreviewTime = -2.f;
    void Stage();
    void MoveCamera();
};
