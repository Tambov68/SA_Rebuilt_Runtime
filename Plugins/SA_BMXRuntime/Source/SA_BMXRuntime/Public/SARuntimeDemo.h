#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "SARuntimeDemo.generated.h"

class UBMXInteractionComponent;
class UStaticMeshComponent;

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

UCLASS()
class SA_BMXRUNTIME_API ASADemoCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ASADemoCharacter();
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<UBMXInteractionComponent> Interaction;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Placeholder;
};

/** Select this as a blank map's GameMode override to spawn the demo at runtime. */
UCLASS()
class SA_BMXRUNTIME_API ASARuntimeDemoMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASARuntimeDemoMode();
    virtual void StartPlay() override;
};
