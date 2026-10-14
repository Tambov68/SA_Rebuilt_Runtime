#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "SARuntimeDemo.generated.h"

class UBMXInteractionComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UAnimInstance;
class UAnimationAsset;

/** Engine primitives only: no private project assets or animated rider. */
UCLASS()
class SA_BMXRUNTIME_API ASADemoBike : public AActor
{
    GENERATED_BODY()
public:
    ASADemoBike();
protected:
    virtual void PostInitializeComponents() override;
};

UCLASS(Config=Game)
class SA_BMXRUNTIME_API ASADemoCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ASADemoCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<USkeletalMesh> DemoRiderMesh;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftClassPtr<UAnimInstance> DemoRiderAnimClass;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoIdleAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoWalkAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoCrouchAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoCrouchWalkAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoJumpAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoFallAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoLandAnimation;
private:
    UPROPERTY() TObjectPtr<UBMXInteractionComponent> Interaction;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Placeholder;
    UPROPERTY() TObjectPtr<UAnimationAsset> IdleAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> WalkAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> CrouchAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> CrouchWalkAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> JumpAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> FallAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> LandAnimation;
    bool bWasFalling=false;
    bool bWasCrouched=false;
    float LandingPoseTime=0.f;
    void OnFootJumpPressed();
    void OnFootJumpReleased();
    void OnFootCrouchPressed();
    void OnFootCrouchReleased();
};

/** Select this as a blank map's GameMode override to spawn the demo at runtime. */
UCLASS(Config=Game)
class SA_BMXRUNTIME_API ASARuntimeDemoMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASARuntimeDemoMode();
    virtual void StartPlay() override;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftClassPtr<AActor> DemoBikeClass;
    UPROPERTY(Config, EditAnywhere, Category="Demo") bool bSpawnDemoEnvironment=true;
};
