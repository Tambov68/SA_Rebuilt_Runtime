#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "SARideCameraComponent.generated.h"
class ACameraActor;
class ACharacter;
class APlayerController;
class UInputComponent;

/** Local riding view independent of pawn possession. */
UCLASS(ClassGroup=(Camera), meta=(BlueprintSpawnableComponent))
class SA_BMXRUNTIME_API USARideCameraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USARideCameraComponent();
    UFUNCTION(BlueprintCallable, Category="Ride Camera") void ToggleView();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ride Camera") bool bFirstPerson=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") float CarFOV=78.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") float BikeFOV=88.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ride Camera") FVector EyeOffset=FVector(10.f,0.f,4.f);
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
    void EnterView(AActor* Ride);
};
