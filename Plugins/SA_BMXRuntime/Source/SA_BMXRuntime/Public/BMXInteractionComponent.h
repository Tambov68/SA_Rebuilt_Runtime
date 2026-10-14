#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BMXInteractionComponent.generated.h"

class ACharacter;
class APlayerController;
class UInputComponent;
class UStaticMeshComponent;
class UAnimInstance;
class UPrimitiveComponent;
class UAnimSequence;

UENUM(BlueprintType)
enum class EBMXState : uint8 { Dropped, Raising, Mounted, Ragdoll };

UENUM(BlueprintType)
enum class EBMXHopState : uint8 { Grounded, Charging, Airborne, Landing };

UCLASS(ClassGroup=(BMX), meta=(BlueprintSpawnableComponent))
class SA_BMXRUNTIME_API UBMXVehicleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBMXVehicleComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") bool bStartDropped = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") bool bAllowInteraction = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") EBMXState State = EBMXState::Dropped;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") TObjectPtr<ACharacter> Rider;
    UFUNCTION(BlueprintPure, Category="BMX") UStaticMeshComponent* GetFrame() const;
    UFUNCTION(BlueprintCallable, Category="BMX") void DropBike(FVector Velocity);
protected:
    virtual void BeginPlay() override;
};

UCLASS(ClassGroup=(BMX), meta=(BlueprintSpawnableComponent))
class SA_BMXRUNTIME_API UBMXInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBMXInteractionComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") TSubclassOf<UAnimInstance> RiderAnimClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX", meta=(ClampMin="50")) float InteractionRange = 230.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX", meta=(ClampMin="0.1")) float RaiseDuration = 0.7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") float MaxRideSpeed = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Sprint", meta=(ClampMin="450", Units="cm/s")) float MaxSprintSpeed = 800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Sprint", meta=(ClampMin="1")) float SprintAcceleration = 350.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Sprint") bool LastSprint = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Animation") float RiderStandBlend = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Animation") FVector RiderPoseOffset = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Animation") float RiderMountBlend = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Animation") bool bDismountPreparing = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") float MaxReverseSpeed = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") float BrakeDeceleration = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX") float MaxLeanDegrees = 8.f;
    // Riding bowls needs steeper support than the safe standing/mount surface.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Ground", meta=(ClampMin="35", ClampMax="80", Units="deg")) float MaxRideSlopeDegrees = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX", meta=(ClampMin="0", ClampMax="3", Units="cm")) float WheelContactClearance = 0.52f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Camera") float CameraFollowDelay = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Camera") float CameraFollowSpeed = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Wheelie", meta=(ClampMin="0", ClampMax="35")) float MaxWheelieDegrees = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Wheelie", meta=(ClampMin="1")) float WheelieMinSpeed = 90.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Wheelie") float WheelieDegrees = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Wheelie") bool LastWheelie = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Stoppie", meta=(ClampMin="0", ClampMax="25")) float MaxStoppieDegrees = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Stoppie", meta=(ClampMin="1")) float StoppieMinSpeed = 180.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Stoppie") float StoppieDegrees = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Stoppie") bool LastStoppie = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") TObjectPtr<AActor> CurrentBike;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") EBMXState State = EBMXState::Dropped;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") FString LastStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") FVector LastGroundNormal = FVector::UpVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") FVector LastExitLocation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float RideSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Debug") int32 RideBlockedFrames = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Debug") FString LastRideBlock;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Hop", meta=(ClampMin="0.1", ClampMax="2")) float MaxHopCharge = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Hop") float MinHopVelocity = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Hop") float MaxHopVelocity = 450.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") EBMXHopState HopState = EBMXHopState::Grounded;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") float HopCharge = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") FVector HopVelocity = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") float HopAirTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") float LastHopHeight = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") int32 HopCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Hop") int32 LandingCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float PedalPhaseDegrees = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float WheelPhaseDegrees = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float LastThrottle = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float LastSteering = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") bool LastBrake = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX") float LeanDegrees = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BMX|Crash", meta=(ClampMin="100", Units="cm/s")) float CrashMinSpeed = 350.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Crash") int32 CrashCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Crash") float LastCrashSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Crash") float RagdollSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BMX|Crash") FString LastHitDiagnostic;
    UFUNCTION(BlueprintCallable, Category="BMX|Crash") bool TriggerCrash(FVector ImpactVelocity, FVector HitLocation);
    UFUNCTION(BlueprintCallable, Category="BMX|Crash") bool TryRecover();
    // Reuse the existing ground-safe recovery for a released vehicle passenger.
    bool BeginExternalRagdoll(FVector Velocity, UAnimSequence* ReleaseAnimation, float Position, const FTransform& InitialMeshWorld);
    UFUNCTION(BlueprintCallable, Category="BMX") void Interact();
    UFUNCTION(BlueprintCallable, Category="BMX") bool TryMount(AActor* Bike);
    UFUNCTION(BlueprintCallable, Category="BMX") bool TryDismount();
    UFUNCTION(BlueprintCallable, Category="BMX") void SetRideInput(float Throttle, float Steering);
    UFUNCTION(BlueprintCallable, Category="BMX") void SetBrakeInput(bool bBrake);
    UFUNCTION(BlueprintCallable, Category="BMX|Sprint") void SetSprintInput(bool bSprint);
    UFUNCTION(BlueprintCallable, Category="BMX|Hop") void SetJumpInput(bool bJump);
    UFUNCTION(BlueprintCallable, Category="BMX|Wheelie") void SetWheelieInput(bool bWheelie);
    UFUNCTION(BlueprintCallable, Category="BMX|Stoppie") void SetStoppieInput(bool bStoppie);
    // Deterministic inspection helper: runs only on a held (tick-disabled) mounted pose.
    UFUNCTION(BlueprintCallable, Category="BMX|Debug") bool StepHeldRide(float Duration);
    UFUNCTION(BlueprintCallable, Category="BMX") void UpdateRiderIK(UAnimInstance* AnimInstance);
    UFUNCTION(BlueprintPure, Category="BMX") bool IsMounted() const { return State == EBMXState::Mounted && IsValid(CurrentBike); }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<APlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<UInputComponent> BikeInput;
    UPROPERTY(Transient) TSubclassOf<UAnimInstance> SavedAnimClass;
    FTransform SavedMeshRelative;
    FRotator SavedControlRotation = FRotator::ZeroRotator;
    ECollisionEnabled::Type SavedCapsuleCollision = ECollisionEnabled::QueryAndPhysics;
    ECollisionEnabled::Type SavedMeshCollision = ECollisionEnabled::NoCollision;
    TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
    uint8 SavedCustomMovementMode = 0;
    bool SavedControllerYaw = false;
    bool SavedThirdPerson = true;
    bool SavedWeaponEquipped = false;
    FName SavedMeshProfile;
    FCollisionResponseContainer SavedMeshResponses;
    ECollisionChannel SavedMeshObjectType = ECC_Pawn;
    float RagdollTime = 0.f;
    float RagdollRestTime = 0.f;
    bool bExternalRagdoll = false;
    bool SavedUseCCD = false;
    FTransform RaiseFrom, RaiseTo;
    float RaiseTime = 0.f;
    float PedalAngle = 0.f;
    float ThrottleInput = 0.f;
    float SteeringInput = 0.f;
    float LastManualLookTime = 0.f;
    FVector WheelieRootOffset = FVector::ZeroVector;
    FVector RiderSeatLocation = FVector::ZeroVector;
    bool WheelieInput = false;
    bool bInjectedWheelieInput = false;
    bool StoppieInput = false;
    bool bInjectedStoppieInput = false;
    bool bStoppieLatched = false;
    float StoppieTime = 0.f;
    bool BrakeInput = false;
    bool bInjectedBrakeInput = false;
    bool SprintInput = false;
    bool bInjectedSprintInput = false;
    bool bInjectedRideInput = false;
    bool bInputInstalled = false;
    bool bMoveIgnoreAdded = false;
    bool JumpInput = false;
    bool bInjectedJumpInput = false;
    bool bJumpWasDown = false;
    bool bNativeJumpDown = false;
    bool bJumpPressPending = false;
    float RiderLandingBlend = 0.f;
    FRotator RiderSpineMotion = FRotator(0,0,25);
    FRotator RiderHeadMotion = FRotator(0,0,-15);
    float DismountPrepareTime = 0.f;
    bool FinishDismount();
    void UpdateRiderMotion(float DeltaTime);
    void JumpPressed();
    void JumpReleased();
    FVector HopStart = FVector::ZeroVector;
    FQuat HopStartRotation = FQuat::Identity;
    void ResetHop();
    void AirTick(float DeltaTime);
    void InstallInput();
    void MouseYaw(float Value);
    void MousePitch(float Value);
    bool GroundUnderCrank(AActor* Bike, FHitResult& Hit) const;
    bool BikePoseClear(AActor* Bike, const FTransform& Pose) const;
    void CompleteMount();
    void CancelRaise(const FString& Reason);
    bool FindExit(FVector& Location) const;
    void RestoreCharacter(const FVector& ExitLocation);
    void RideTick(float DeltaTime);
    void UpdatePedals(float DeltaTime);
    void ApplyCamera(bool bThirdPerson) const;
    void RagdollTick(float DeltaTime);
    UFUNCTION() void OnBikeHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};

UCLASS()
class SA_BMXRUNTIME_API UBMXRiderLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="BMX") static void UpdatePlayerRiderIK(UAnimInstance* AnimInstance);
};
