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
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<USkeletalMesh> DemoRiderMesh;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftClassPtr<UAnimInstance> DemoRiderAnimClass;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoIdleAnimation;
    UPROPERTY(Config, EditAnywhere, Category="Demo") TSoftObjectPtr<UAnimationAsset> DemoWalkAnimation;
private:
    UPROPERTY() TObjectPtr<UBMXInteractionComponent> Interaction;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Placeholder;
    UPROPERTY() TObjectPtr<UAnimationAsset> IdleAnimation;
    UPROPERTY() TObjectPtr<UAnimationAsset> WalkAnimation;
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
