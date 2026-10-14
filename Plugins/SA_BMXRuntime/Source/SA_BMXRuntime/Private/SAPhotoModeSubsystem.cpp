#include "SAPhotoModeSubsystem.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#if SA_BMX_WITH_DLSS
#include "DLSSLibrary.h"
#endif
#include "ChaosVehicleMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "GameFramework/HUD.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"
#include "Styling/CoreStyle.h"
#include "UnrealClient.h"

namespace
{
    // UE 5.8 exposes a getter, but the editable pause-tick property has no public setter.
    void SetFullPausedTick(APlayerController* PC, bool Value)
    {
        if (FBoolProperty* Property=FindFProperty<FBoolProperty>(PC->GetClass(),TEXT("bShouldPerformFullTickWhenPaused")))
            Property->SetPropertyValue_InContainer(PC,Value);
    }
}

class FSAPhotoInput : public IInputProcessor
{
public:
    TWeakObjectPtr<USAPhotoModeSubsystem> Owner;
    TSet<FKey> Keys;
    FVector2D Mouse=FVector2D::ZeroVector;
    bool bRotate=false;
    explicit FSAPhotoInput(USAPhotoModeSubsystem* InOwner):Owner(InOwner){}
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
    {
        auto* S=Owner.Get(); if(!S) return false;
        const FKey K=E.GetKey();
        if(K==EKeys::F8 && !E.IsRepeat())
        {
            if(!S->IsActive())
            {
                if(!GEngine||!GEngine->GameViewport||GEngine->GameViewport->GetWorld()!=S->GetWorld())return false;
                const auto ViewportWidget=GEngine->GameViewport->GetGameViewportWidget();
                if(!ViewportWidget||!ViewportWidget->HasAnyUserFocusOrFocusedDescendants())return false;
            }
            S->Toggle();return true;
        }
        if(!S->IsActive()) return false;
        Keys.Add(K);
        if(!E.IsRepeat())
        {
            if(K==EKeys::Escape) S->Exit();
            else if(K==EKeys::P) S->Capture();
            else if(K==EKeys::H) S->TogglePanel();
            else if(K==EKeys::One) S->SetVideo(false);
            else if(K==EKeys::Two) S->SetVideo(true);
            else if(K==EKeys::F) S->Focus();
        }
        return K==EKeys::Escape||K==EKeys::P||K==EKeys::H||K==EKeys::One||K==EKeys::Two||K==EKeys::F||K==EKeys::W||K==EKeys::A||K==EKeys::S||K==EKeys::D||K==EKeys::Q||K==EKeys::E||K==EKeys::LeftShift||K==EKeys::LeftControl;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&,const FKeyEvent& E) override
    { const bool Held=Keys.Contains(E.GetKey()); Keys.Remove(E.GetKey());return Owner.IsValid()&&Owner->IsActive()&&Held; }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication&,const FPointerEvent& E) override
    { if(Owner.IsValid()&&Owner->IsActive()&&E.GetEffectingButton()==EKeys::RightMouseButton){bRotate=true;return true;}return false; }
    virtual bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent& E) override
    { if(E.GetEffectingButton()==EKeys::RightMouseButton){const bool Was=bRotate;bRotate=false;return Was;}return false; }
    virtual bool HandleMouseMoveEvent(FSlateApplication&,const FPointerEvent& E) override
    { if(Owner.IsValid()&&Owner->IsActive()&&bRotate){Mouse+=E.GetCursorDelta();return true;}return false; }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&,const FPointerEvent& E,const FPointerEvent*) override
    { if(Owner.IsValid()&&Owner->IsActive()){Owner->SetLens(float(E.GetWheelDelta())*5.f);return true;}return false; }
};

bool USAPhotoModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{ const UWorld* W=Cast<UWorld>(Outer);return Super::ShouldCreateSubsystem(Outer)&&W&&W->IsGameWorld()&&!IsRunningDedicatedServer(); }
void USAPhotoModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LastTickTime=FPlatformTime::Seconds();
    if(FSlateApplication::IsInitialized())
    { Input=MakeShared<FSAPhotoInput>(this);FSlateApplication::Get().RegisterInputPreProcessor(Input,0); }
}
void USAPhotoModeSubsystem::Deinitialize()
{
    Exit();
    if(Input.IsValid()&&FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Input);
    Input.Reset();Super::Deinitialize();
}
TStatId USAPhotoModeSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(SAPhotoMode,STATGROUP_Tickables); }
void USAPhotoModeSubsystem::Toggle() { if(bActive) Exit();else Enter(); }
void USAPhotoModeSubsystem::Enter()
{
    APlayerController* PC=GetWorld()->GetFirstPlayerController();
    if(!PC||!PC->IsLocalController()||!PC->PlayerCameraManager||!GEngine||!GEngine->GameViewport)return;
    Controller=PC;PreviousView=PC->GetViewTarget();PreviousPawn=PC->GetPawn();
    bWasPaused=PC->IsPaused();bOldCursor=PC->bShowMouseCursor;
    bOldFullPausedTick=PC->ShouldPerformFullTickWhenPaused();
    const FMinimalViewInfo POV=PC->PlayerCameraManager->GetCameraCacheView();
    FActorSpawnParameters Spawn;Spawn.ObjectFlags|=RF_Transient;
    Camera=GetWorld()->SpawnActor<ACineCameraActor>(POV.Location,POV.Rotation,Spawn);
    if(!Camera)return;
    auto* C=Camera->GetCineCameraComponent();
    C->Filmback.SensorWidth=36.f;C->Filmback.SensorHeight=20.25f;
    C->LensSettings.MinFocalLength=15.f;C->LensSettings.MaxFocalLength=120.f;
    C->SetCurrentFocalLength(FMath::Clamp(18.f/FMath::Tan(FMath::DegreesToRadians(POV.FOV*.5f)),15.f,120.f));
    C->SetCurrentAperture(5.6f);
    C->PostProcessSettings=POV.PostProcessSettings;
    Exposure=C->PostProcessSettings.AutoExposureBias;
    C->PostProcessSettings.bOverride_MotionBlurAmount=true;C->PostProcessSettings.MotionBlurAmount=0.f;
    bActive=true;bVideo=false;bPanelHidden=false;CaptureCountdown=0;
    if(Input){Input->Keys.Reset();Input->Mouse=FVector2D::ZeroVector;Input->bRotate=false;}
    PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
    PC->FlushPressedKeys();SetFullPausedTick(PC,true);
    if(APawn* Pawn=PC->GetPawn())
    {
        Pawn->ConsumeMovementInputVector();
        if(auto* Vehicle=Pawn->FindComponentByClass<UChaosVehicleMovementComponent>())
        {Vehicle->SetThrottleInput(0);Vehicle->SetBrakeInput(0);Vehicle->SetSteeringInput(0);Vehicle->SetHandbrakeInput(false);}
    }
    PC->SetViewTarget(Camera);PC->SetPause(true);
    PC->bShowMouseCursor=true;PC->SetInputMode(FInputModeUIOnly());
    if(auto* HUD=PC->GetHUD()){bOldHUDVisible=HUD->bShowHUD;HUD->bShowHUD=false;}
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,UUserWidget::StaticClass(),true);
    for(auto* Widget:Widgets)if(Widget&&Widget->IsInViewport()&&(!Widget->GetOwningPlayer()||Widget->GetOwningPlayer()==PC))
    {HiddenWidgets.Add(Widget,uint8(Widget->GetVisibility()));Widget->SetVisibility(ESlateVisibility::Hidden);}
    Focus();ApplyQuality();BuildPanel();
}
void USAPhotoModeSubsystem::Exit()
{
    if(!bActive)return;
    bActive=false;
    if(Panel&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset();RestoreQuality();
    if(auto* PC=Controller.Get())
    {
        if(PC->GetPawn()!=PreviousPawn.Get()&&PC->GetPawn())PC->SetViewTarget(PC->GetPawn());
        else if(PreviousView.IsValid())PC->SetViewTarget(PreviousView.Get());
        else if(PC->GetPawn())PC->SetViewTarget(PC->GetPawn());
        PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);
        PC->FlushPressedKeys();SetFullPausedTick(PC,bOldFullPausedTick);
        PC->SetPause(bWasPaused);PC->bShowMouseCursor=bOldCursor;PC->SetInputMode(FInputModeGameOnly());
        if(auto* HUD=PC->GetHUD())HUD->bShowHUD=bOldHUDVisible;
    }
    for(const auto& P:HiddenWidgets)if(P.Key.IsValid())P.Key->SetVisibility(ESlateVisibility(P.Value));
    HiddenWidgets.Reset();
    if(IsValid(Camera))Camera->Destroy();Camera=nullptr;
    if(Input){Input->Keys.Reset();Input->Mouse=FVector2D::ZeroVector;Input->bRotate=false;}
    CaptureCountdown=0;PendingCaptureFrames=0;Controller.Reset();PreviousView.Reset();PreviousPawn.Reset();
}
void USAPhotoModeSubsystem::Tick(float)
{
    const double Now=FPlatformTime::Seconds();const float DT=FMath::Clamp(float(Now-LastTickTime),0.f,.05f);LastTickTime=Now;
    if(!bActive)return;
    if(!Controller.IsValid()||!IsValid(Camera)||!GEngine||!GEngine->GameViewport){Exit();return;}
    if(Controller->GetViewTarget()!=Camera)Controller->SetViewTarget(Camera);
    if(!FSlateApplication::Get().IsActive()) {Input->Keys.Reset();Input->bRotate=false;Input->Mouse=FVector2D::ZeroVector;return;}
    if(Input)
    {
        const auto& K=Input->Keys;
        FVector Move((K.Contains(EKeys::W)?1.f:0.f)-(K.Contains(EKeys::S)?1.f:0.f),(K.Contains(EKeys::D)?1.f:0.f)-(K.Contains(EKeys::A)?1.f:0.f),0.f);
        const float Speed=300.f*(K.Contains(EKeys::LeftShift)?4.f:1.f)*(K.Contains(EKeys::LeftControl)?.25f:1.f);
        FVector WorldMove=Camera->GetActorRotation().RotateVector(Move.GetClampedToMaxSize(1.f));
        WorldMove.Z+=(K.Contains(EKeys::E)?1.f:0.f)-(K.Contains(EKeys::Q)?1.f:0.f);
        Camera->AddActorWorldOffset(WorldMove*Speed*DT,false);
        FRotator R=Camera->GetActorRotation();R.Yaw+=Input->Mouse.X*.15f;R.Pitch=FMath::Clamp(R.Pitch-Input->Mouse.Y*.15f,-89.f,89.f);R.Roll=0;
        Camera->SetActorRotation(R);Input->Mouse=FVector2D::ZeroVector;
    }
    if(CaptureCountdown>0&&--CaptureCountdown==0)
    {
        if(Panel)Panel->SetVisibility(bPanelHidden?EVisibility::Hidden:EVisibility::Visible);
        if(Controller.IsValid())Controller->bShowMouseCursor=bCaptureCursor;
    }
    if(PendingCaptureFrames>0&&--PendingCaptureFrames==0)FinishCapture();
}
void USAPhotoModeSubsystem::ApplyQuality()
{
    auto Set=[this](const TCHAR* Name,const TCHAR* Value)
    {
        if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name))
        {if(!SavedCVars.Contains(Name)){SavedCVars.Add(Name,V->GetString());SavedCVarPriorities.Add(Name,uint32(V->GetFlags())&uint32(ECVF_SetByMask));}V->Set(Value,ECVF_SetByConsole);}
    };
    for(const TCHAR* N:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality")})Set(N,bVideo?TEXT("3"):TEXT("4"));
    Set(TEXT("r.ScreenPercentage"),TEXT("100"));
    Set(TEXT("r.DynamicRes.OperationMode"),TEXT("0"));
#if SA_BMX_WITH_DLSS
    const bool bDLSSSupported=UDLSSLibrary::IsDLSSSupported();
    if(bDLSSSupported)
    {
        Set(TEXT("r.NGX.DLSS.Enable"),bVideo?TEXT("1"):TEXT("0"));
        Set(TEXT("r.AntiAliasingMethod"),bVideo?TEXT("2"):TEXT("4"));
        if(bVideo)
        {
            bool Supported=false,Fixed=false;float Optimal=100,Min=100,Max=100,Sharp=0;
            const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY();
            UDLSSLibrary::GetDLSSModeInformation(UDLSSMode::Quality,FVector2D(Size),Supported,Optimal,Fixed,Min,Max,Sharp);
            if(Supported)
            {
                Set(TEXT("r.TemporalAA.Upscaler"),TEXT("1"));Set(TEXT("r.TemporalAA.Upsampling"),TEXT("1"));
                Set(TEXT("r.ScreenPercentage"),*FString::SanitizeFloat(Optimal));
                Status=TEXT("Видео · Epic · DLSS Quality");
            }
            else {Set(TEXT("r.NGX.DLSS.Enable"),TEXT("0"));Set(TEXT("r.AntiAliasingMethod"),TEXT("4"));Status=TEXT("Видео · Epic · TSR (DLSS Quality недоступен)");}
        }
        else Status=TEXT("Фото · Cinematic · нативное разрешение");
    }
    else
#endif
    {Set(TEXT("r.NGX.DLSS.Enable"),TEXT("0"));Set(TEXT("r.AntiAliasingMethod"),TEXT("4"));Status=bVideo?TEXT("Видео · Epic · TSR · DLSS недоступен"):TEXT("Фото · Cinematic · нативное разрешение");}
}
void USAPhotoModeSubsystem::RestoreQuality()
{
    auto Restore=[this](const TPair<FString,FString>& P)
    {
        if(auto* V=IConsoleManager::Get().FindConsoleVariable(*P.Key))
        {
            const auto Old=EConsoleVariableFlags(SavedCVarPriorities.FindChecked(P.Key));
            if(Old!=ECVF_SetByConsole)V->Unset(ECVF_SetByConsole);
            V->Set(*P.Value,Old);
        }
    };
    // Restore scalability first; then restore individual settings it may have changed.
    for(const auto& P:SavedCVars)if(P.Key.StartsWith(TEXT("sg.")))Restore(P);
    for(const auto& P:SavedCVars)if(!P.Key.StartsWith(TEXT("sg.")))Restore(P);
    SavedCVars.Reset();SavedCVarPriorities.Reset();
}
void USAPhotoModeSubsystem::SetVideo(bool Video)
{if(!bActive)return;bVideo=Video;if(auto* PC=Controller.Get())PC->SetPause(Video?false:true);ApplyQuality();}
void USAPhotoModeSubsystem::SetLens(float Delta)
{if(Camera){auto* C=Camera->GetCineCameraComponent();C->SetCurrentFocalLength(FMath::Clamp(C->CurrentFocalLength+Delta,15.f,120.f));}}
void USAPhotoModeSubsystem::SetAperture(float FStop)
{if(Camera)Camera->GetCineCameraComponent()->SetCurrentAperture(FMath::Clamp(FStop,1.4f,16.f));}
void USAPhotoModeSubsystem::SetExposure(float Stops)
{if(Camera){Exposure=Stops;auto& P=Camera->GetCineCameraComponent()->PostProcessSettings;P.bOverride_AutoExposureBias=true;P.AutoExposureBias=Stops;}}
void USAPhotoModeSubsystem::Focus()
{
    if(!Camera)return;
    FHitResult H;const FVector Start=Camera->GetActorLocation();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SAPhotoFocus),true,Camera);
    float D=1000.f;if(GetWorld()->LineTraceSingleByChannel(H,Start,Start+Camera->GetActorForwardVector()*30000.f,ECC_Visibility,Q))D=H.Distance;
    auto* C=Camera->GetCineCameraComponent();C->FocusSettings.FocusMethod=ECameraFocusMethod::Manual;C->FocusSettings.ManualFocusDistance=FMath::Max(D,30.f);
}
void USAPhotoModeSubsystem::TogglePanel()
{bPanelHidden=!bPanelHidden;if(Panel)Panel->SetVisibility(bPanelHidden?EVisibility::Hidden:EVisibility::Visible);}
void USAPhotoModeSubsystem::Capture()
{
    if(!bActive||CaptureCountdown||PendingCaptureFrames||!GEngine||!GEngine->GameViewport||!GEngine->GameViewport->Viewport)return;
    if(Panel)Panel->SetVisibility(EVisibility::Hidden);
    if(Controller.IsValid()){bCaptureCursor=Controller->bShowMouseCursor;Controller->bShowMouseCursor=false;}
    // Allow Slate to draw at least one complete frame without the controls before capture.
    PendingCaptureFrames=2;
}
void USAPhotoModeSubsystem::FinishCapture()
{
    if(!bActive||!GEngine||!GEngine->GameViewport||!GEngine->GameViewport->Viewport)return;
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("PhotoMode"));IFileManager::Get().MakeDirectory(*Dir,true);
    auto& Config=GetHighResScreenshotConfig();
    Config.FilenameOverride=Dir/TEXT("SA_Photo_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)+TEXT(".png");
    Config.SetHDRCapture(false);Config.bMaskEnabled=false;
    if(!Config.SetResolution(2560,1440,1.f)){Status=TEXT("Не удалось задать размер снимка");CaptureCountdown=1;return;}
    if(GEngine->GameViewport->Viewport->TakeHighResScreenShot())
    {CaptureCountdown=8;Status=TEXT("PNG 2560×1440 → Saved/PhotoMode");}
    else {Status=TEXT("Снимок не запущен");CaptureCountdown=1;}
}
FText USAPhotoModeSubsystem::StatusText() const{return FText::FromString(Status);}
FText USAPhotoModeSubsystem::LensText() const{return FText::FromString(FString::Printf(TEXT("Объектив: %.0f мм"),Camera?Camera->GetCineCameraComponent()->CurrentFocalLength:0));}
FText USAPhotoModeSubsystem::ApertureText() const{return FText::FromString(FString::Printf(TEXT("Диафрагма: f/%.1f"),Camera?Camera->GetCineCameraComponent()->CurrentAperture:0));}
FText USAPhotoModeSubsystem::ExposureText() const{return FText::FromString(FString::Printf(TEXT("Экспозиция: %+.1f"),Exposure));}
void USAPhotoModeSubsystem::BuildPanel()
{
    TSharedRef<SVerticalBox> V=SNew(SVerticalBox);
    V->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Text(FText::FromString(TEXT("SA REBUILT · ФОТОРЕЖИМ")))];
    V->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return StatusText();}).AutoWrapText(true)];
    V->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(FText::FromString(TEXT("WASD — движение · Q/E — вниз/вверх\nПКМ + мышь — поворот · Shift — быстрее\nКолесо — объектив · F — фокус по центру\nP — снимок · H — панель · F8/Esc — выход")))];
    TSharedRef<SHorizontalBox> Modes=SNew(SHorizontalBox);
    Modes->AddSlot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("1 · Фото / пауза"))).OnClicked_Lambda([this]{SetVideo(false);return FReply::Handled();})];
    Modes->AddSlot().FillWidth(1)[SNew(SButton).Text(FText::FromString(TEXT("2 · Видео / движение"))).OnClicked_Lambda([this]{SetVideo(true);return FReply::Handled();})];
    V->AddSlot().AutoHeight().Padding(0,6)[Modes];
    V->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]{return LensText();})];
    V->AddSlot().AutoHeight()[SNew(SSlider).Value_Lambda([this]{return Camera?(Camera->GetCineCameraComponent()->CurrentFocalLength-15.f)/105.f:0.f;}).OnValueChanged_Lambda([this](float X){if(Camera)SetLens(15.f+105.f*X-Camera->GetCineCameraComponent()->CurrentFocalLength);})];
    V->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text_Lambda([this]{return ApertureText();})];
    V->AddSlot().AutoHeight()[SNew(SSlider).Value_Lambda([this]{return Camera?(Camera->GetCineCameraComponent()->CurrentAperture-1.4f)/14.6f:0.f;}).OnValueChanged_Lambda([this](float X){SetAperture(1.4f+14.6f*X);})];
    V->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text_Lambda([this]{return ExposureText();})];
    V->AddSlot().AutoHeight()[SNew(SSlider).Value_Lambda([this]{return (Exposure+16.f)/24.f;}).OnValueChanged_Lambda([this](float X){SetExposure(-16.f+24.f*X);})];
    V->AddSlot().AutoHeight().Padding(0,8)[SNew(SButton).Text(FText::FromString(TEXT("Фокус на объекте по центру"))).OnClicked_Lambda([this]{Focus();return FReply::Handled();})];
    V->AddSlot().AutoHeight()[SNew(SButton).Text(FText::FromString(TEXT("Сохранить PNG · 2560×1440"))).OnClicked_Lambda([this]{Capture();return FReply::Handled();})];
    V->AddSlot().AutoHeight().Padding(0,8)[SNew(SButton).Text(FText::FromString(TEXT("Вернуться в игру"))).OnClicked_Lambda([this]{Exit();return FReply::Handled();})];
    Panel=SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1)
      +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(16)[SNew(SBox).WidthOverride(360)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).ForegroundColor(FLinearColor::White).Padding(16).BorderBackgroundColor(FLinearColor(.025f,.035f,.045f,1.f))[V]]];
    GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(),10000);
}
