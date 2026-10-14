#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SAVehicleInteractionComponent.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class UAnimInstance;
class UAnimSequence;
class ACameraActor;
class USceneComponent;

UENUM(BlueprintType)
enum class ESAVehicleInteractionState : uint8 { OnFoot, Entering, Driving, Exiting, Jumping };

// Shares the existing BMX F interaction instead of installing another key binding.
UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class SA_BMXRUNTIME_API USAVehicleInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USAVehicleInteractionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="100", Units="cm"))
    float InteractionRange = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0", Units="cm/s"))
    float MaxEnterSpeed = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0", Units="cm/s"))
    float MaxExitSpeed = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="120", Units="cm"))
    float ExitSideDistance = 180.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    TObjectPtr<APawn> CurrentVehicle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    FString LastStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    FVector LastExitLocation = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    int32 EnterCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    int32 ExitCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle")
    ESAVehicleInteractionState State = ESAVehicleInteractionState::OnFoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Animation") float TransitionTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Animation") float DoorOpenAlpha = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Jump") int32 JumpOutCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Jump") FVector LastReleaseVelocity = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation") TObjectPtr<UAnimSequence> EnterAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation") TObjectPtr<UAnimSequence> ExitAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation") TObjectPtr<UAnimSequence> DrivingAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation") TObjectPtr<UAnimSequence> JumpAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation", meta=(ClampMin="0", ClampMax="90")) float DoorOpenDegrees = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Jump") bool bAllowJumpOut = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Jump", meta=(ClampMin="0", Units="cm/s")) float JumpSideSpeed = 280.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Jump", meta=(ClampMin="0", Units="cm/s")) float JumpUpSpeed = 160.f;

    UFUNCTION(BlueprintPure, Category="Vehicle") bool IsDriving() const;
    UFUNCTION(BlueprintPure, Category="Vehicle") APawn* FindNearestVehicle() const;
    UFUNCTION(BlueprintCallable, Category="Vehicle") bool TryEnter(APawn* Vehicle);
    UFUNCTION(BlueprintCallable, Category="Vehicle") bool TryExit();
    UFUNCTION(BlueprintCallable, Category="Vehicle|Jump") bool TryJumpOut();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Animation") float VisualSteering=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Animation") float SteeringWheelDegrees=45.f;
    // Called after the rider mesh has evaluated its current pose.
    void UpdateAnimationCamera();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<APlayerController> DriverController;
    bool bDriving = false;
    bool SavedHidden = false;
    bool SavedActorCollision = true;
    bool SavedControllerYaw = false;
    TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
    uint8 SavedCustomMovementMode = 0;
    FTransform SavedCharacterTransform;
    FRotator SavedControlRotation;
    UPROPERTY(Transient) TSubclassOf<UAnimInstance> SavedAnimClass;
    FTransform SavedMeshRelative;
    bool SavedOwnerNoSee = false;
    bool SavedOnlyOwnerSee = false;
    bool SavedDisablePostProcess = false;
    bool SavedPauseAnims = false;
    bool bIgnoreMoveAdded = false;
    int32 ExitSide = -1;
    FVector EntryWarp = FVector::ZeroVector;
    FVector ExitWarp = FVector::ZeroVector;
    FVector PlannedExit = FVector::ZeroVector;
    TWeakObjectPtr<ACameraActor> TransitionCamera;
    TWeakObjectPtr<APawn> ClosingDoorVehicle;
    int32 ClosingDoorSide=-1;
    TArray<TPair<TWeakObjectPtr<USceneComponent>,bool>> HiddenAccessories;

    bool FindExitLocation(APawn* Vehicle, FVector& OutLocation) const;
    void UpdateDrivingPose(float DeltaTime);
    bool DoorSpaceClear(APawn* Vehicle, int32 Side) const;
    void SetDoor(APawn* Vehicle, int32 Side, float Alpha);
    void StartCamera();
    void UpdateCamera();
    void FinishCamera(AActor* Target);
    void SetDriverPose(UAnimSequence* Animation, float Position, const FVector& Warp = FVector::ZeroVector, bool Mirror = false);
    void FinishEntry();
    void FinishExit();
    void ReleaseJump();
    void ClearInteraction();
    void RestoreCharacter(const FVector& Location, const FRotator& Rotation);
};
