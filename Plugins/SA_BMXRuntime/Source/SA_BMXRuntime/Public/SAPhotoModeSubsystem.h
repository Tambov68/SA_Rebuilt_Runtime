#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SAPhotoModeSubsystem.generated.h"

class ACineCameraActor;
class APlayerController;
class FSAPhotoInput;
class SWidget;
class UUserWidget;

/** Local photo camera; lives independently of pedestrian/vehicle possession. */
UCLASS()
class SA_BMXRUNTIME_API USAPhotoModeSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickableWhenPaused() const override { return true; }
    virtual TStatId GetStatId() const override;
    void Toggle();
    void Exit();
    void SetVideo(bool Video);
    void Capture();
    void Focus();
    void SetLens(float Millimeters);
    void SetAperture(float FStop);
    void SetExposure(float Stops);
    bool IsActive() const { return bActive; }
    bool IsVideo() const { return bVideo; }
    void TogglePanel();
    FText StatusText() const;
    FText LensText() const;
    FText ApertureText() const;
    FText ExposureText() const;
private:
    void Enter();
    void BuildPanel();
    void ApplyQuality();
    void RestoreQuality();
    void FinishCapture();
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> PreviousView;
    TWeakObjectPtr<APawn> PreviousPawn;
    TMap<TWeakObjectPtr<UUserWidget>, uint8> HiddenWidgets;
    UPROPERTY(Transient) TObjectPtr<ACineCameraActor> Camera;
    TSharedPtr<FSAPhotoInput> Input;
    TSharedPtr<SWidget> Panel;
    TMap<FString, FString> SavedCVars;
    TMap<FString, uint32> SavedCVarPriorities;
    bool bActive=false, bVideo=false, bWasPaused=false, bOldCursor=false, bPanelHidden=false;
    bool bOldFullPausedTick=false;
    bool bOldHUDVisible=true;
    int32 CaptureCountdown=0;
    int32 PendingCaptureFrames=0;
    bool bCaptureCursor=true;
    double LastTickTime=0;
    float Exposure=0;
    FString Status;
};
