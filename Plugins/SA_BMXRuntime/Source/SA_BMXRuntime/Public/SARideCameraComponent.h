#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "SARideCameraComponent.generated.h"
class ACameraActor;
class ACharacter;
class APlayerController;
class UInputComponent;
class USpringArmComponent;

/** Local riding view independent of pawn possession. */
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class SA_BMXRUNTIME_API USARideCameraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USARideCameraComponent();
    UFUNCTION(BlueprintCallable, Category="Ride Camera") void ToggleView();
    UFUNCTION(BlueprintCallable, Category="Ride Camera") void ApplyLookInput(float YawInput, float PitchInput);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ride Camera") bool bFirstPerson=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") float CarFOV=78.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") float BikeFOV=88.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") FVector EyeOffset=FVector(10.f,0.f,4.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") FVector CarSeatedEyeOffset=FVector(8.f,0.f,53.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera", meta=(ClampMin="0")) float LookSensitivity=2.5f;
    bool WantsFirstPerson() const { return bPreferFirstPerson; }
    FRotator GetLookRotation() const { return LastViewRotation; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* Function) override;
private:
    void InstallInput();
    void LeaveView(bool bRestoreTarget);
    void LookYaw(float Value);
    void LookPitch(float Value);
    AActor* RideTarget() const;
    TWeakObjectPtr<ACharacter> Character;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> Target;
    TWeakObjectPtr<AActor> PreviousView;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
    UPROPERTY(Transient) TObjectPtr<UInputComponent> RideInput;
    FVector Anchor=FVector::ZeroVector;
    FVector SmoothedEye=FVector::ZeroVector;
    FVector NeckEyeOffset=FVector::ZeroVector;
    EVisibilityBasedAnimTickOption SavedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    float LookX=0.f,LookY=0.f;
    FRotator SmoothedBody=FRotator::ZeroRotator;
    bool bHeadWasHidden=false;
    bool bBike=false;
    bool bPreferFirstPerson=false;
    bool bWasTransition=false;
    bool bHasSavedLook=false;
    FRotator LastViewRotation=FRotator::ZeroRotator;
    TWeakObjectPtr<AActor> SavedLookTarget;
    float SavedLookX=0.f, SavedLookY=0.f;
    TWeakObjectPtr<USpringArmComponent> OrbitArm;
    FRotator SavedOrbitRotation=FRotator::ZeroRotator;
    float OrbitYaw=0.f, OrbitPitch=0.f;
    void UpdateOrbit(AActor* Ride);
    void RestoreOrbit();
    void EnterView(AActor* Ride);
};
