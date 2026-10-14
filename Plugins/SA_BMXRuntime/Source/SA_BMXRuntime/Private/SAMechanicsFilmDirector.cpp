#include "SAMechanicsFilmDirector.h"
#include "SAVehicleInteractionComponent.h"
#include "BMXInteractionComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimSequence.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ASAMechanicsFilmDirector::ASAMechanicsFilmDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    // Follow the completed physics step before PlayerCameraManager caches its POV.
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ASAMechanicsFilmDirector::Stage()
{
    Controller = UGameplayStatics::GetPlayerController(this, 0);
    if (!Controller.IsValid()) return;
    Character = Cast<ACharacter>(Controller->GetPawn());
    if (!Character.IsValid()) return;
    VehicleInteraction = Character->FindComponentByClass<USAVehicleInteractionComponent>();
    BMX = Character->FindComponentByClass<UBMXInteractionComponent>();
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
        if (auto* M = It->FindComponentByClass<UChaosWheeledVehicleMovementComponent>()) { Car = *It; Movement = M; break; }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->FindComponentByClass<UBMXVehicleComponent>()) { Bike = *It; break; }
    if (!FilmCamera) return;
    FVector P = Character->GetActorLocation();
    if (ShotId == 4 && Bike.IsValid())
        P = Bike->GetActorLocation() - Bike->GetActorRightVector() * 145.f;
    else if (Car.IsValid()) P = Car->GetActorTransform().TransformPosition(FVector(-15.f,-230.f,100.f));
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character.Get());
    if (Car.IsValid()) Params.AddIgnoredActor(Car.Get());
    if (Bike.IsValid()) Params.AddIgnoredActor(Bike.Get());
    if (GetWorld()->LineTraceSingleByChannel(Hit,P+FVector(0,0,300),P-FVector(0,0,700),ECC_Visibility,Params)) P.Z=Hit.ImpactPoint.Z+92.f;
    Character->SetActorLocation(P,false,nullptr,ETeleportType::TeleportPhysics);
    Character->SetActorRotation(FRotator(0,ShotId==4 ? 90.f : 90.f,0));
    Controller->SetViewTarget(FilmCamera);
    bStaged = true;
    Status = TEXT("Staged");
}

void ASAMechanicsFilmDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bPreview) { PreviewTime += DeltaSeconds; FilmTime = FMath::Max(0.f,PreviewTime); }
    if (!bStaged) Stage();
    if (!bStaged) return;
    if (FilmTime > .15f && !bEntered)
    {
        if (ShotId == 4 && BMX.IsValid() && Bike.IsValid())
        {
            // The integer shot track is evaluated after initial world warm-up.
            // Stage at the settled bicycle now, rather than at the default car shot.
            Stage();
            bEntered = BMX->TryMount(Bike.Get());
        }
        else if (ShotId != 0 && VehicleInteraction.IsValid() && Car.IsValid()) bEntered = VehicleInteraction->TryEnter(Car.Get());
        else bEntered = true;
        if (!bEntered) Status = ShotId==4 && BMX.IsValid() ? BMX->LastStatus : VehicleInteraction.IsValid() ? VehicleInteraction->LastStatus : TEXT("No interaction component");
    }
    if (ShotId == 4 && BMX.IsValid() && BMX->IsMounted())
    {
        BMX->SetRideInput(FilmTime>1.2f && FilmTime<7.f ? .85f : 0.f,0.f);
        BMX->SetBrakeInput(FilmTime>7.f);
        BMX->SetWheelieInput(FilmTime>2.4f && FilmTime<3.7f);
        BMX->SetJumpInput(FilmTime>4.3f && FilmTime<4.85f);
        if (FilmTime>7.8f && !bAction) bAction=BMX->TryDismount();
    }
    if ((ShotId==2 || ShotId==3) && VehicleInteraction.IsValid() && VehicleInteraction->IsDriving() && Movement.IsValid())
    {
        Movement->SetHandbrakeInput(false);
        Movement->SetBrakeInput(0.f);
        Movement->SetSteeringInput(0.f);
        Movement->SetThrottleInput(bAction ? 0.f : .55f);
        if (ShotId==3 && FilmTime>6.f && !bAction && FMath::Abs(Movement->GetForwardSpeed())>600.f)
            bAction=VehicleInteraction->TryJumpOut();
    }
    if (ShotId==5 && FilmTime>4.f && !bAction && VehicleInteraction.IsValid() && VehicleInteraction->IsDriving())
        bAction=VehicleInteraction->TryExit();
    MoveCamera();
    if (Controller.IsValid()) Controller->SetViewTarget(FilmCamera);
    if (FilmTime>8.5f && !bReported)
    {
        bReported=true;
        Status=FString::Printf(TEXT("Film shot %d: entered=%d action=%d car enters=%d exits=%d jumps=%d BMX hops=%d landings=%d; %s"),ShotId,bEntered,bAction,
            VehicleInteraction.IsValid()?VehicleInteraction->EnterCount:0,VehicleInteraction.IsValid()?VehicleInteraction->ExitCount:0,VehicleInteraction.IsValid()?VehicleInteraction->JumpOutCount:0,
            BMX.IsValid()?BMX->HopCount:0,BMX.IsValid()?BMX->LandingCount:0,*Status);
        UE_LOG(LogTemp,Display,TEXT("%s"),*Status);
    }
}

void ASAMechanicsFilmDirector::MoveCamera()
{
    if (!FilmCamera) return;
    const float T=FilmTime;
    FVector P, Target;
    float Lens=40.f;
    if (ShotId==0)
    {
        float A=FMath::Clamp(T/7.f,0.f,1.f); A=A*A*(3.f-2.f*A);
        P=FMath::Lerp(FVector(-7100,-4200,150),FVector(-5600,400,260),A);
        Target=FMath::Lerp(FVector(-6700,-2600,100),FVector(-2500,1800,100),A);
        Lens=24.f;
    }
    else if (ShotId==4 && Character.IsValid())
    {
        Target=Character->GetActorLocation()+FVector(0,0,-12);
        P=Target+FVector(-400,-180,100);
        Lens=35.f;
    }
    else if (Car.IsValid())
    {
        const auto X=Car->GetActorTransform();
        Target=X.TransformPosition(FVector(0,-20,85));
        if (ShotId==1 || ShotId==5)
        {
            const float A=FMath::SmoothStep(0.f,7.f,T);
            P=X.TransformPosition(FVector(FMath::Lerp(-220.f,-80.f,A),-590.f,180.f));
            Target=X.TransformPosition(FVector(-10,-95,90));
            Lens=42.f;
        }
        else if (ShotId==3 && bAction && Character.IsValid())
        {
            Target=Character->GetMesh()->GetComponentLocation()+FVector(0,0,45);
            P=Target+FVector(-500,-540,230);
            Lens=35.f;
        }
        else
        {
            P=X.TransformPosition(FVector(-570,-510,170));
            Lens=35.f;
        }
    }
    else return;
    FilmCamera->SetActorLocationAndRotation(P,(Target-P).Rotation());
    auto* C=FilmCamera->GetCineCameraComponent();
    C->SetCurrentFocalLength(Lens);
    C->SetCurrentAperture(8.f);
    C->FocusSettings.FocusMethod=ECameraFocusMethod::Manual;
    C->FocusSettings.ManualFocusDistance=FVector::Distance(P,Target);
}
