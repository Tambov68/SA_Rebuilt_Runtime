#include "SAVehicleInteractionComponent.h"
#include "BMXInteractionComponent.h"
#include "SARideCameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "ChaosVehicleMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

namespace
{
float Ease(float V) { V=FMath::Clamp(V,0.f,1.f);return V*V*(3.f-2.f*V); }
float EntryDoor(float T)
{
    if (T<.2f) return 0.f;
    if (T<.75f) return Ease((T-.2f)/.55f);
    if (T<2.2f) return 1.f;
    return 1.f-Ease((T-2.2f)/.6f);
}
}

USAVehicleInteractionComponent::USAVehicleInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.f;
}
void USAVehicleInteractionComponent::BeginPlay()
{
    Super::BeginPlay();Character=Cast<ACharacter>(GetOwner());
    if (!EnterAnimation) EnterAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Vehicles/F40/Animation/AS_F40_Enter_L.AS_F40_Enter_L"));
    if (!ExitAnimation) ExitAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Vehicles/F40/Animation/AS_F40_Exit_L.AS_F40_Exit_L"));
    if (!DrivingAnimation) DrivingAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Vehicles/F40/Animation/AS_F40_Drive.AS_F40_Drive"));
    if (!JumpAnimation) JumpAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Vehicles/F40/Animation/AS_F40_Jump_L.AS_F40_Jump_L"));
}
void USAVehicleInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TransitionCamera.IsValid()) TransitionCamera->Destroy();
    Super::EndPlay(Reason);
}
bool USAVehicleInteractionComponent::IsDriving() const { return bDriving; }
APawn* USAVehicleInteractionComponent::FindNearestVehicle() const
{
    const ACharacter* Player=Character.Get();
    if (!Player||bDriving||!Player->IsLocallyControlled()) return nullptr;
    APawn* Best=nullptr;float Distance=FMath::Square(InteractionRange);
    for (TActorIterator<APawn> It(GetWorld());It;++It)
    {
        APawn* V=*It;
        if (V->GetController()||!V->FindComponentByClass<UChaosVehicleMovementComponent>()) continue;
        const float D=FVector::DistSquared(Player->GetActorLocation(),V->GetActorLocation());
        if (D<Distance) { Distance=D;Best=V; }
    }
    return Best;
}
bool USAVehicleInteractionComponent::DoorSpaceClear(APawn* Vehicle,int32 Side) const
{
    if (!Vehicle||!Character.IsValid()) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SAVehicleDoor),false,Character.Get());Q.AddIgnoredActor(Vehicle);
    const FTransform T=Vehicle->GetActorTransform();
    const FVector Hinge(66.5f,Side*73.f,50.f);
    FVector Previous=T.TransformPosition(FVector(-30.f,Side*85.f,55.f));
    for (int32 I=1;I<=6;++I)
    {
        const FQuat R(FVector::UpVector,FMath::DegreesToRadians(-Side*DoorOpenDegrees*I/6.f));
        const FVector P=T.TransformPosition(Hinge+R.RotateVector(FVector(-96.5f,Side*12.f,5.f)));
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit,Previous,P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(12.f),Q)) return false;
        Previous=P;
    }
    return true;
}
void USAVehicleInteractionComponent::SetDoor(APawn* Vehicle,int32 Side,float Alpha)
{
    DoorOpenAlpha=FMath::Clamp(Alpha,0.f,1.f);
    if (!Vehicle) return;
    TArray<UStaticMeshComponent*> Parts;Vehicle->GetComponents(Parts);
    const FString Prefix=Side<0?TEXT("Door_L"):TEXT("Door_R");
    for (auto* Part:Parts) if (Part->GetName().StartsWith(Prefix))
        Part->SetRelativeRotation(FRotator(0.f,-Side*DoorOpenDegrees*DoorOpenAlpha,0.f));
}
void USAVehicleInteractionComponent::StartCamera()
{
    if (!DriverController.IsValid()||!IsValid(CurrentVehicle)) return;
    if(State!=ESAVehicleInteractionState::Jumping)
        if(auto* Ride=GetOwner()->FindComponentByClass<USARideCameraComponent>())
            if(Ride->WantsFirstPerson())return;
    if (!TransitionCamera.IsValid())
    {
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
        TransitionCamera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity,Params);
        if (TransitionCamera.IsValid()) TransitionCamera->GetCameraComponent()->SetFieldOfView(60.f);
    }
    UpdateCamera();
    if (TransitionCamera.IsValid()) DriverController->SetViewTargetWithBlend(TransitionCamera.Get(),.22f);
}
void USAVehicleInteractionComponent::UpdateCamera()
{
    if (!TransitionCamera.IsValid()||!IsValid(CurrentVehicle)) return;
    const FTransform T=CurrentVehicle->GetActorTransform();
    const FVector Desired=T.TransformPosition(FVector(190.f,ExitSide*300.f,145.f));
    FVector Look=T.TransformPosition(FVector(-10.f,ExitSide*65.f,68.f));
    if(Character.IsValid())
    {
        auto* Mesh=Character->GetMesh();
        Look=FMath::Lerp(Look,(Mesh->GetSocketLocation(TEXT("pelvis"))+Mesh->GetSocketLocation(TEXT("head")))*.5f,.5f);
    }
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SAVehicleCamera),false,CurrentVehicle);Q.AddIgnoredActor(GetOwner());
    FHitResult Hit;
    const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,Look,Desired,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12.f),Q);
    const FVector P=Blocked?Hit.Location+Hit.ImpactNormal*4.f:Desired;
    TransitionCamera->SetActorLocationAndRotation(P,(Look-P).Rotation());
}
void USAVehicleInteractionComponent::UpdateAnimationCamera()
{
    if(State==ESAVehicleInteractionState::Entering||State==ESAVehicleInteractionState::Exiting||State==ESAVehicleInteractionState::Jumping)UpdateCamera();
}
void USAVehicleInteractionComponent::FinishCamera(AActor* Target)
{
    if(State==ESAVehicleInteractionState::Driving)
        if(auto* Ride=GetOwner()->FindComponentByClass<USARideCameraComponent>())
            if(Ride->WantsFirstPerson())return;
    if (DriverController.IsValid()&&IsValid(Target)) DriverController->SetViewTargetWithBlend(Target,.3f);
    // Keep the outgoing camera alive for the blend; destroy it on the next interaction/end play.
}
void USAVehicleInteractionComponent::SetDriverPose(UAnimSequence* Animation,float Position,const FVector& Warp,bool Mirror)
{
    if (!Character.IsValid()||!Animation) return;
    auto* Mesh=Character->GetMesh();
    if (!Mesh->GetSingleNodeInstance()||Mesh->GetSingleNodeInstance()->GetCurrentAsset()!=Animation)
    { Mesh->PlayAnimation(Animation,false);Mesh->SetPlayRate(0.f); }
    Mesh->SetPosition(FMath::Clamp(Position,0.f,Animation->GetPlayLength()),false);
    FTransform Relative=SavedMeshRelative;
    // Authored poses use the vehicle's origin, +X forward, cm. Manny faces mesh +Y.
    Relative.SetLocation(FVector(0.f,0.f,-93.f));
    FVector Scale=SavedMeshRelative.GetScale3D();if (Mirror) Scale.X*=-1.f;
    Relative.SetScale3D(Scale);
    Mesh->SetRelativeTransform(Relative);
    Character->SetActorRelativeLocation(FVector(0.f,0.f,93.f)+Warp);
    Character->SetActorRelativeRotation(FRotator::ZeroRotator);
}
bool USAVehicleInteractionComponent::TryEnter(APawn* Vehicle)
{
    ACharacter* P=Character.Get();APlayerController* PC=P?Cast<APlayerController>(P->GetController()):nullptr;
    auto* M=IsValid(Vehicle)?Vehicle->FindComponentByClass<UChaosVehicleMovementComponent>():nullptr;
    if (bDriving||!P||!PC||!PC->IsLocalController()||!PC->HasAuthority()||!M||!EnterAnimation||!DrivingAnimation)
    { LastStatus=TEXT("Vehicle entry prerequisites missing");return false; }
    if (Vehicle->GetController()||P->GetCharacterMovement()->MovementMode==MOVE_None||P->GetAttachParentActor())
    { LastStatus=TEXT("Vehicle occupied or player already mounted");return false; }
    if (FVector::DistSquared(P->GetActorLocation(),Vehicle->GetActorLocation())>FMath::Square(InteractionRange))
    { LastStatus=TEXT("Vehicle is too far away");return false; }
    if (Vehicle->GetVelocity().Size()>MaxEnterSpeed)
    { LastStatus=TEXT("Stop the vehicle before entering");return false; }
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SAVehicleEntry),false,P);Q.AddIgnoredActor(Vehicle);
    FHitResult Wall;
    if (GetWorld()->LineTraceSingleByChannel(Wall,P->GetActorLocation(),Vehicle->GetActorLocation()+FVector::UpVector*70.f,ECC_Visibility,Q)||!DoorSpaceClear(Vehicle,-1))
    { LastStatus=TEXT("Driver door or entry path is obstructed");return false; }
    const FVector Local=Vehicle->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
    if (Local.Y>0.f)
    { LastStatus=TEXT("Approach the driver door on the left");return false; }
    SavedCharacterTransform=P->GetActorTransform();SavedControlRotation=PC->GetControlRotation();
    SavedHidden=P->IsHidden();SavedActorCollision=P->GetActorEnableCollision();SavedControllerYaw=P->bUseControllerRotationYaw;
    SavedMovementMode=P->GetCharacterMovement()->MovementMode;SavedCustomMovementMode=P->GetCharacterMovement()->CustomMovementMode;
    auto* Mesh=P->GetMesh();SavedMeshRelative=Mesh->GetRelativeTransform();SavedAnimClass=Mesh->GetAnimClass();
    SavedOwnerNoSee=Mesh->bOwnerNoSee;SavedOnlyOwnerSee=Mesh->bOnlyOwnerSee;
    SavedDisablePostProcess=Mesh->GetDisablePostProcessBlueprint();SavedPauseAnims=Mesh->bPauseAnims;
    HiddenAccessories.Reset();TArray<USceneComponent*> Parts;P->GetComponents(Parts);
    for (auto* Part:Parts) if (Part!=Mesh&&Part->GetName().Contains(TEXT("Weapon")))
    { HiddenAccessories.Emplace(Part,bool(Part->bHiddenInGame));Part->SetHiddenInGame(true,true); }
    EntryWarp=Vehicle->GetActorTransform().InverseTransformPosition(Mesh->GetComponentLocation())-FVector(-15.f,-140.f,-13.f);
    DriverController=PC;CurrentVehicle=Vehicle;ExitSide=-1;
    if(ClosingDoorVehicle.IsValid())SetDoor(ClosingDoorVehicle.Get(),ClosingDoorSide,0.f);
    ClosingDoorVehicle.Reset();
    P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->DisableMovement();
    P->bUseControllerRotationYaw=false;P->SetActorEnableCollision(false);P->SetActorHiddenInGame(false);
    Mesh->SetOwnerNoSee(false);Mesh->SetOnlyOwnerSee(false);Mesh->SetDisablePostProcessBlueprint(true);Mesh->bPauseAnims=false;
    Mesh->AddTickPrerequisiteComponent(this);
    P->AttachToActor(Vehicle,FAttachmentTransformRules::KeepWorldTransform);
    PC->SetIgnoreMoveInput(true);bIgnoreMoveAdded=true;
    // Brake input also requests reverse near rest. Park independently of the pedals.
    M->StopMovementImmediately();M->SetParked(true);
    M->SetThrottleInput(0.f);M->SetSteeringInput(0.f);M->SetBrakeInput(0.f);M->SetHandbrakeInput(true);
    State=ESAVehicleInteractionState::Entering;bDriving=true;TransitionTime=0.f;
    SetDriverPose(EnterAnimation,0.f,EntryWarp);SetDoor(Vehicle,-1,0.f);StartCamera();
    LastStatus=TEXT("Entering: opening door, ducking into seat");return true;
}
void USAVehicleInteractionComponent::FinishEntry()
{
    if (!IsValid(CurrentVehicle)||!DriverController.IsValid()||!Character.IsValid()) return;
    if (bIgnoreMoveAdded) { DriverController->SetIgnoreMoveInput(false);bIgnoreMoveAdded=false; }
    DriverController->Possess(CurrentVehicle);
    if (DriverController->GetPawn()!=CurrentVehicle)
    {
        RestoreCharacter(SavedCharacterTransform.GetLocation(),SavedCharacterTransform.Rotator());
        ClearInteraction();LastStatus=TEXT("Vehicle possession failed");return;
    }
    CurrentVehicle->EnableInput(DriverController.Get());
    auto* M=CurrentVehicle->FindComponentByClass<UChaosVehicleMovementComponent>();
    M->SetParked(false);M->SetBrakeInput(0.f);M->SetHandbrakeInput(false);M->SetThrottleInput(0.f);
    DriverController->SetControlRotation(FRotator(0.f,CurrentVehicle->GetActorRotation().Yaw,0.f));
    VisualSteering=0.f;SetDriverPose(DrivingAnimation,DrivingAnimation->GetPlayLength()*.5f);SetDoor(CurrentVehicle,-1,0.f);
    State=ESAVehicleInteractionState::Driving;++EnterCount;FinishCamera(CurrentVehicle);
    LastStatus=TEXT("Driving: WASD, F exit; F while moving jumps out");
}
bool USAVehicleInteractionComponent::FindExitLocation(APawn* Vehicle,FVector& Out) const
{
    const ACharacter* P=Character.Get();if (!P||!Vehicle) return false;
    const auto* C=P->GetCapsuleComponent();const FQuat R=FRotator(0.f,Vehicle->GetActorRotation().Yaw,0.f).Quaternion();
    FCollisionQueryParams GroundQ(SCENE_QUERY_STAT(SAVehicleExitGround),false,P);GroundQ.AddIgnoredActor(Vehicle);
    FCollisionQueryParams SpaceQ(SCENE_QUERY_STAT(SAVehicleExitSpace),false,P);
    for (const FVector Local:{FVector(-15.f,-ExitSideDistance,0.f),FVector(-15.f,ExitSideDistance,0.f),
        FVector(-15.f,-ExitSideDistance-60.f,0.f),FVector(-15.f,ExitSideDistance+60.f,0.f)})
    {
        if (!DoorSpaceClear(Vehicle,Local.Y<0?-1:1)) continue;
        FVector A=Vehicle->GetActorLocation()+R.RotateVector(Local);FHitResult Ground;
        if (!GetWorld()->LineTraceSingleByChannel(Ground,A+FVector::UpVector*200.f,A-FVector::UpVector*250.f,ECC_Visibility,GroundQ)) continue;
        if (Ground.ImpactNormal.Z<P->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        const FVector Standing=Ground.ImpactPoint+FVector::UpVector*(C->GetScaledCapsuleHalfHeight()+3.f);
        if (GetWorld()->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,
            FCollisionShape::MakeCapsule(C->GetScaledCapsuleRadius(),C->GetScaledCapsuleHalfHeight()),SpaceQ)) continue;
        Out=Standing;return true;
    }
    return false;
}
bool USAVehicleInteractionComponent::TryExit()
{
    if(!ExitAnimation){LastStatus=TEXT("Dedicated exit animation missing");return false;}
    if (State!=ESAVehicleInteractionState::Driving)
    { LastStatus=TEXT("Wait for the current entry or exit animation");return false; }
    if (!IsValid(CurrentVehicle)||!DriverController.IsValid()||DriverController->GetPawn()!=CurrentVehicle) return false;
    if (CurrentVehicle->GetVelocity().Size()>MaxExitSpeed) return TryJumpOut();
    if (!FindExitLocation(CurrentVehicle,PlannedExit))
    { LastStatus=TEXT("No safe exit space: move away from the obstacle");return false; }
    const FVector Local=CurrentVehicle->GetActorTransform().InverseTransformPosition(PlannedExit);
    ExitSide=Local.Y<0.f?-1:1;ExitWarp=Local-FVector(-15.f,ExitSide*180.f,77.f);
    CurrentVehicle->DisableInput(DriverController.Get());
    State=ESAVehicleInteractionState::Exiting;TransitionTime=0.f;StartCamera();
    LastStatus=TEXT("Exiting: opening door and stepping onto the road");return true;
}
void USAVehicleInteractionComponent::RestoreCharacter(const FVector& Location,const FRotator& Rotation)
{
    ACharacter* P=Character.Get();if (!P) return;
    P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    P->SetActorLocationAndRotation(Location,Rotation,false,nullptr,ETeleportType::TeleportPhysics);
    P->bUseControllerRotationYaw=SavedControllerYaw;P->SetActorHiddenInGame(SavedHidden);P->SetActorEnableCollision(SavedActorCollision);
    auto* Mesh=P->GetMesh();Mesh->RemoveTickPrerequisiteComponent(this);Mesh->SetRelativeTransform(SavedMeshRelative);
    Mesh->SetAnimInstanceClass(SavedAnimClass);Mesh->SetOwnerNoSee(SavedOwnerNoSee);Mesh->SetOnlyOwnerSee(SavedOnlyOwnerSee);
    Mesh->SetDisablePostProcessBlueprint(SavedDisablePostProcess);Mesh->bPauseAnims=SavedPauseAnims;
    for (auto& Part:HiddenAccessories) if (Part.Key.IsValid()) Part.Key->SetHiddenInGame(Part.Value,true);
    HiddenAccessories.Reset();
    P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(SavedMovementMode,SavedCustomMovementMode);
    if (DriverController.IsValid()&&DriverController->HasAuthority())
    {
        if (bIgnoreMoveAdded) { DriverController->SetIgnoreMoveInput(false);bIgnoreMoveAdded=false; }
        DriverController->Possess(P);
        FRotator View(SavedControlRotation.Pitch,Rotation.Yaw,0.f);
        if(auto* Ride=P->FindComponentByClass<USARideCameraComponent>())if(Ride->bFirstPerson)View=Ride->GetLookRotation();
        View.Roll=0.f;DriverController->SetControlRotation(View);
        DriverController->SetInputMode(FInputModeGameOnly());DriverController->bShowMouseCursor=false;
    }
    bDriving=false;
}
void USAVehicleInteractionComponent::ClearInteraction()
{
    CurrentVehicle=nullptr;DriverController.Reset();State=ESAVehicleInteractionState::OnFoot;TransitionTime=0.f;
}
void USAVehicleInteractionComponent::FinishExit()
{
    FVector Confirmed;
    if (!FindExitLocation(CurrentVehicle,Confirmed)||
        FVector::DistSquared(Confirmed,PlannedExit)>FMath::Square(60.f))
    {
        SetDoor(CurrentVehicle,ExitSide,0.f);ExitSide=-1;SetDriverPose(DrivingAnimation,DrivingAnimation->GetPlayLength()*.5f);
        if (auto* M=CurrentVehicle->FindComponentByClass<UChaosVehicleMovementComponent>())
        { M->SetParked(false);M->SetBrakeInput(0.f);M->SetHandbrakeInput(false); }
        CurrentVehicle->EnableInput(DriverController.Get());State=ESAVehicleInteractionState::Driving;
        FinishCamera(CurrentVehicle);LastStatus=TEXT("Exit became obstructed; remain seated");return;
    }
    if (auto* M=CurrentVehicle->FindComponentByClass<UChaosVehicleMovementComponent>())
    {
        // Clear interpolated and replicated inputs while the driver still owns the car.
        // Raw pedal setters alone leave the last replicated throttle active after unpossess.
        M->StopMovementImmediately();M->SetParked(true);
    }
    SetDoor(CurrentVehicle,ExitSide,0.f);
    const FRotator Yaw(0.f,CurrentVehicle->GetActorRotation().Yaw,0.f);
    RestoreCharacter(Confirmed,Yaw);LastExitLocation=Confirmed;++ExitCount;FinishCamera(Character.Get());
    ClearInteraction();LastStatus=TEXT("On foot: F enter car or bicycle");
}
bool USAVehicleInteractionComponent::TryJumpOut()
{
    if (State!=ESAVehicleInteractionState::Driving||!IsValid(CurrentVehicle)||!Character.IsValid()||!JumpAnimation||!bAllowJumpOut)
    { LastStatus=TEXT("Jump-out prerequisites missing");return false; }
    auto* BMX=Character->FindComponentByClass<UBMXInteractionComponent>();
    if (!BMX||!Character->GetMesh()->GetPhysicsAsset())
    { LastStatus=TEXT("Jump-out needs character ragdoll and recovery");return false; }
    ExitSide=-1;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(SAVehicleJump),false,Character.Get());Q.AddIgnoredActor(CurrentVehicle);
    const FVector Start=CurrentVehicle->GetActorTransform().TransformPosition(FVector(-10.f,-70.f,70.f));
    const FVector End=CurrentVehicle->GetActorTransform().TransformPosition(FVector(-10.f,-185.f,70.f));
    FHitResult Wall;
    if (!DoorSpaceClear(CurrentVehicle,-1)||GetWorld()->SweepSingleByChannel(Wall,Start,End,FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(32.f,55.f),Q))
    { LastStatus=TEXT("Jump path is obstructed");return false; }
    CurrentVehicle->DisableInput(DriverController.Get());
    State=ESAVehicleInteractionState::Jumping;TransitionTime=0.f;StartCamera();
    LastStatus=TEXT("Jumping out: pushing through driver door");return true;
}
void USAVehicleInteractionComponent::ReleaseJump()
{
    ACharacter* P=Character.Get();APawn* V=CurrentVehicle;
    auto* Mesh=P->GetMesh();auto* BMX=P->FindComponentByClass<UBMXInteractionComponent>();
    // Finalize the launch pose before taking its world transform and enabling rigid bodies.
    SetDriverPose(JumpAnimation,JumpAnimation->GetPlayLength());Mesh->TickAnimation(0.f,false);Mesh->RefreshBoneTransforms();
    const FTransform ReleaseMeshWorld=Mesh->GetComponentTransform();const FVector Pelvis=Mesh->GetSocketLocation(TEXT("pelvis"));
    FVector Velocity=V->GetVelocity();
    if (auto* Root=Cast<UPrimitiveComponent>(V->GetRootComponent())) Velocity=Root->GetPhysicsLinearVelocityAtPoint(Pelvis);
    LastReleaseVelocity=Velocity-V->GetActorRightVector()*JumpSideSpeed+FVector::UpVector*JumpUpSpeed;
    const FRotator Yaw(0.f,V->GetActorRotation().Yaw,0.f);
    if (auto* M=V->FindComponentByClass<UChaosVehicleMovementComponent>())
    { M->SetParked(false);M->SetThrottleInput(0.f);M->SetBrakeInput(0.f);M->SetSteeringInput(0.f);M->SetHandbrakeInput(false); }
    ClosingDoorVehicle=V;ClosingDoorSide=-1;SetDoor(V,-1,1.f);
    RestoreCharacter(Pelvis,Yaw);FinishCamera(P);
    const bool Released=BMX->BeginExternalRagdoll(LastReleaseVelocity,JumpAnimation,JumpAnimation->GetPlayLength(),ReleaseMeshWorld);
    ++JumpOutCount;++ExitCount;LastExitLocation=Pelvis;ClearInteraction();
    LastStatus=Released?TEXT("Jumped out: inherited vehicle velocity; F recover after settling"):TEXT("Release failed; character control restored");
}
void USAVehicleInteractionComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Dt,Type,Function);
    if (!bDriving)
    {
        if (ClosingDoorVehicle.IsValid())
        { SetDoor(ClosingDoorVehicle.Get(),ClosingDoorSide,FMath::Max(0.f,DoorOpenAlpha-Dt*1.6f));if (DoorOpenAlpha<=0.f) ClosingDoorVehicle.Reset(); }
        return;
    }
    const bool Entering=State==ESAVehicleInteractionState::Entering;
    if (!IsValid(CurrentVehicle)||!DriverController.IsValid()||
        DriverController->GetPawn()!=(Entering?static_cast<APawn*>(Character.Get()):CurrentVehicle.Get()))
    {
        SetDoor(CurrentVehicle,ExitSide,0.f);RestoreCharacter(SavedCharacterTransform.GetLocation(),SavedCharacterTransform.Rotator());
        FinishCamera(Character.Get());ClearInteraction();LastStatus=TEXT("Vehicle control lost; character restored");return;
    }
    if (State==ESAVehicleInteractionState::Driving){UpdateDrivingPose(Dt);return;}
    TransitionTime+=Dt;
    auto* M=CurrentVehicle->FindComponentByClass<UChaosVehicleMovementComponent>();
    M->SetThrottleInput(0.f);M->SetSteeringInput(0.f);
    const bool Jumping=State==ESAVehicleInteractionState::Jumping;
    M->SetBrakeInput(0.f);M->SetParked(!Jumping);M->SetHandbrakeInput(!Jumping);
    if (Entering)
    {
        SetDoor(CurrentVehicle,-1,EntryDoor(TransitionTime));SetDriverPose(EnterAnimation,TransitionTime,EntryWarp*(1.f-Ease(TransitionTime/.35f)));
        if (TransitionTime>=EnterAnimation->GetPlayLength()) FinishEntry();
    }
    else if (State==ESAVehicleInteractionState::Exiting)
    {
        const float Length=ExitAnimation->GetPlayLength();
        const float Across=ExitSide>0?.35f:0.f;
        const float T=FMath::Max(0.f,TransitionTime-Across);
        const float Pos=FMath::Min(Length,T);
        FVector Warp=ExitWarp*Ease((T-.7f)/1.7f);
        if (ExitSide>0) Warp.Y-=64.f*(1.f-Ease(TransitionTime/.35f));
        SetDriverPose(ExitAnimation,Pos,Warp,ExitSide>0);
        SetDoor(CurrentVehicle,ExitSide,Ease((T-.25f)/.65f)*(1.f-Ease((T-2.65f)/.65f)));
        if (T>=Length) FinishExit();
    }
    else if (Jumping)
    {
        SetDoor(CurrentVehicle,-1,Ease(TransitionTime/.25f));SetDriverPose(JumpAnimation,TransitionTime);
        if (TransitionTime>=JumpAnimation->GetPlayLength()) ReleaseJump();
    }
}
void USAVehicleInteractionComponent::UpdateDrivingPose(float DeltaTime)
{
    if(!CurrentVehicle||!DrivingAnimation)return;
    auto* Movement=CurrentVehicle->FindComponentByClass<UChaosVehicleMovementComponent>();
    if(!Movement)return;
    VisualSteering=FMath::FInterpTo(VisualSteering,Movement->GetSteeringInput(),DeltaTime,8.f);
    // The pose axis was authored with two-bone IK at each steering sample.
    // Sampling the same normalized axis keeps the hands and rim synchronized.
    SetDriverPose(DrivingAnimation,(VisualSteering+1.f)*.5f*DrivingAnimation->GetPlayLength());
    TArray<UStaticMeshComponent*> Parts;CurrentVehicle->GetComponents(Parts);
    for(auto* Part:Parts)if(Part->GetName().StartsWith(TEXT("F40_SteeringWheel")))
        Part->SetRelativeRotation(FQuat(FVector(.804467f,.089021f,-.587289f).GetSafeNormal(),FMath::DegreesToRadians(VisualSteering*SteeringWheelDegrees)));
}
