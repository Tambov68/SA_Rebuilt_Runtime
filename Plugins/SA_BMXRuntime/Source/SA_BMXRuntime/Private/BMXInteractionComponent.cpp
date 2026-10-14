#include "BMXInteractionComponent.h"
#include "SAVehicleInteractionComponent.h"
#include "SARideCameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
template<class T> T* Part(AActor* Actor, FName Name)
{
    if (!IsValid(Actor)) return nullptr;
    TArray<T*> Parts; Actor->GetComponents<T>(Parts);
    for (T* P : Parts) if (P->GetFName() == Name) return P;
    return nullptr;
}
bool BoolValue(UObject* Object, FName Name, bool Fallback=false)
{
    const FBoolProperty* P = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
    return P ? P->GetPropertyValue_InContainer(Object) : Fallback;
}
template<class T> void PoseValue(UAnimInstance* Anim, FName Name, const T& Value)
{
    FStructProperty* P = Anim ? FindFProperty<FStructProperty>(Anim->GetClass(),Name) : nullptr;
    if (P && P->Struct == TBaseStructure<T>::Get()) *P->ContainerPtrToValuePtr<T>(Anim) = Value;
}
FQuat GroundRotation(const FVector& Forward, const FVector& Normal)
{
    // Preserve the intended compass heading. Repeated plane projection of the
    // previous tilted forward vector otherwise steers the bike on side slopes.
    FVector X=Forward.GetSafeNormal2D();
    if (X.IsNearlyZero()) X=FVector::ForwardVector;
    X.Z=-(X.X*Normal.X+X.Y*Normal.Y)/FMath::Max(Normal.Z,0.1f);
    X.Normalize();
    return FRotationMatrix::MakeFromXZ(X,Normal).ToQuat();
}
// Separate horizontal surfaces must not be interpreted as a connecting ramp.
bool CoherentWheelSupport(const FHitResult& Front, const FHitResult& Rear)
{
    const FVector Span=Front.Location-Rear.Location;
    return FVector::DotProduct(Front.ImpactNormal,Rear.ImpactNormal)>0.7f
        && FMath::Abs(FVector::DotProduct(Span,Front.ImpactNormal))<=12.f
        && FMath::Abs(FVector::DotProduct(Span,Rear.ImpactNormal))<=12.f;
}
float HopPitch(float Time)
{
    // Lift the front first, then bring the rear up and level for landing.
    if (Time<0.16f) return 18.f*FMath::Clamp(Time/0.16f,0.f,1.f);
    return 18.f*(1.f-FMath::Clamp((Time-0.16f)/0.24f,0.f,1.f));
}
FVector SupportedRoot(FVector Position, const FHitResult& Ground)
{
    // Keep XY fixed: adding the normal's XY every frame causes downhill drift.
    const FVector N=Ground.ImpactNormal;
    Position.Z=(FVector::DotProduct(N,Ground.ImpactPoint)+54.55f-N.X*Position.X-N.Y*Position.Y)/N.Z;
    return Position;
}
void ResetPedals(AActor* Bike)
{
    if (auto* Cycle=Part<URotatingMovementComponent>(Bike,"BMX_PedalCycle")) Cycle->Deactivate();
    if (auto* Crank=Part<USceneComponent>(Bike,"BMX_Crank")) Crank->SetRelativeRotation(FRotator::ZeroRotator);
    if (auto* Steering=Part<USceneComponent>(Bike,"BMX_SteeringPivot")) Steering->SetRelativeRotation(FRotator::ZeroRotator);
    for (FName Name : {FName("BMX_Pedal_L"),FName("BMX_Pedal_R")})
        if (auto* Pedal=Part<USceneComponent>(Bike,Name)) { Pedal->SetAbsolute(false,false,false); Pedal->SetRelativeRotation(FRotator::ZeroRotator); }
    for (FName Name : {FName("BMX_Wheel_F"),FName("BMX_Wheel_R")})
        if (auto* Wheel=Part<USceneComponent>(Bike,Name)) Wheel->SetRelativeRotation(FRotator::ZeroRotator);
}
}

UBMXVehicleComponent::UBMXVehicleComponent() { PrimaryComponentTick.bCanEverTick=false; }
UStaticMeshComponent* UBMXVehicleComponent::GetFrame() const { return Part<UStaticMeshComponent>(GetOwner(),"BMX_Frame"); }
void UBMXVehicleComponent::BeginPlay()
{
    Super::BeginPlay(); State=EBMXState::Dropped;
    if (bStartDropped) DropBike(FVector::ZeroVector);
}
void UBMXVehicleComponent::DropBike(FVector Velocity)
{
    ResetPedals(GetOwner()); Rider=nullptr; State=EBMXState::Dropped;
    if (auto* Frame=GetFrame())
    {
        Frame->SetSimulatePhysics(true);
        Frame->SetPhysicsLinearVelocity(Velocity);
        // A small release wobble avoids balancing a perfectly symmetric bike upright.
        Frame->SetPhysicsAngularVelocityInDegrees(GetOwner()->GetActorForwardVector()*18.f);
        Frame->WakeAllRigidBodies();
    }
}
UBMXInteractionComponent::UBMXInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
void UBMXInteractionComponent::BeginPlay()
{
    Super::BeginPlay(); Character=Cast<ACharacter>(GetOwner()); InstallInput();
    if(Character.IsValid()&&!GetOwner()->FindComponentByClass<USARideCameraComponent>())
    {
        auto* RideCamera=NewObject<USARideCameraComponent>(GetOwner(),TEXT("RideFirstPersonCamera"));
        GetOwner()->AddInstanceComponent(RideCamera);RideCamera->RegisterComponent();
    }
}
void UBMXInteractionComponent::InstallInput()
{
    if (bInputInstalled || !Character.IsValid()) return;
    PlayerController=Cast<APlayerController>(Character->GetController());
    if (!PlayerController.IsValid() || !PlayerController->IsLocalController()) return;
    BikeInput=NewObject<UInputComponent>(GetOwner(),TEXT("BMXInteractionInput"));
    BikeInput->RegisterComponent(); BikeInput->Priority=50;
    BikeInput->BindKey(EKeys::F,IE_Pressed,this,&UBMXInteractionComponent::Interact).bConsumeInput=true;
    BikeInput->BindKey(EKeys::SpaceBar,IE_Pressed,this,&UBMXInteractionComponent::JumpPressed).bConsumeInput=false;
    BikeInput->BindKey(EKeys::SpaceBar,IE_Released,this,&UBMXInteractionComponent::JumpReleased).bConsumeInput=false;
    BikeInput->BindAxisKey(EKeys::MouseX,this,&UBMXInteractionComponent::MouseYaw).bConsumeInput=false;
    BikeInput->BindAxisKey(EKeys::MouseY,this,&UBMXInteractionComponent::MousePitch).bConsumeInput=false;
    PlayerController->PushInputComponent(BikeInput); bInputInstalled=true;
}
void UBMXInteractionComponent::MouseYaw(float Value) { if ((IsMounted()||State==EBMXState::Ragdoll) && PlayerController.IsValid()) {PlayerController->AddYawInput(Value);if (!FMath::IsNearlyZero(Value)) LastManualLookTime=GetWorld()->GetTimeSeconds();} }
void UBMXInteractionComponent::MousePitch(float Value) { if ((IsMounted()||State==EBMXState::Ragdoll) && PlayerController.IsValid()) {PlayerController->AddPitchInput(-Value);if (!FMath::IsNearlyZero(Value)) LastManualLookTime=GetWorld()->GetTimeSeconds();} }
void UBMXInteractionComponent::SetRideInput(float Throttle,float Steering)
{
    ThrottleInput=FMath::Clamp(Throttle,-1.f,1.f); SteeringInput=FMath::Clamp(Steering,-1.f,1.f); bInjectedRideInput=true;
}
void UBMXInteractionComponent::SetBrakeInput(bool bBrake) {BrakeInput=bBrake;bInjectedBrakeInput=true;}
void UBMXInteractionComponent::SetSprintInput(bool bSprint) {SprintInput=bSprint;bInjectedSprintInput=true;}
void UBMXInteractionComponent::SetWheelieInput(bool bWheelie) {WheelieInput=bWheelie;bInjectedWheelieInput=true;}
void UBMXInteractionComponent::SetStoppieInput(bool bStoppie) {StoppieInput=bStoppie;bInjectedStoppieInput=true;}
void UBMXInteractionComponent::SetJumpInput(bool bJump) {JumpInput=bJump;bInjectedJumpInput=true;}
void UBMXInteractionComponent::JumpPressed()
{
    if (IsMounted()&&!bInjectedJumpInput) {bNativeJumpDown=true;bJumpPressPending=true;}
}
void UBMXInteractionComponent::JumpReleased() {bNativeJumpDown=false;}
void UBMXInteractionComponent::ResetHop()
{
    HopState=EBMXHopState::Grounded;HopCharge=0.f;HopVelocity=FVector::ZeroVector;HopAirTime=0.f;
    JumpInput=false;bInjectedJumpInput=false;bJumpWasDown=false;bNativeJumpDown=false;bJumpPressPending=false;
    SprintInput=false;bInjectedSprintInput=false;LastSprint=false;
    RiderLandingBlend=0.f;RiderSpineMotion=FRotator(0,0,25);RiderHeadMotion=FRotator(0,0,-15);
    RiderStandBlend=0.f;RiderPoseOffset=FVector::ZeroVector;RiderMountBlend=0.f;bDismountPreparing=false;DismountPrepareTime=0.f;
}
bool UBMXInteractionComponent::StepHeldRide(float Duration)
{
    if (IsComponentTickEnabled()||!IsMounted()||Duration<=0.f||Duration>2.f) return false;
    const int32 Steps=FMath::CeilToInt(Duration*120.f);
    for (int32 I=0;I<Steps&&IsMounted();++I) RideTick(Duration/Steps);
    if (Character.IsValid()) UpdateRiderIK(Character->GetMesh()->GetAnimInstance());
    return true;
}
void UBMXInteractionComponent::Interact()
{
    auto* CarInteraction=GetOwner()->FindComponentByClass<USAVehicleInteractionComponent>();
    if (CarInteraction && CarInteraction->IsDriving())
    {
        CarInteraction->TryExit(); LastStatus=CarInteraction->LastStatus; return;
    }
    if (State==EBMXState::Ragdoll) {TryRecover();return;}
    if (State==EBMXState::Raising) return;
    if (IsMounted()) { TryDismount(); return; }
    AActor* Best=nullptr; float DistanceSq=FMath::Square(InteractionRange);
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        auto* V=It->FindComponentByClass<UBMXVehicleComponent>();
        if (!V || !V->bAllowInteraction || V->Rider || V->State==EBMXState::Raising) continue;
        const float D=FVector::DistSquared(GetOwner()->GetActorLocation(),It->GetActorLocation());
        if (D<DistanceSq) { DistanceSq=D;Best=*It; }
    }
    APawn* Car=CarInteraction ? CarInteraction->FindNearestVehicle() : nullptr;
    if (Car && (!Best || FVector::DistSquared(GetOwner()->GetActorLocation(),Car->GetActorLocation())<DistanceSq))
    {
        CarInteraction->TryEnter(Car); LastStatus=CarInteraction->LastStatus; return;
    }
    if (Best) TryMount(Best); else LastStatus=TEXT("No vehicle in interaction range");
}
bool UBMXInteractionComponent::GroundUnderCrank(AActor* Bike,FHitResult& Hit) const
{
    auto* Crank=Part<USceneComponent>(Bike,"BMX_Crank"); if (!Crank) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BMXGround),false,GetOwner()); Q.AddIgnoredActor(Bike);
    const FVector P=Crank->GetComponentLocation();
    return GetWorld()->LineTraceSingleByChannel(Hit,P+FVector::UpVector*140.f,P-FVector::UpVector*250.f,ECC_Visibility,Q)
        && Hit.ImpactNormal.Z>=FMath::Cos(FMath::DegreesToRadians(35.f));
}
bool UBMXInteractionComponent::BikePoseClear(AActor* Bike,const FTransform& Pose) const
{
    auto* Frame=Part<UStaticMeshComponent>(Bike,"BMX_Frame"); if (!Frame) return false;
    FComponentQueryParams Q(TEXT("BMXRaiseClear"),GetOwner()); Q.AddIgnoredActor(Bike);
    TArray<FOverlapResult> Hits;
    GetWorld()->ComponentOverlapMultiByChannel(Hits,Frame,Pose.GetLocation(),Pose.GetRotation(),ECC_PhysicsBody,Q);
    for (const FOverlapResult& H : Hits) if (H.bBlockingHit) { UE_LOG(LogTemp,Display,TEXT("BMX raise blocked by %s at %s"),*GetNameSafe(H.GetActor()),*Pose.GetLocation().ToCompactString());return false; }
    return true;
}
bool UBMXInteractionComponent::TryMount(AActor* Bike)
{
    if (!Character.IsValid() || State!=EBMXState::Dropped || !IsValid(Bike) || !RiderAnimClass) { LastStatus=TEXT("Mount prerequisites missing"); return false; }
    auto* Vehicle=Bike->FindComponentByClass<UBMXVehicleComponent>(); auto* Frame=Part<UStaticMeshComponent>(Bike,"BMX_Frame");
    if (!Vehicle || !Vehicle->bAllowInteraction || Vehicle->Rider || !Frame || !Frame->DoesSocketExist("Socket_Seat")
        || FVector::DistSquared(Character->GetActorLocation(),Bike->GetActorLocation())>FMath::Square(InteractionRange)) { LastStatus=TEXT("Bike unavailable or too far");return false; }
    for (FName Name : {FName("BMX_Steering"),FName("BMX_Pedal_L"),FName("BMX_Pedal_R")})
        if (!Part<USceneComponent>(Bike,Name)) {LastStatus=TEXT("Bike contact components missing");return false;}
    FHitResult Ground;
    if (!GroundUnderCrank(Bike,Ground)) { LastStatus=TEXT("No walkable ground below crank");return false; }
    LastGroundNormal=Ground.ImpactNormal;
    const FQuat Rotation=GroundRotation(Bike->GetActorForwardVector(),LastGroundNormal);
    // Crank local X=-10.1344cm, tyre lowest Z=-53.5cm. Add 1cm clearance.
    const FVector Location=SupportedRoot(Bike->GetActorLocation(),Ground);
    RaiseTo=FTransform(Rotation,Location,Frame->GetComponentScale());
    // Check the entire interpolated body path before disabling its simulation.
    RaiseFrom=Bike->GetActorTransform();
    for (int32 I=1;I<=12;++I)
    {
        const float A=I/12.f; FTransform Pose(FQuat::Slerp(RaiseFrom.GetRotation(),Rotation,A),FMath::Lerp(RaiseFrom.GetLocation(),Location,A)+LastGroundNormal*(24.f*FMath::Sin(PI*A)),RaiseTo.GetScale3D());
        if (!BikePoseClear(Bike,Pose)) {LastStatus=TEXT("Blocked bike lifting path");return false;}
    }
    CurrentBike=Bike; Vehicle->State=EBMXState::Raising;Vehicle->Rider=Character.Get();State=EBMXState::Raising;RaiseTime=0.f;
    Frame->SetSimulatePhysics(false);ResetPedals(Bike);RideSpeed=0.f;
    LastStatus=TEXT("Raising BMX along ground normal");return true;
}
void UBMXInteractionComponent::CancelRaise(const FString& Reason)
{
    if (IsValid(CurrentBike)) if (auto* V=CurrentBike->FindComponentByClass<UBMXVehicleComponent>()) V->DropBike(FVector::ZeroVector);
    CurrentBike=nullptr;State=EBMXState::Dropped;LastStatus=Reason;
}
void UBMXInteractionComponent::ApplyCamera(bool bThirdPerson) const
{
    if (UFunction* F=GetOwner()->FindFunction(TEXT("ApplyCameraView")))
    { struct { bool ThirdPerson; } Params{bThirdPerson};GetOwner()->ProcessEvent(F,&Params); }
}
void UBMXInteractionComponent::CompleteMount()
{
    ResetHop();
    ACharacter* C=Character.Get(); auto* V=IsValid(CurrentBike)?CurrentBike->FindComponentByClass<UBMXVehicleComponent>():nullptr;
    auto* Frame=V?V->GetFrame():nullptr;
    if (!C || !V || !Frame) {CancelRaise(TEXT("Bike disappeared while raising"));return;}
    USkeletalMeshComponent* Mesh=C->GetMesh(); auto* Movement=C->GetCharacterMovement(); auto* Capsule=C->GetCapsuleComponent();
    SavedAnimClass=Mesh->GetAnimClass();SavedMeshRelative=Mesh->GetRelativeTransform();
    SavedCapsuleCollision=Capsule->GetCollisionEnabled();SavedMeshCollision=Mesh->GetCollisionEnabled();
    SavedMeshProfile=Mesh->GetCollisionProfileName();SavedMeshResponses=Mesh->GetCollisionResponseToChannels();SavedMeshObjectType=Mesh->GetCollisionObjectType();
    SavedMovementMode=Movement->MovementMode;SavedCustomMovementMode=Movement->CustomMovementMode;
    SavedControllerYaw=C->bUseControllerRotationYaw;SavedThirdPerson=BoolValue(C,"IsThirdPerson",true);
    SavedWeaponEquipped=BoolValue(C,"bWeaponEquipped");
    if (SavedWeaponEquipped) if (UFunction* F=C->FindFunction(TEXT("ToggleWeapon"))) C->ProcessEvent(F,nullptr);
    C->UnCrouch(); Movement->StopMovementImmediately();Movement->DisableMovement();
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    C->AttachToComponent(Frame,FAttachmentTransformRules::SnapToTargetNotIncludingScale,TEXT("Socket_Seat"));
    // Match the existing test rig's pelvis (-8cm aft,+9cm up) using this mesh's reference skeleton.
    FVector Pelvis=FVector::ZeroVector;
    if (USkeletalMesh* Asset=Mesh->GetSkeletalMeshAsset())
    {
        const auto& Ref=Asset->GetRefSkeleton(); int32 Bone=Ref.FindBoneIndex(TEXT("pelvis"));
        FTransform T=FTransform::Identity;
        while (Bone!=INDEX_NONE) { T=T*Ref.GetRefBonePose()[Bone];Bone=Ref.GetParentIndex(Bone); }
        Pelvis=Mesh->GetRelativeTransform().TransformPosition(T.GetLocation());
    }
    C->SetActorRelativeLocation(FVector(-8,0,9)-Pelvis);C->SetActorRelativeRotation(FRotator::ZeroRotator);
    RiderSeatLocation=C->GetRootComponent()->GetRelativeLocation();WheelieRootOffset=FVector::ZeroVector;WheelieDegrees=0.f;LastWheelie=false;
    StoppieDegrees=0.f;LastStoppie=false;bStoppieLatched=false;StoppieTime=0.f;
    if (PlayerController.IsValid()) SavedControlRotation=PlayerController->GetControlRotation();
    ApplyCamera(true);C->bUseControllerRotationYaw=false;
    if (PlayerController.IsValid()) PlayerController->SetControlRotation(FRotator(-12.f,CurrentBike->GetActorRotation().Yaw,0.f));
    Mesh->SetAnimInstanceClass(RiderAnimClass); Mesh->AddTickPrerequisiteComponent(this);
    V->State=EBMXState::Mounted;V->Rider=C;State=EBMXState::Mounted;PedalAngle=0.f;PedalPhaseDegrees=0.f;WheelPhaseDegrees=0.f;LeanDegrees=0.f;
    LastManualLookTime=GetWorld()->GetTimeSeconds();
    if (PlayerController.IsValid()) {PlayerController->SetIgnoreMoveInput(true);bMoveIgnoreAdded=true;}
    if (BikeInput) BikeInput->bBlockInput=true;
    Frame->SetNotifyRigidBodyCollision(true);
    Frame->OnComponentHit.AddUniqueDynamic(this,&UBMXInteractionComponent::OnBikeHit);
    RiderMountBlend=1.f;
    UpdateRiderMotion(0.f);
    UpdateRiderIK(Mesh->GetAnimInstance()); LastStatus=TEXT("Mounted: WASD ride, mouse look, F exit");
}
bool UBMXInteractionComponent::FindExit(FVector& Location) const
{
    ACharacter* C=Character.Get(); if (!C || !IsValid(CurrentBike)) return false;
    float Radius,HalfHeight;C->GetCapsuleComponent()->GetScaledCapsuleSize(Radius,HalfHeight);
    const FVector P=CurrentBike->GetActorLocation();const FVector Right=CurrentBike->GetActorRightVector().GetSafeNormal2D();const FVector Forward=CurrentBike->GetActorForwardVector().GetSafeNormal2D();
    FCollisionQueryParams GroundQ(SCENE_QUERY_STAT(BMXExitGround),false,C);GroundQ.AddIgnoredActor(CurrentBike);
    FCollisionQueryParams SpaceQ(SCENE_QUERY_STAT(BMXExitSpace),false,C);
    FCollisionQueryParams PathQ(SCENE_QUERY_STAT(BMXExitPath),false,C);PathQ.AddIgnoredActor(CurrentBike);
    const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,HalfHeight);
    // The capsule uses a Custom profile in BP_Player. Query its actual responses,
    // rather than looking up a nonexistent profile named "Custom".
    const ECollisionChannel Channel=C->GetCapsuleComponent()->GetCollisionObjectType();
    const FCollisionResponseParams Responses(C->GetCapsuleComponent()->GetCollisionResponseToChannels());
    for (const FVector Offset : {Right*110.f,-Right*110.f,Right*140.f-Forward*65.f,-Right*140.f-Forward*65.f})
    {
        FHitResult Ground;FVector Candidate=P+Offset;
        if (!GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector::UpVector*150.f,Candidate-FVector::UpVector*250.f,ECC_Visibility,GroundQ)
            || Ground.ImpactNormal.Z<C->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        Candidate=Ground.ImpactPoint+FVector::UpVector*(HalfHeight+2.f);
        if (FMath::Abs(Ground.ImpactPoint.Z-(P.Z-54.55f))>50.f) continue;
        if (GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,Channel,Capsule,SpaceQ,Responses)) continue;
        FVector From=C->GetActorLocation();From.Z=FMath::Max(From.Z,Candidate.Z);
        FHitResult Block;
        if (GetWorld()->SweepSingleByChannel(Block,From,Candidate,FQuat::Identity,Channel,Capsule,PathQ,Responses))
        {
            if (!Block.bStartPenetrating) continue;
            // Seated riders have no capsule collision. Expanding to a standing
            // capsule beside stairs can therefore start inside their edge even
            // when the seated body is clear. Permit only leaving those initial
            // overlaps: the body centre must have a clear path, no new blocking
            // component may be entered, and after becoming clear the complete
            // standing capsule must sweep safely all the way to the exit.
            FHitResult CentreHit;
            if (GetWorld()->LineTraceSingleByChannel(CentreHit,From,Candidate,Channel,PathQ,Responses)) continue;
            TArray<FOverlapResult> Initial;
            GetWorld()->OverlapMultiByChannel(Initial,From,FQuat::Identity,Channel,Capsule,PathQ,Responses);
            TSet<const UPrimitiveComponent*> InitialBlockers;
            for (const FOverlapResult& H : Initial) if (H.bBlockingHit) InitialBlockers.Add(H.GetComponent());
            if (InitialBlockers.IsEmpty()) continue;
            const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Distance(From,Candidate)/5.f));
            bool bClearPath=false;
            for (int32 Step=1;Step<=Steps;++Step)
            {
                const FVector Sample=FMath::Lerp(From,Candidate,Step/static_cast<float>(Steps));
                TArray<FOverlapResult> Overlaps;
                GetWorld()->OverlapMultiByChannel(Overlaps,Sample,FQuat::Identity,Channel,Capsule,PathQ,Responses);
                bool bStillOverlapping=false,bNewBlocker=false;
                for (const FOverlapResult& H : Overlaps) if (H.bBlockingHit)
                {
                    bStillOverlapping=true;
                    if (!InitialBlockers.Contains(H.GetComponent())) bNewBlocker=true;
                }
                if (bNewBlocker) break;
                if (!bStillOverlapping)
                {
                    FHitResult RemainingHit;
                    bClearPath=!GetWorld()->SweepSingleByChannel(RemainingHit,Sample,Candidate,FQuat::Identity,Channel,Capsule,PathQ,Responses);
                    break;
                }
            }
            if (!bClearPath) continue;
        }
        Location=Candidate;return true;
    }
    return false;
}
void UBMXInteractionComponent::RestoreCharacter(const FVector& ExitLocation)
{
    ACharacter* C=Character.Get();if (!C) return;
    C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    C->SetActorLocationAndRotation(ExitLocation,FRotator(0,C->GetActorRotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    C->GetMesh()->RemoveTickPrerequisiteComponent(this);C->GetMesh()->SetRelativeTransform(SavedMeshRelative);C->GetMesh()->SetAnimInstanceClass(SavedAnimClass);
    C->GetMesh()->SetCollisionProfileName(SavedMeshProfile);
    C->GetMesh()->SetCollisionObjectType(SavedMeshObjectType);C->GetMesh()->SetCollisionResponseToChannels(SavedMeshResponses);
    C->GetCapsuleComponent()->SetCollisionEnabled(SavedCapsuleCollision);C->GetMesh()->SetCollisionEnabled(SavedMeshCollision);
    auto* M=C->GetCharacterMovement();M->StopMovementImmediately();M->SetMovementMode(SavedMovementMode==MOVE_None?MOVE_Walking:SavedMovementMode.GetValue(),SavedCustomMovementMode);
    ApplyCamera(SavedThirdPerson);C->bUseControllerRotationYaw=SavedControllerYaw;
    if (PlayerController.IsValid()) PlayerController->SetControlRotation(SavedControlRotation);
    if (bMoveIgnoreAdded && PlayerController.IsValid()) PlayerController->SetIgnoreMoveInput(false);
    bMoveIgnoreAdded=false;if (BikeInput) BikeInput->bBlockInput=false;
    if (SavedWeaponEquipped) if (UFunction* F=C->FindFunction(TEXT("ToggleWeapon"))) C->ProcessEvent(F,nullptr);
}
bool UBMXInteractionComponent::TryDismount()
{
    if (!IsMounted()) return false;
    if (bDismountPreparing) return false;
    if (HopState==EBMXHopState::Airborne) {LastStatus=TEXT("Land before dismounting");return false;}
    FVector Exit;
    if (!FindExit(Exit)) {LastStatus=TEXT("Exit blocked: remain mounted");return false;}
    // Keep the rider attached during this short weight shift. Recheck the exit
    // at release: a moving obstacle may have occupied it during preparation.
    bDismountPreparing=true;DismountPrepareTime=0.f;
    HopState=EBMXHopState::Grounded;HopCharge=0.f;
    LastStatus=TEXT("Preparing dismount; exit will be checked again");return true;
}
bool UBMXInteractionComponent::FinishDismount()
{
    FVector Exit;
    if (!IsMounted()||!FindExit(Exit))
    {bDismountPreparing=false;DismountPrepareTime=0.f;LastStatus=TEXT("Exit became blocked: remain mounted");return false;}
    if (auto* Frame=Part<UStaticMeshComponent>(CurrentBike,"BMX_Frame")) Frame->OnComponentHit.RemoveDynamic(this,&UBMXInteractionComponent::OnBikeHit);
    auto* V=CurrentBike->FindComponentByClass<UBMXVehicleComponent>();const FVector Velocity=CurrentBike->GetActorForwardVector()*RideSpeed;
    RestoreCharacter(Exit);LastExitLocation=Exit;
    if (V) V->DropBike(Velocity);
    CurrentBike=nullptr;State=EBMXState::Dropped;RideSpeed=0.f;ThrottleInput=0.f;SteeringInput=0.f;bInjectedRideInput=false;
    BrakeInput=false;bInjectedBrakeInput=false;LastBrake=false;LeanDegrees=0.f;
    WheelieInput=false;bInjectedWheelieInput=false;LastWheelie=false;WheelieDegrees=0.f;WheelieRootOffset=FVector::ZeroVector;
    StoppieInput=false;bInjectedStoppieInput=false;LastStoppie=false;StoppieDegrees=0.f;bStoppieLatched=false;StoppieTime=0.f;
    ResetHop();LastStatus=TEXT("Dismounted safely; BMX dropped");return true;
}
void UBMXInteractionComponent::OnBikeHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,FVector NormalImpulse,const FHitResult& Hit)
{
    if (!IsMounted()||!OtherComp||OtherActor==Character.Get()||OtherActor==CurrentBike) return;
    if (HopState==EBMXHopState::Airborne&&Hit.ImpactNormal.Z>=FMath::Cos(FMath::DegreesToRadians(MaxRideSlopeDegrees))) return;
    const FVector BikeVelocity=HopState==EBMXHopState::Airborne?HopVelocity:CurrentBike->GetActorForwardVector()*RideSpeed;
    const bool bDynamic=OtherComp->IsSimulatingPhysics(Hit.BoneName);
    const float OtherMass=bDynamic?FMath::Max(OtherComp->GetMass(),1.f):0.f;
    // A kinematic bike may already have arrested the other body by the time its
    // hit event arrives. Reconstruct the incoming velocity from its lost impulse.
    FVector Normal=Hit.ImpactNormal.GetSafeNormal();
    if (FVector::DotProduct(Normal,CurrentBike->GetActorLocation()-OtherComp->GetComponentLocation())<0.f) Normal=-Normal;
    // Chaos reported the impulse with the opposite sign on this kinematic hit.
    // Use the verified contact direction from the other body toward the bike.
    const FVector IntoBike=Normal*FMath::Abs(FVector::DotProduct(NormalImpulse,Normal));
    const FVector OtherVelocity=bDynamic?OtherComp->GetPhysicsLinearVelocity(Hit.BoneName)+IntoBike/OtherMass:OtherActor?OtherActor->GetVelocity():FVector::ZeroVector;
    const float Closing=FMath::Max(0.f,-FVector::DotProduct(BikeVelocity-OtherVelocity,Normal));
    LastHitDiagnostic=FString::Printf(TEXT("Other=%s dynamic=%d closing=%.2f impulse=%s normal=%s"),*GetNameSafe(OtherActor),bDynamic,Closing,*NormalImpulse.ToCompactString(),*Normal.ToCompactString());
    if (Closing<CrashMinSpeed||Normal.IsNearlyZero()) return;
    const float ReleasedMass=13.f+(Character.IsValid()?FMath::Max(Character->GetMesh()->GetMass(),60.f):80.f);
    const float Transfer=bDynamic?OtherMass/(OtherMass+ReleasedMass):0.5f;
    LastCrashSpeed=Closing;
    if (TriggerCrash(Normal*Closing*Transfer,Hit.ImpactPoint)&&bDynamic&&IsValid(OtherComp))
    {
        // Refund the infinite-mass part of the initial kinematic contact. Only
        // the finite bike+rider share remains in the car's momentum loss.
        OtherComp->AddImpulseAtLocation(IntoBike*Transfer,Hit.ImpactPoint,Hit.BoneName);
    }
}
bool UBMXInteractionComponent::TriggerCrash(FVector ImpactVelocity,FVector HitLocation)
{
    if (!IsMounted()||!Character.IsValid()||ImpactVelocity.ContainsNaN()||HitLocation.ContainsNaN()) return false;
    ACharacter* C=Character.Get();USkeletalMeshComponent* Mesh=C->GetMesh();
    if (!Mesh->GetPhysicsAsset()||Mesh->GetPhysicsAsset()->SkeletalBodySetups.IsEmpty()||Mesh->GetBoneIndex("pelvis")==INDEX_NONE)
    {LastStatus=TEXT("Crash refused: rider physics asset missing");return false;}
    AActor* Bike=CurrentBike;auto* Frame=Part<UStaticMeshComponent>(Bike,"BMX_Frame");auto* V=Bike->FindComponentByClass<UBMXVehicleComponent>();
    if (!Frame||!V) return false;
    const FVector BaseVelocity=HopState==EBMXHopState::Airborne?HopVelocity:Bike->GetActorForwardVector()*RideSpeed;
    const FVector ReleasedVelocity=(BaseVelocity+ImpactVelocity).GetClampedToMaxSize(1200.f);
    Frame->OnComponentHit.RemoveDynamic(this,&UBMXInteractionComponent::OnBikeHit);
    UE_LOG(LogTemp,Display,TEXT("BMXHop: rider released at=%s velocity=%s hopState=%d"),*HitLocation.ToCompactString(),*BaseVelocity.ToCompactString(),static_cast<int32>(HopState));
    State=EBMXState::Ragdoll;CurrentBike=nullptr;RagdollTime=0.f;RagdollRestTime=0.f;++CrashCount;
    C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->RemoveTickPrerequisiteComponent(this);
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    C->GetCharacterMovement()->StopMovementImmediately();C->GetCharacterMovement()->DisableMovement();
    C->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
    // Released riders do not collide with the overlapping 13kg bike at the
    // first physics frame. Cars use Vehicle; world geometry still blocks.
    Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody,ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);
    Mesh->SetAllBodiesSimulatePhysics(true);Mesh->SetSimulatePhysics(true);Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
    Mesh->SetAllPhysicsLinearVelocity(ReleasedVelocity+FVector::UpVector*140.f);
    Mesh->WakeAllRigidBodies();V->DropBike(ReleasedVelocity);
    RideSpeed=0.f;ThrottleInput=0.f;SteeringInput=0.f;bInjectedRideInput=false;
    BrakeInput=false;bInjectedBrakeInput=false;WheelieInput=false;bInjectedWheelieInput=false;StoppieInput=false;bInjectedStoppieInput=false;
    WheelieDegrees=0.f;StoppieDegrees=0.f;LeanDegrees=0.f;WheelieRootOffset=FVector::ZeroVector;
    ResetHop();ApplyCamera(true);LastStatus=TEXT("Ragdoll: wait until settled, F to recover");return true;
}
bool UBMXInteractionComponent::BeginExternalRagdoll(FVector Velocity,UAnimSequence* ReleaseAnimation,float Position,const FTransform& InitialMeshWorld)
{
    if (!Character.IsValid()||State==EBMXState::Mounted||State==EBMXState::Ragdoll||Velocity.ContainsNaN()) return false;
    ACharacter* C=Character.Get();auto* Mesh=C->GetMesh();auto* Movement=C->GetCharacterMovement();auto* Capsule=C->GetCapsuleComponent();
    if (!Mesh->GetPhysicsAsset()||Mesh->GetPhysicsAsset()->SkeletalBodySetups.IsEmpty()) return false;
    SavedAnimClass=Mesh->GetAnimClass();SavedMeshRelative=Mesh->GetRelativeTransform();
    SavedCapsuleCollision=Capsule->GetCollisionEnabled();SavedMeshCollision=Mesh->GetCollisionEnabled();
    SavedMeshProfile=Mesh->GetCollisionProfileName();SavedMeshResponses=Mesh->GetCollisionResponseToChannels();SavedMeshObjectType=Mesh->GetCollisionObjectType();
    SavedMovementMode=Movement->MovementMode;SavedCustomMovementMode=Movement->CustomMovementMode;
    SavedControllerYaw=C->bUseControllerRotationYaw;SavedThirdPerson=BoolValue(C,"IsThirdPerson",true);SavedWeaponEquipped=BoolValue(C,"bWeaponEquipped");
    SavedUseCCD=Mesh->BodyInstance.bUseCCD;bExternalRagdoll=true;
    if (SavedWeaponEquipped) if (UFunction* F=C->FindFunction(TEXT("ToggleWeapon"))) C->ProcessEvent(F,nullptr);
    if (PlayerController.IsValid())
    {
        SavedControlRotation=PlayerController->GetControlRotation();PlayerController->SetIgnoreMoveInput(true);bMoveIgnoreAdded=true;
    }
    if (ReleaseAnimation)
    {
        Mesh->PlayAnimation(ReleaseAnimation,false);Mesh->SetPlayRate(0.f);Mesh->SetPosition(Position,false);
        Mesh->SetWorldTransform(InitialMeshWorld,false,nullptr,ETeleportType::TeleportPhysics);
        Mesh->TickAnimation(0.f,false);Mesh->RefreshBoneTransforms();
    }
    C->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Movement->StopMovementImmediately();Movement->DisableMovement();C->bUseControllerRotationYaw=false;
    Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCollisionProfileName(TEXT("Ragdoll"));Mesh->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Ignore);
    Mesh->SetAllUseCCD(true);Mesh->SetAllBodiesSimulatePhysics(true);Mesh->SetSimulatePhysics(true);
    Mesh->SetAllBodiesPhysicsBlendWeight(1.f);Mesh->SetAllPhysicsLinearVelocity(Velocity);Mesh->WakeAllRigidBodies();
    State=EBMXState::Ragdoll;CurrentBike=nullptr;RagdollTime=0.f;RagdollRestTime=0.f;
    if (BikeInput) BikeInput->bBlockInput=true;
    ApplyCamera(true);LastStatus=TEXT("Vehicle jump-out: ragdoll; F recover after settling");return true;
}
void UBMXInteractionComponent::RagdollTick(float DeltaTime)
{
    if (!Character.IsValid()) return;
    auto* Mesh=Character->GetMesh();const FVector Pelvis=Mesh->GetSocketLocation("pelvis");
    RagdollTime+=DeltaTime;RagdollSpeed=Mesh->GetPhysicsLinearVelocity("pelvis").Size();
    if (bExternalRagdoll&&RagdollTime>.8f) Mesh->SetCollisionResponseToChannel(ECC_Vehicle,ECR_Block);
    RagdollRestTime=RagdollSpeed<100.f?RagdollRestTime+DeltaTime:0.f;
    // The mesh is detached, so moving the noncolliding capsule follows the
    // body with the third-person camera without dragging the physical limbs.
    if (!Pelvis.ContainsNaN()) Character->SetActorLocation(Pelvis,false,nullptr,ETeleportType::TeleportPhysics);
}
bool UBMXInteractionComponent::TryRecover()
{
    if (State!=EBMXState::Ragdoll||!Character.IsValid()) return false;
    if (RagdollTime<1.5f||RagdollRestTime<0.5f) {LastStatus=TEXT("Recovery waits for body to settle");return false;}
    ACharacter* C=Character.Get();auto* Mesh=C->GetMesh();auto* Capsule=C->GetCapsuleComponent();
    const FVector Pelvis=Mesh->GetSocketLocation("pelvis");float Radius,HalfHeight;Capsule->GetScaledCapsuleSize(Radius,HalfHeight);
    const FCollisionShape Shape=FCollisionShape::MakeCapsule(Radius,HalfHeight);
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BMXRecovery),false,C);
    FVector Exit;bool bFound=false;
    for (const FVector Offset : {FVector::ZeroVector,FVector(110,0,0),FVector(-110,0,0),FVector(0,110,0),FVector(0,-110,0)})
    {
        const FVector XY=Pelvis+Offset;FHitResult Ground;
        if (!GetWorld()->LineTraceSingleByChannel(Ground,XY+FVector::UpVector*180.f,XY-FVector::UpVector*250.f,ECC_Visibility,Q)
            ||Ground.ImpactNormal.Z<C->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        const FVector Candidate=Ground.ImpactPoint+FVector::UpVector*(HalfHeight+2.f);
        if (GetWorld()->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,Capsule->GetCollisionObjectType(),Shape,Q,Responses)) continue;
        // A neighbouring recovery point must not cross a wall from the body.
        FHitResult Wall;const FVector From=Pelvis+FVector::UpVector*30.f;
        if (GetWorld()->LineTraceSingleByChannel(Wall,From,Candidate,ECC_Visibility,Q)) continue;
        Exit=Candidate;bFound=true;break;
    }
    if (!bFound) {LastStatus=TEXT("Recovery blocked: no safe standing capsule");return false;}
    Mesh->SetAllBodiesSimulatePhysics(false);Mesh->SetSimulatePhysics(false);Mesh->SetAllBodiesPhysicsBlendWeight(0.f);
    if (bExternalRagdoll) { Mesh->SetAllUseCCD(SavedUseCCD);bExternalRagdoll=false; }
    Mesh->AttachToComponent(Capsule,FAttachmentTransformRules::KeepWorldTransform);
    RestoreCharacter(Exit);LastExitLocation=Exit;State=EBMXState::Dropped;RagdollSpeed=0.f;
    LastStatus=TEXT("Recovered safely; walking control restored");return true;
}
void UBMXInteractionComponent::UpdatePedals(float DeltaTime)
{
    const float Cadence=FMath::Clamp(RideSpeed*360.f/165.f,-1080.f,1080.f);
    PedalAngle=FMath::Fmod(PedalAngle+Cadence*DeltaTime,360.f);
    PedalPhaseDegrees=FMath::Fmod(PedalAngle+360.f,360.f);
    if (auto* Crank=Part<USceneComponent>(CurrentBike,"BMX_Crank")) Crank->SetRelativeRotation(FRotator(0,0,PedalAngle));
    for (FName Name : {FName("BMX_Pedal_L"),FName("BMX_Pedal_R")})
        if (auto* Pedal=Part<USceneComponent>(CurrentBike,Name)) Pedal->SetRelativeRotation(FRotator(0,0,-PedalAngle));
}
void UBMXInteractionComponent::AirTick(float DeltaTime)
{
    if (!IsMounted()) return;
    HopAirTime+=DeltaTime;
    const FVector Previous=CurrentBike->GetActorLocation();
    const float Gravity=GetWorld()->GetGravityZ();
    FVector Position=Previous+HopVelocity*DeltaTime+FVector::UpVector*(0.5f*Gravity*DeltaTime*DeltaTime);
    HopVelocity.Z+=Gravity*DeltaTime;
    const FRotator Start=HopStartRotation.Rotator();
    const float Blend=FMath::Clamp(HopAirTime/0.3f,0.f,1.f);
    const float LiftPitch=HopPitch(HopAirTime);
    const FQuat Rotation=FRotator(FMath::Clamp(Start.Pitch,-25.f,25.f)*(1.f-Blend)+LiftPitch,Start.Yaw,FMath::Clamp(Start.Roll,-20.f,20.f)*(1.f-Blend)).Quaternion();
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BMXHopLanding),false,GetOwner());Q.AddIgnoredActor(CurrentBike);
    FHitResult Front,Rear;
    const float MinNormalZ=FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxRideSlopeDegrees,35.f,80.f)));
    const auto Support=[&](float X,FHitResult& H)
    {
        const FVector P=Position+Rotation.RotateVector(FVector(X,0,-27.03f));
        const FVector OldAxle=Previous+CurrentBike->GetActorQuat().RotateVector(FVector(X,0,-27.03f));
        const FVector TraceStart(P.X,P.Y,FMath::Max(P.Z,OldAxle.Z)+4.f);
        return GetWorld()->SweepSingleByChannel(H,TraceStart,P-FVector::UpVector*500.f,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(28.4f),Q)
            &&!H.bStartPenetrating&&H.ImpactNormal.Z>=MinNormalZ;
    };
    const bool HasFront=Support(47.36f,Front),HasRear=Support(-46.34f,Rear);
    bool bLanding=false;
    if (HopVelocity.Z<=0.f&&HasFront&&HasRear&&CoherentWheelSupport(Front,Rear))
    {
        const auto RootZ=[&](float X,const FHitResult& H)
        {return H.Location.Z-Rotation.RotateVector(FVector(X,0,-27.03f)).Z+WheelContactClearance/FMath::Max(H.ImpactNormal.Z,0.3f);};
        const float FloorZ=FMath::Max(RootZ(47.36f,Front),RootZ(-46.34f,Rear));
        if (Position.Z<=FloorZ+0.5f&&FloorZ<=Previous.Z+4.f)
        {
            Position.Z=FloorZ+0.25f;
            bLanding=BikePoseClear(CurrentBike,FTransform(Rotation,Position,CurrentBike->GetActorScale3D()));
        }
    }
    FHitResult Hit;CurrentBike->SetActorLocationAndRotation(Position,Rotation,true,&Hit,ETeleportType::None);
    if (!IsMounted()) return;
    LastHopHeight=FMath::Max(LastHopHeight,CurrentBike->GetActorLocation().Z-HopStart.Z);
    const float Travel=FVector::Dist2D(Previous,CurrentBike->GetActorLocation());
    WheelPhaseDegrees=FMath::Fmod(WheelPhaseDegrees-Travel*180.f/(PI*27.f),360.f);
    for (FName Name : {FName("BMX_Wheel_F"),FName("BMX_Wheel_R")})
        if (auto* Wheel=Part<USceneComponent>(CurrentBike,Name)) Wheel->SetRelativeRotation(FRotator(0,0,WheelPhaseDegrees));
    if (bLanding&&(!Hit.bBlockingHit||Hit.ImpactNormal.Z>=MinNormalZ))
    {
        HopState=EBMXHopState::Landing;++LandingCount;RiderLandingBlend=1.f;
        LastGroundNormal=(Front.ImpactNormal+Rear.ImpactNormal).GetSafeNormal();
        RideSpeed=FMath::Min(HopVelocity.Size2D(),FMath::Max(MaxRideSpeed,MaxSprintSpeed))*(RideSpeed<0.f?-1.f:1.f);
        HopVelocity=FVector::ZeroVector;HopCharge=0.f;
        LastStatus=TEXT("Hop landed: ground control restored");
        UE_LOG(LogTemp,Display,TEXT("BMXHop: landed height=%.1f time=%.2f front=%s rear=%s"),LastHopHeight,HopAirTime,*GetNameSafe(Front.GetActor()),*GetNameSafe(Rear.GetActor()));
    }
    else
    {
        if (Hit.bBlockingHit)
        {
            // Unsupported obstacle contact must not cancel gravity forever.
            if (Hit.ImpactNormal.Z>=MinNormalZ && HopVelocity.Z<=0.f)
                if (TriggerCrash(FVector::ZeroVector,Hit.ImpactPoint)) return;
            const float FallingSpeed=HopVelocity.Z;
            HopVelocity=FVector::VectorPlaneProject(HopVelocity,Hit.ImpactNormal);
            if (Hit.ImpactNormal.Z<MinNormalZ) HopVelocity.Z=FMath::Min(HopVelocity.Z,FallingSpeed);
        }
        LastStatus=Hit.bBlockingHit?TEXT("Airborne: contact blocked, landing support pending"):TEXT("Airborne: ballistic hop, feet held on pedals");
    }
}
void UBMXInteractionComponent::UpdateRiderMotion(float DeltaTime)
{
    RiderMountBlend=FMath::Max(0.f,RiderMountBlend-DeltaTime/0.45f);
    RiderLandingBlend=FMath::Max(0.f,RiderLandingBlend-DeltaTime*3.5f);
    const float Charge=HopState==EBMXHopState::Charging?HopCharge/FMath::Max(MaxHopCharge,0.1f):0.f;
    const float Sprint=HopState==EBMXHopState::Grounded&&LastSprint?FMath::Clamp((RideSpeed-250.f)/550.f,0.f,1.f):0.f;
    const float Air=HopState==EBMXHopState::Airborne?1.f:0.f;
    const float PedalWave=FMath::Sin(FMath::DegreesToRadians(PedalPhaseDegrees));
    const float Wheelie=FMath::Clamp(WheelieDegrees/28.f,0.f,1.f);
    const float Stoppie=FMath::Clamp(StoppieDegrees/20.f,0.f,1.f);
    const float Exit=bDismountPreparing?FMath::Clamp(DismountPrepareTime/0.35f,0.f,1.f):0.f;
    RiderStandBlend=FMath::FInterpTo(RiderStandBlend,Sprint,DeltaTime,7.f);
    const FRotator SpineTarget(PedalWave*3.f*RiderStandBlend,0,25.f+10.f*RiderStandBlend+8.f*Charge-4.f*Air+7.f*RiderLandingBlend-7.f*Wheelie+8.f*Stoppie-12.f*Exit-8.f*RiderMountBlend);
    const FRotator HeadTarget(0,0,-15.f-4.f*RiderStandBlend-3.f*Charge+2.f*Air+4.f*Wheelie-3.f*Stoppie+5.f*Exit);
    RiderSpineMotion=FMath::RInterpTo(RiderSpineMotion,SpineTarget,DeltaTime,12.f);
    RiderHeadMotion=FMath::RInterpTo(RiderHeadMotion,HeadTarget,DeltaTime,12.f);
    FVector Desired(4.f*RiderStandBlend,PedalWave*0.8f*RiderStandBlend,
        6.f*RiderStandBlend-5.f*Charge-4.f*RiderLandingBlend+3.f*Air+4.f*Exit+4.f*RiderMountBlend);
    RiderPoseOffset=FMath::VInterpTo(RiderPoseOffset,Desired,DeltaTime,12.f);
    // Set from the seat baseline every time, never accumulate offsets. Keeping
    // this here also removes the charging crouch on the first airborne frame.
    if (Character.IsValid()) Character->SetActorRelativeLocation(RiderSeatLocation
        +FVector(-5.f,0.f,2.f)*Wheelie+FVector(6.f,0.f,2.f)*Stoppie+RiderPoseOffset);
}
void UBMXInteractionComponent::RideTick(float DeltaTime)
{
    if (bDismountPreparing)
    {
        RideSpeed=0.f;LastSprint=false;DismountPrepareTime+=DeltaTime;
        UpdateRiderMotion(DeltaTime);
        if (DismountPrepareTime>=0.35f) FinishDismount();
        return;
    }
    if (RiderMountBlend>0.f) {UpdateRiderMotion(DeltaTime);return;}
    const bool Jump=bInjectedJumpInput?JumpInput:(bNativeJumpDown||(PlayerController.IsValid()&&PlayerController->IsInputKeyDown(EKeys::SpaceBar)));
    const bool Pressed=bJumpPressPending||(!bJumpWasDown&&Jump);bJumpPressPending=false;
    if (HopState==EBMXHopState::Airborne) {bJumpWasDown=Jump;AirTick(DeltaTime);if(IsMounted())UpdateRiderMotion(DeltaTime);return;}
    if (HopState==EBMXHopState::Landing) HopState=EBMXHopState::Grounded;
    if (Pressed&&WheelieDegrees<1.f&&StoppieDegrees<1.f)
    {HopState=EBMXHopState::Charging;HopCharge=0.f;}
    if (HopState==EBMXHopState::Charging)
    {
        if (Jump) HopCharge=FMath::Min(HopCharge+DeltaTime,MaxHopCharge);
        else
        {
            HopVelocity=CurrentBike->GetActorForwardVector()*RideSpeed;
            HopVelocity.Z=FMath::Max(0.f,HopVelocity.Z)+FMath::Lerp(MinHopVelocity,MaxHopVelocity,FMath::Clamp(HopCharge/FMath::Max(MaxHopCharge,0.1f),0.f,1.f));
            HopState=EBMXHopState::Airborne;HopAirTime=0.f;HopStart=CurrentBike->GetActorLocation();HopStartRotation=CurrentBike->GetActorQuat();LastHopHeight=0.f;++HopCount;
            UE_LOG(LogTemp,Display,TEXT("BMXHop: takeoff charge=%.2f velocity=%s"),HopCharge,*HopVelocity.ToCompactString());
            WheelieDegrees=0.f;StoppieDegrees=0.f;WheelieRootOffset=FVector::ZeroVector;LastWheelie=false;LastStoppie=false;
            if (Character.IsValid()) Character->SetActorRelativeLocation(RiderSeatLocation);
            bJumpWasDown=Jump;AirTick(DeltaTime);if(IsMounted())UpdateRiderMotion(DeltaTime);return;
        }
    }
    bJumpWasDown=Jump;
    float Throttle=ThrottleInput,Steering=SteeringInput;
    if (!bInjectedRideInput && PlayerController.IsValid())
    {
        Throttle=(PlayerController->IsInputKeyDown(EKeys::W)?1.f:0.f)-(PlayerController->IsInputKeyDown(EKeys::S)?1.f:0.f);
        Steering=(PlayerController->IsInputKeyDown(EKeys::D)?1.f:0.f)-(PlayerController->IsInputKeyDown(EKeys::A)?1.f:0.f);
    }
    const bool Brake=bInjectedBrakeInput?BrakeInput:(PlayerController.IsValid()&&PlayerController->IsInputKeyDown(EKeys::LeftAlt));
    LastThrottle=Throttle;LastSteering=Steering;LastBrake=Brake;
    const bool WheelieRequested=bInjectedWheelieInput?WheelieInput:(PlayerController.IsValid()&&PlayerController->IsInputKeyDown(EKeys::LeftControl));
    LastWheelie=HopState!=EBMXHopState::Charging&&WheelieRequested&&!Brake&&Throttle>0.f&&RideSpeed>=WheelieMinSpeed;
    const bool StoppieRequested=bInjectedStoppieInput?StoppieInput:(PlayerController.IsValid()&&PlayerController->IsInputKeyDown(EKeys::LeftShift));
    if (!StoppieRequested||!Brake) {bStoppieLatched=false;StoppieTime=0.f;}
    else if (!bStoppieLatched&&RideSpeed>=StoppieMinSpeed&&WheelieDegrees<1.f)
    {bStoppieLatched=true;StoppieTime=0.65f;}
    LastStoppie=HopState!=EBMXHopState::Charging&&StoppieRequested&&Brake&&StoppieTime>0.f;
    StoppieTime=FMath::Max(0.f,StoppieTime-DeltaTime);
    const bool SprintRequested=bInjectedSprintInput?SprintInput:(PlayerController.IsValid()&&PlayerController->IsInputKeyDown(EKeys::LeftShift));
    LastSprint=SprintRequested&&Throttle>0.f&&!Brake&&!LastStoppie;
    const float ForwardLimit=LastSprint?FMath::Max(MaxRideSpeed,MaxSprintSpeed):MaxRideSpeed;
    const float TargetSpeed=Brake?0.f:Throttle*(Throttle<0.f?MaxReverseSpeed:ForwardLimit);
    const bool Slowing=FMath::Abs(RideSpeed)>FMath::Abs(TargetSpeed)&&RideSpeed*TargetSpeed>=0.f;
    const float Acceleration=Brake?BrakeDeceleration:(Slowing||Throttle==0.f?180.f:(LastSprint?SprintAcceleration:250.f));
    RideSpeed=FMath::FInterpConstantTo(RideSpeed,TargetSpeed,DeltaTime,Acceleration);
    const float LeanTarget=WheelieDegrees+StoppieDegrees>1.f?0.f:-Steering*MaxLeanDegrees*FMath::Clamp(RideSpeed/FMath::Max(MaxRideSpeed,1.f),-1.f,1.f);
    LeanDegrees=FMath::FInterpTo(LeanDegrees,LeanTarget,DeltaTime,5.f);
    const float TurnScale=WheelieDegrees+StoppieDegrees>1.f?0.4f:1.f;
    const FQuat Turn=FQuat(LastGroundNormal,FMath::DegreesToRadians(Steering*55.f*TurnScale*DeltaTime*FMath::Clamp(RideSpeed/200.f,-1.f,1.f)));
    // Remove the previous pivot displacement before integrating travel. Never add it cumulatively.
    FQuat Rotation=GroundRotation((Turn*CurrentBike->GetActorQuat()).GetForwardVector(),LastGroundNormal);
    const FVector PreviousPosition=CurrentBike->GetActorLocation()-WheelieRootOffset;
    FVector Position=PreviousPosition+Rotation.GetForwardVector()*RideSpeed*DeltaTime;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BMXRideGround),false,GetOwner());Q.AddIgnoredActor(CurrentBike);
    FHitResult Front,Rear,Ground;
    const float MinRideNormalZ=FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxRideSlopeDegrees,35.f,80.f)));
    const auto Probe=[&](float X,FHitResult& Hit)
    {
        // A rounded tyre crosses coping before touching its steep inner face.
        // A point ahead of the axle instead samples that face prematurely and
        // raises the entire frame until the rear probe loses the floor.
        const FVector P=Position+Rotation.RotateVector(FVector(X,0,-27.03f));
        return GetWorld()->SweepSingleByChannel(Hit,P+FVector::UpVector*90.f,P-FVector::UpVector*240.f,
            FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(28.4f),Q)
            && !Hit.bStartPenetrating && Hit.ImpactNormal.Z>=MinRideNormalZ;
    };
    const auto SampleSupport=[&]()
    {
        if (!Probe(47.36f,Front)||!Probe(-46.34f,Rear)||!CoherentWheelSupport(Front,Rear)) return false;
        const FVector AverageNormal=(Front.ImpactNormal+Rear.ImpactNormal).GetSafeNormal();
        const FVector Right=FVector::CrossProduct(AverageNormal,Rotation.GetForwardVector()).GetSafeNormal();
        // Sweep Location is the tyre centre at contact, unlike ImpactPoint.
        Ground.ImpactNormal=FVector::CrossProduct((Front.Location-Rear.Location).GetSafeNormal(),Right).GetSafeNormal();
        Ground.ImpactPoint=(Front.Location+Rear.Location)*0.5f;
        return Ground.ImpactNormal.Z>=MinRideNormalZ;
    };
    bool bSupportStopped=false;
    if (!SampleSupport())
    {
        // Refuse the unsupported next step, then lower the stunt at the still-supported current base.
        Position=PreviousPosition;
        RideSpeed=0.f;LastWheelie=false;LastStoppie=false;StoppieTime=0.f;
        if (!SampleSupport())
        {
            // Lost support starts a fall rather than freezing the mounted pose.
            HopState=EBMXHopState::Airborne;HopAirTime=0.f;
            HopVelocity=FVector::ZeroVector;HopStart=CurrentBike->GetActorLocation();
            HopStartRotation=CurrentBike->GetActorQuat();
            WheelieDegrees=0.f;StoppieDegrees=0.f;WheelieRootOffset=FVector::ZeroVector;
            AirTick(DeltaTime);if(IsMounted())UpdateRiderMotion(DeltaTime);return;
        }
        bSupportStopped=true;
    }
    const FVector Heading=Rotation.GetForwardVector().GetSafeNormal2D();
    Rotation=GroundRotation(Heading,Ground.ImpactNormal)*FQuat(FVector::ForwardVector,FMath::DegreesToRadians(LeanDegrees));LastGroundNormal=Ground.ImpactNormal;
    // Preserve both rounded wheel supports across changes in surface normal.
    const auto AxleHeight=[&](float X,const FHitResult& Support)
    {
        const FVector Offset=Rotation.RotateVector(FVector(X,0,-27.03f));
        return Support.Location.Z-Offset.Z+WheelContactClearance/FMath::Max(Support.ImpactNormal.Z,0.3f);
    };
    Position.Z=FMath::Max(AxleHeight(47.36f,Front),AxleHeight(-46.34f,Rear));
    // Tilting changes the axle XY positions. Resolve their new contacts before
    // moving the compound collider, especially on the short coping transition.
    for (int32 Iteration=0;Iteration<3;++Iteration)
    {
        if (!SampleSupport()) break;
        Rotation=GroundRotation(Heading,Ground.ImpactNormal)*FQuat(FVector::ForwardVector,FMath::DegreesToRadians(LeanDegrees));
        LastGroundNormal=Ground.ImpactNormal;
        Position.Z=FMath::Max(AxleHeight(47.36f,Front),AxleHeight(-46.34f,Rear));
    }
    // The last rotation changes axle XY again. Re-sample at that FINAL rotation
    // and solve height without rotating again: stale contacts can bury a tyre
    // in the steep coping face. 28.4cm encloses the tyre collider box corners.
    if (SampleSupport())
        Position.Z=FMath::Max(AxleHeight(47.36f,Front),AxleHeight(-46.34f,Rear));
    const FVector GroundPosition=Position;
    const FQuat GroundQuat=Rotation;
    const float PreviousAngle=WheelieDegrees-StoppieDegrees;
    const float TargetAngle=LastStoppie?-MaxStoppieDegrees:(LastWheelie?MaxWheelieDegrees:0.f);
    const float RequestedAngle=FMath::FInterpConstantTo(PreviousAngle,TargetAngle,DeltaTime,LastWheelie?45.f:60.f);
    const auto TiltPose=[&](float Angle)
    {
        const FQuat Tilt=GroundQuat*FRotator(Angle,0.f,0.f).Quaternion();
        const FVector Axle(Angle<0.f?47.36f:-46.34f,0.f,-27.03f);
        const FVector PivotOffset=GroundQuat.RotateVector(Axle)-Tilt.RotateVector(Axle);
        return FTransform(Tilt,GroundPosition+PivotOffset,CurrentBike->GetActorScale3D());
    };
    FTransform Pose=TiltPose(RequestedAngle);
    float AppliedAngle=RequestedAngle;
    if ((FMath::Abs(RequestedAngle)>0.01f||FMath::Abs(PreviousAngle)>0.01f)&&!BikePoseClear(CurrentBike,Pose))
    {
        AppliedAngle=PreviousAngle;Pose=TiltPose(AppliedAngle);
        if (!BikePoseClear(CurrentBike,Pose)) {RideSpeed=0.f;LastStatus=TEXT("Stopped: wheelie pose obstructed");return;}
    }
    Rotation=Pose.GetRotation();Position=Pose.GetLocation();
    FHitResult Hit;CurrentBike->SetActorLocationAndRotation(Position,Rotation,true,&Hit,ETeleportType::None);
    // OnComponentHit may release the bike during the swept move.
    if (!IsMounted()) return;
    // Unreal applies the new rotation before sweeping from the old location.
    // Across coping this can put a tyre fractionally inside its own support
    // surface and refuse even an upward move. Resolve only shallow FLOOR
    // penetration, and only after checking the complete raised body is clear.
    for (int32 Retry=0;Retry<3&&Hit.bBlockingHit&&Hit.PenetrationDepth<=2.f&&Hit.ImpactNormal.Z>=MinRideNormalZ
        && (Hit.GetActor()==Front.GetActor()||Hit.GetActor()==Rear.GetActor());++Retry)
    {
        float Lift=FMath::Max(0.75f,Hit.PenetrationDepth/FMath::Max(Hit.ImpactNormal.Z,0.3f)+0.35f);
        FVector Raised;
        bool bClear=false;
        for (;Lift<=6.f;Lift+=0.75f)
        {
            Raised=CurrentBike->GetActorLocation()+FVector::UpVector*Lift;
            bClear=BikePoseClear(CurrentBike,FTransform(Rotation,Raised,CurrentBike->GetActorScale3D()));
            if (bClear) break;
        }
        if (bClear)
        {
            CurrentBike->SetActorLocation(Raised,false,nullptr,ETeleportType::TeleportPhysics);
            Position.Z=FMath::Max(Position.Z,Raised.Z);
            CurrentBike->SetActorLocationAndRotation(Position,Rotation,true,&Hit,ETeleportType::None);
            if (!IsMounted()) return;
        }
        else break;
    }
    if (Hit.bBlockingHit)
    {
        RideSpeed=0; ++RideBlockedFrames;
        LastRideBlock=FString::Printf(TEXT("%s normal=%s penetrating=%d depth=%.3f"),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToCompactString(),Hit.bStartPenetrating,Hit.PenetrationDepth);
        LastStatus=FString::Printf(TEXT("Ride blocked by %s; normal %s; penetration %.3f"),*GetNameSafe(Hit.GetComponent()),*Hit.ImpactNormal.ToString(),Hit.PenetrationDepth);
    }
    else LastStatus=HopState==EBMXHopState::Charging?TEXT("Charging hop: release Space"):TEXT("Mounted: WASD, Shift sprint, Space hop, Alt brake, Ctrl wheelie, Shift+Alt stoppie, F exit");
    if (bSupportStopped&&!Hit.bBlockingHit) LastStatus=TEXT("Edge guarded: stopped on supported ground; lowering stunt");
    WheelieDegrees=FMath::Max(0.f,AppliedAngle);StoppieDegrees=FMath::Max(0.f,-AppliedAngle);WheelieRootOffset=Position-GroundPosition;
    const FVector Forward=GroundQuat.GetForwardVector();
    const float Travel=FVector::DotProduct((CurrentBike->GetActorLocation()-WheelieRootOffset)-PreviousPosition,Forward.GetSafeNormal2D())/FMath::Max(Forward.Size2D(),0.1f);
    WheelPhaseDegrees=FMath::Fmod(WheelPhaseDegrees-Travel*180.f/(PI*27.f),360.f);
    for (FName Name : {FName("BMX_Wheel_F"),FName("BMX_Wheel_R")})
        if (auto* Wheel=Part<USceneComponent>(CurrentBike,Name)) Wheel->SetRelativeRotation(FRotator(0,0,WheelPhaseDegrees));
    // Steering is visual while mounted; the dropped compound collider is neutralized on release.
    if (auto* Pivot=Part<USceneComponent>(CurrentBike,"BMX_SteeringPivot")) Pivot->SetRelativeRotation(FRotator(0,Steering*12.f,0));
    if (HopState!=EBMXHopState::Charging) UpdatePedals(DeltaTime);
    UpdateRiderMotion(DeltaTime);
    if (PlayerController.IsValid()&&FMath::Abs(RideSpeed)>30.f&&GetWorld()->GetTimeSeconds()-LastManualLookTime>CameraFollowDelay)
    {
        FRotator View=PlayerController->GetControlRotation();
        const FRotator Target(View.Pitch,CurrentBike->GetActorRotation().Yaw,0.f);
        PlayerController->SetControlRotation(FMath::RInterpTo(View,Target,DeltaTime,CameraFollowSpeed));
    }
}
void UBMXInteractionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);InstallInput();
    if (State==EBMXState::Raising)
    {
        if (!IsValid(CurrentBike)) {CancelRaise(TEXT("Bike missing"));return;}
        RaiseTime+=DeltaTime;const float Alpha=FMath::Clamp(RaiseTime/RaiseDuration,0.f,1.f);const float Smooth=Alpha*Alpha*(3.f-2.f*Alpha);
        FTransform Pose(FQuat::Slerp(RaiseFrom.GetRotation(),RaiseTo.GetRotation(),Smooth),FMath::Lerp(RaiseFrom.GetLocation(),RaiseTo.GetLocation(),Smooth)+LastGroundNormal*(24.f*FMath::Sin(PI*Smooth)),RaiseTo.GetScale3D());
        if (!BikePoseClear(CurrentBike,Pose)) {CancelRaise(TEXT("Lifting interrupted by obstruction"));return;}
        CurrentBike->SetActorTransform(Pose,false,nullptr,ETeleportType::TeleportPhysics);
        if (Alpha>=1.f) CompleteMount();
    }
    else if (IsMounted())
    {
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(DeltaTime*120.f),1,32);
        for (int32 Step=0;Step<Steps&&IsMounted();++Step) RideTick(DeltaTime/Steps);
        if (IsMounted()&&Character.IsValid()) UpdateRiderIK(Character->GetMesh()->GetAnimInstance());
    }
    else if (State==EBMXState::Ragdoll) RagdollTick(DeltaTime);
}
void UBMXInteractionComponent::UpdateRiderIK(UAnimInstance* Anim)
{
    if (!IsMounted() || !Anim) return;
    PoseValue(Anim,"RiderSpineMotion",RiderSpineMotion);
    PoseValue(Anim,"RiderHeadMotion",RiderHeadMotion);
    auto* Bars=Part<USceneComponent>(CurrentBike,"BMX_Steering");auto* L=Part<USceneComponent>(CurrentBike,"BMX_Pedal_L");auto* R=Part<USceneComponent>(CurrentBike,"BMX_Pedal_R");
    if (!Bars || !L || !R) return;
    FTransform HandL=Bars->GetSocketTransform("Socket_Hand_L"),HandR=Bars->GetSocketTransform("Socket_Hand_R");
    const FTransform GoalL(FRotator(0,90,-90),FVector(0,-7,3));const FTransform GoalR(FRotator(0,90,90),FVector(0,7,3));
    HandL=GoalL*HandL;HandR=GoalR*HandR;
    PoseValue(Anim,"HandLWorld",HandL.GetLocation());PoseValue(Anim,"HandRWorld",HandR.GetLocation());
    PoseValue(Anim,"HandLWorldRot",HandL.Rotator());PoseValue(Anim,"HandRWorldRot",HandR.Rotator());
    const FTransform FootGoal(FRotator(0,90,0),FVector(-8,0,9.1));
    PoseValue(Anim,"FootLWorld",(FootGoal*L->GetSocketTransform("Socket_Pedal_L")).GetLocation());
    PoseValue(Anim,"FootRWorld",(FootGoal*R->GetSocketTransform("Socket_Pedal_R")).GetLocation());
}
void UBMXRiderLibrary::UpdatePlayerRiderIK(UAnimInstance* Anim)
{
    if (Anim && Anim->GetOwningActor()) if (auto* C=Anim->GetOwningActor()->FindComponentByClass<UBMXInteractionComponent>()) C->UpdateRiderIK(Anim);
}
void UBMXInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PlayerController.IsValid() && BikeInput) PlayerController->PopInputComponent(BikeInput);
    if (bMoveIgnoreAdded && PlayerController.IsValid()) PlayerController->SetIgnoreMoveInput(false);
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBMXSupportRegression,"SA.BMX.Hop.SupportAndPitch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBMXSupportRegression::RunTest(const FString& Parameters)
{
    FHitResult Front,Rear;
    Front.ImpactNormal=Rear.ImpactNormal=FVector::UpVector;
    Front.Location=FVector(47.36f,0,28.4f);Rear.Location=FVector(-46.34f,0,28.4f);
    TestTrue(TEXT("Flat ground supported"),CoherentWheelSupport(Front,Rear));
    Front.Location.Z+=45.f;
    TestFalse(TEXT("Bench seat and ground are not a ramp"),CoherentWheelSupport(Front,Rear));
    Front.Location.Z+=45.f;
    TestFalse(TEXT("Bench back and ground are not a ramp"),CoherentWheelSupport(Front,Rear));
    Front.Location=Rear.Location+FVector(1,0,1).GetSafeNormal()*93.7f;
    Front.ImpactNormal=Rear.ImpactNormal=FVector(-1,0,1).GetSafeNormal();
    TestTrue(TEXT("Continuous 45 degree ramp supported"),CoherentWheelSupport(Front,Rear));
    TestEqual(TEXT("Takeoff level"),HopPitch(0.f),0.f);
    TestEqual(TEXT("Front lift limited"),HopPitch(0.16f),18.f);
    TestEqual(TEXT("Bike levels after rear lift"),HopPitch(0.4f),0.f);
    TestEqual(TEXT("Landing level"),HopPitch(1.f),0.f);
    return true;
}
#endif
