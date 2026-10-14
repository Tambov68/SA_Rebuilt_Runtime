#include "SARideCameraComponent.h"
#include "BMXInteractionComponent.h"
#include "SAVehicleInteractionComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

USARideCameraComponent::USARideCameraComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
void USARideCameraComponent::BeginPlay()
{
    Super::BeginPlay();Character=Cast<ACharacter>(GetOwner());
    if(Character.IsValid()) AddTickPrerequisiteComponent(Character->GetMesh());
    InstallInput();
}
void USARideCameraComponent::InstallInput()
{
    if(RideInput||!Character.IsValid())return;
    auto* PC=Cast<APlayerController>(Character->GetController());
    if(!PC||!PC->IsLocalController())return;
    Controller=PC;
    RideInput=NewObject<UInputComponent>(GetOwner(),TEXT("RideViewInput"));
    RideInput->RegisterComponent();RideInput->Priority=60;
    RideInput->BindKey(EKeys::V,IE_Pressed,this,&USARideCameraComponent::ToggleView).bConsumeInput=false;
    RideInput->BindAxisKey(EKeys::MouseX,this,&USARideCameraComponent::LookYaw).bConsumeInput=false;
    RideInput->BindAxisKey(EKeys::MouseY,this,&USARideCameraComponent::LookPitch).bConsumeInput=false;
    PC->PushInputComponent(RideInput);
}
AActor* USARideCameraComponent::RideTarget() const
{
    if(!Character.IsValid()||!Controller.IsValid())return nullptr;
    auto* Car=Character->FindComponentByClass<USAVehicleInteractionComponent>();
    if(Car&&Car->CurrentVehicle)
    {
        if(Car->State==ESAVehicleInteractionState::Driving&&Car->CurrentVehicle==Controller->GetPawn())return Car->CurrentVehicle;
        if(bPreferFirstPerson&&(Car->State==ESAVehicleInteractionState::Entering||Car->State==ESAVehicleInteractionState::Exiting))return Car->CurrentVehicle;
    }
    auto* Bike=Character->FindComponentByClass<UBMXInteractionComponent>();
    if(Bike&&Bike->IsMounted()&&Controller->GetPawn()==Character.Get())return Bike->CurrentBike;
    return nullptr;
}
void USARideCameraComponent::ToggleView()
{
    if(Character.IsValid())if(auto* Car=Character->FindComponentByClass<USAVehicleInteractionComponent>())
        if(Car->State==ESAVehicleInteractionState::Entering||Car->State==ESAVehicleInteractionState::Exiting)return;
    if(bFirstPerson){bPreferFirstPerson=false;LeaveView(true);return;}
    AActor* Ride=RideTarget();if(!Ride)return;
    bPreferFirstPerson=true;EnterView(Ride);
}
void USARideCameraComponent::EnterView(AActor* Ride)
{
    auto* Mesh=Character->GetMesh();
    if(!Mesh||Mesh->GetBoneIndex(TEXT("head"))==INDEX_NONE)return;
    FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
    if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity,P);
    if(!Camera)return;
    Target=Ride;PreviousView=Controller->GetPawn();
    bBike=Ride->FindComponentByClass<UBMXVehicleComponent>()!=nullptr;
    // Store gaze in vehicle space so turning the car outside the cockpit does
    // not turn the driver's restored gaze toward an unrelated world heading.
    LookX=bHasSavedLook&&SavedLookTarget.Get()==Ride?SavedLookX:0.f;
    LookY=bHasSavedLook&&SavedLookTarget.Get()==Ride?SavedLookY:0.f;
    auto* Car=Character->FindComponentByClass<USAVehicleInteractionComponent>();
    bWasTransition=!bBike&&Car&&Car->State!=ESAVehicleInteractionState::Driving;
    const FVector Eye=(!bBike&&!bWasTransition)?
        Mesh->GetSocketLocation(TEXT("pelvis"))+Ride->GetActorQuat().RotateVector(CarSeatedEyeOffset):
        Mesh->GetSocketLocation(TEXT("head"))+Ride->GetActorQuat().RotateVector(EyeOffset);
    // Keep the car viewpoint rigidly in the cockpit. For BMX follow the neck,
    // which remains animated when the head is hidden from the local view.
    Anchor=bBike?Mesh->GetSocketTransform(TEXT("neck_01")).InverseTransformPosition(Eye):Ride->GetActorTransform().InverseTransformPosition(Eye);
    NeckEyeOffset=Mesh->GetSocketTransform(TEXT("neck_01")).InverseTransformPosition(Eye);
    SmoothedEye=Ride->GetActorTransform().InverseTransformPosition(Eye);
    SmoothedBody=FRotator::ZeroRotator;
    SavedAnimTickOption=Mesh->VisibilityBasedAnimTickOption;
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    bHeadWasHidden=Mesh->IsBoneHiddenByName(TEXT("head"));
    Mesh->HideBoneByName(TEXT("head"),EPhysBodyOp::PBO_None);
    Camera->GetCameraComponent()->SetFieldOfView(bBike?BikeFOV:CarFOV);
    LastViewRotation=FRotator(LookY+Ride->GetActorRotation().Pitch,LookX+Ride->GetActorRotation().Yaw,0.f);
    Camera->SetActorLocationAndRotation(Eye,LastViewRotation);
    bFirstPerson=true;Controller->SetViewTargetWithBlend(Camera,.2f);
}
void USARideCameraComponent::LeaveView(bool bRestoreTarget)
{
    if(!bFirstPerson)return;
    if(Camera){LastViewRotation=Camera->GetActorRotation();bHasSavedLook=true;SavedLookX=LookX;SavedLookY=LookY;SavedLookTarget=Target;}
    if(Character.IsValid())
    {
        Character->GetMesh()->VisibilityBasedAnimTickOption=SavedAnimTickOption;
        if(!bHeadWasHidden)Character->GetMesh()->UnHideBoneByName(TEXT("head"));
    }
    if(bRestoreTarget&&Controller.IsValid())
    {
        AActor* Restore=PreviousView.Get();
        if(!bBike&&Controller->GetPawn())Restore=Controller->GetPawn();
        Controller->SetViewTargetWithBlend(IsValid(Restore)?Restore:Controller->GetPawn(),.2f);
    }
    bFirstPerson=false;Target.Reset();PreviousView.Reset();bWasTransition=false;
    SmoothedEye=FVector::ZeroVector;NeckEyeOffset=FVector::ZeroVector;Anchor=FVector::ZeroVector;
}
void USARideCameraComponent::LookYaw(float V){ApplyLookInput(V,0.f);}
void USARideCameraComponent::LookPitch(float V){ApplyLookInput(0.f,V);}
void USARideCameraComponent::ApplyLookInput(float YawInput,float PitchInput)
{
    if(!FMath::IsFinite(YawInput)||!FMath::IsFinite(PitchInput))return;
    auto* Car=Character.IsValid()?Character->FindComponentByClass<USAVehicleInteractionComponent>():nullptr;
    if(Car&&Car->CurrentVehicle&&Car->State!=ESAVehicleInteractionState::Driving)return;
    if(bFirstPerson)
    {
        LookX=FMath::Clamp(LookX+YawInput*LookSensitivity,-110.f,110.f);
        LookY=FMath::Clamp(LookY-PitchInput*LookSensitivity,-65.f,45.f);
    }
    else if(Car&&RideTarget())
    {
        OrbitYaw=FMath::UnwindDegrees(OrbitYaw+YawInput*LookSensitivity);
        OrbitPitch=FMath::Clamp(OrbitPitch-PitchInput*LookSensitivity,-35.f,45.f);
    }
}
void USARideCameraComponent::RestoreOrbit()
{
    if(OrbitArm.IsValid())OrbitArm->SetRelativeRotation(SavedOrbitRotation);
    OrbitArm.Reset();OrbitYaw=0.f;OrbitPitch=0.f;
}
void USARideCameraComponent::UpdateOrbit(AActor* Ride)
{
    if(!Ride){RestoreOrbit();return;}
    if(OrbitArm.IsValid()&&OrbitArm->GetOwner()!=Ride)RestoreOrbit();
    if(!OrbitArm.IsValid())
    {
        TArray<USpringArmComponent*> Arms;Ride->GetComponents(Arms);
        for(auto* Arm:Arms)if(Arm->GetName()==TEXT("VehicleCameraBoom"))
        {OrbitArm=Arm;SavedOrbitRotation=Arm->GetRelativeRotation();break;}
    }
    if(OrbitArm.IsValid())OrbitArm->SetRelativeRotation(FRotator(
        FMath::Clamp(SavedOrbitRotation.Pitch+OrbitPitch,-75.f,15.f),
        SavedOrbitRotation.Yaw+OrbitYaw,SavedOrbitRotation.Roll));
}
void USARideCameraComponent::TickComponent(float DT,ELevelTick Type,FActorComponentTickFunction* F)
{
    Super::TickComponent(DT,Type,F);InstallInput();
    if(Character.IsValid())if(auto* Car=Character->FindComponentByClass<USAVehicleInteractionComponent>())Car->UpdateAnimationCamera();
    if(!bFirstPerson&&bPreferFirstPerson)if(AActor* Ride=RideTarget())EnterView(Ride);
    const bool bEligible=RideTarget()!=nullptr;
    auto* DrivingCar=Character.IsValid()?Character->FindComponentByClass<USAVehicleInteractionComponent>():nullptr;
    const bool bCarLook=DrivingCar&&DrivingCar->State==ESAVehicleInteractionState::Driving&&bEligible;
    if(bCarLook)UpdateOrbit(RideTarget());else RestoreOrbit();
    if(RideInput)
    {
        RideInput->KeyBindings[0].bConsumeInput=bEligible;
        for(auto& Binding:RideInput->AxisKeyBindings)Binding.bConsumeInput=bFirstPerson||bCarLook;
    }
    if(!bFirstPerson)return;
    if(!Target.IsValid()||RideTarget()!=Target.Get())
    {
        // A normal bicycle dismount does not assign a new view target.
        // Restore it only while this camera still owns the view; preserve any
        // transition camera another system has already selected.
        const bool Restore=Controller.IsValid()&&Controller->GetViewTarget()==Camera;
        if(Restore)Controller->SetControlRotation(LastViewRotation);
        LeaveView(Restore);return;
    }
    if(!bHeadWasHidden)
    {
        Character->GetMesh()->HideBoneByName(TEXT("head"),EPhysBodyOp::PBO_None);
    }
    const FTransform RideTransform=Target->GetActorTransform();
    const FVector AnimatedEye=Character->GetMesh()->GetSocketTransform(TEXT("neck_01")).TransformPosition(NeckEyeOffset);
    FVector LocalEye=RideTransform.InverseTransformPosition(AnimatedEye);
    auto* Car=Character->FindComponentByClass<USAVehicleInteractionComponent>();
    const bool bTransition=!bBike&&Car&&Car->State!=ESAVehicleInteractionState::Driving;
    if(bWasTransition&&!bTransition&&!bBike)
        Anchor=RideTransform.InverseTransformPosition(Character->GetMesh()->GetSocketLocation(TEXT("pelvis")))+CarSeatedEyeOffset;
    bWasTransition=bTransition;
    // Smooth only animation displacement in ride space, never vehicle travel.
    if(!bBike&&!bTransition)LocalEye=Anchor;
    SmoothedEye=FMath::VInterpTo(SmoothedEye,LocalEye,DT,bTransition?22.f:16.f);
    FRotator Body=Target->GetActorRotation();
    SmoothedBody=FMath::RInterpTo(SmoothedBody,FRotator(Body.Pitch,0.f,Body.Roll*.25f),DT,12.f);
    FRotator View(SmoothedBody.Pitch+LookY,Body.Yaw+LookX,SmoothedBody.Roll);
    if(bTransition)
    {
        const float Alpha=Car->State==ESAVehicleInteractionState::Entering?
            FMath::Clamp((Car->TransitionTime-.8f)/1.3f,0.f,1.f):
            1.f-FMath::Clamp((Car->TransitionTime-.35f)/1.7f,0.f,1.f);
        View.Yaw+=-70.f*(1.f-FMath::SmoothStep(0.f,1.f,Alpha));
    }
    Camera->SetActorLocationAndRotation(RideTransform.TransformPosition(SmoothedEye),View);
    LastViewRotation=View;
}
void USARideCameraComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    LeaveView(false);
    RestoreOrbit();
    if(Controller.IsValid()&&RideInput)Controller->PopInputComponent(RideInput);
    if(Camera)Camera->Destroy();
    Super::EndPlay(Reason);
}
