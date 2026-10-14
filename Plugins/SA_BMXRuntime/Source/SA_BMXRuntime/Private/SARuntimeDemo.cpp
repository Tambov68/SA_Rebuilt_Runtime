#include "SARuntimeDemo.h"
#include "BMXInteractionComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

ASADemoBike::ASADemoBike()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto Part=[this](const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh,
                     FVector Location, FVector Scale, FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* P=CreateDefaultSubobject<UStaticMeshComponent>(Name);
        if(Parent) P->SetupAttachment(Parent); else SetRootComponent(P);
        P->SetStaticMesh(Mesh); P->SetRelativeLocation(Location);
        P->SetRelativeScale3D(Scale); P->SetRelativeRotation(Rotation);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return P;
    };
    auto* Frame=Part(TEXT("BMX_Frame"),nullptr,Cube.Object,FVector::ZeroVector,FVector(1));
    // Root keeps unit scale: the riding algorithm uses centimetres and root collision.
    Frame->SetVisibility(false);
    Frame->SetCollisionProfileName(TEXT("PhysicsActor"));
    Frame->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Part(TEXT("DemoTopTube"),Frame,Cube.Object,FVector(0,0,15),FVector(.7,.04,.04));
    Part(TEXT("DemoSeatTube"),Frame,Cube.Object,FVector(-25,0,0),FVector(.04,.04,.4));
    Part(TEXT("DemoSeat"),Frame,Cube.Object,FVector(-25,0,25),FVector(.22,.15,.04));
    auto* Pivot=CreateDefaultSubobject<USceneComponent>(TEXT("BMX_SteeringPivot"));
    Pivot->SetupAttachment(Frame); Pivot->SetRelativeLocation(FVector(35,0,0));
    Part(TEXT("BMX_Steering"),Pivot,Cube.Object,FVector(0,0,25),FVector(.06,.5,.04));
    Part(TEXT("DemoFork"),Pivot,Cube.Object,FVector(0,0,-5),FVector(.04,.04,.5));
    auto* Crank=Part(TEXT("BMX_Crank"),Frame,Cylinder.Object,FVector(-10,0,-15),FVector(.14,.14,.03),FRotator(90,0,0));
    Part(TEXT("BMX_Pedal_L"),Crank,Cube.Object,FVector(0,-4,0),FVector(.6,.6,1));
    Part(TEXT("BMX_Pedal_R"),Crank,Cube.Object,FVector(0,4,0),FVector(.6,.6,1));
    // Runtime replaces each wheel's rotation. Keep the disc orientation on a child mesh.
    for(int32 I=0;I<2;++I)
    {
        auto* Wheel=CreateDefaultSubobject<USceneComponent>(I==0?TEXT("BMX_Wheel_F"):TEXT("BMX_Wheel_R"));
        Wheel->SetupAttachment(Frame);Wheel->SetRelativeLocation(FVector(I==0?45.f:-45.f,0,-27));
        Part(I==0?TEXT("DemoFrontDisc"):TEXT("DemoRearDisc"),Wheel,Cylinder.Object,FVector::ZeroVector,FVector(.54,.54,.06),FRotator(90,0,0));
    }
    auto* Vehicle=CreateDefaultSubobject<UBMXVehicleComponent>(TEXT("BMX_Vehicle"));
    Vehicle->bStartDropped=false;
}

void ASADemoBike::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // Duplicate meshes into transient objects before adding sockets. Engine assets stay untouched.
    TArray<UStaticMeshComponent*> Parts; GetComponents(Parts);
    for(auto* P:Parts)
    {
        FName Socket; FVector Location=FVector::ZeroVector;
        if(P->GetFName()==TEXT("BMX_Frame")) {Socket=TEXT("Socket_Seat");Location=FVector(-25,0,25);}
        else if(P->GetFName()==TEXT("BMX_Pedal_L")) Socket=TEXT("Socket_Pedal_L");
        else if(P->GetFName()==TEXT("BMX_Pedal_R")) Socket=TEXT("Socket_Pedal_R");
        else if(P->GetFName()!=TEXT("BMX_Steering")) continue;
        if(!P->GetStaticMesh()) continue;
        auto* Mesh=DuplicateObject<UStaticMesh>(P->GetStaticMesh(),this);
        auto AddSocket=[Mesh](FName Name,FVector Position)
        {
            auto* S=NewObject<UStaticMeshSocket>(Mesh); S->SocketName=Name;
            S->RelativeLocation=Position; Mesh->Sockets.Add(S);
        };
        if(P->GetFName()==TEXT("BMX_Steering"))
        {AddSocket(TEXT("Socket_Hand_L"),FVector(0,-40,0));AddSocket(TEXT("Socket_Hand_R"),FVector(0,40,0));}
        else AddSocket(Socket,Location);
        P->SetStaticMesh(Mesh);
    }
}

ASADemoCharacter::ASADemoCharacter()
{
    PrimaryActorTick.bCanEverTick=true;
    Interaction=CreateDefaultSubobject<UBMXInteractionComponent>(TEXT("BMX_Interaction"));
    // No skeleton: the placeholder demo deliberately does not demonstrate rider IK.
    Interaction->RiderAnimClass=UAnimInstance::StaticClass();
    auto* Arm=CreateDefaultSubobject<USpringArmComponent>(TEXT("DemoCameraArm"));
    Arm->SetupAttachment(GetRootComponent()); Arm->TargetArmLength=330;
    Arm->SocketOffset=FVector(0,0,65); Arm->bUsePawnControlRotation=true;
    auto* Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("DemoCamera"));
    Camera->SetupAttachment(Arm); Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Camera->PostProcessSettings.MotionBlurAmount=0;
    Placeholder=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DemoRiderPlaceholder"));
    Placeholder->SetupAttachment(GetRootComponent());
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Placeholder->SetStaticMesh(Cube.Object); Placeholder->SetRelativeScale3D(FVector(.3,.3,.5));
    Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASADemoCharacter::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Rider=DemoRiderMesh.LoadSynchronous())
    {
        GetMesh()->SetSkeletalMeshAsset(Rider);
        GetMesh()->SetRelativeLocation(FVector(0,0,-90));
        GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
        Placeholder->SetVisibility(false);
    }
    if(UClass* AnimClass=DemoRiderAnimClass.LoadSynchronous())
    {
        Interaction->RiderAnimClass=AnimClass;
        GetMesh()->SetAnimInstanceClass(AnimClass);
    }
}

void ASADemoCharacter::Tick(float DT)
{
    Super::Tick(DT);
    auto* PC=Cast<APlayerController>(GetController()); if(!PC) return;
    const bool Mounted=Interaction->State!=EBMXState::Dropped;
    Placeholder->SetRelativeLocation(Mounted?FVector(0,0,25):FVector::ZeroVector);
    if(!Mounted)
    {
        const FRotator Heading(0,PC->GetControlRotation().Yaw,0);
        AddMovementInput(Heading.Vector(),(PC->IsInputKeyDown(EKeys::W)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::S)?1.f:0.f));
        AddMovementInput(FRotationMatrix(Heading).GetUnitAxis(EAxis::Y),(PC->IsInputKeyDown(EKeys::D)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::A)?1.f:0.f));
        float MX=0,MY=0; PC->GetInputMouseDelta(MX,MY); AddControllerYawInput(MX*.6f);AddControllerPitchInput(-MY*.6f);
    }
    if(GEngine) GEngine->AddOnScreenDebugMessage(781,0.f,FColor::White,
        TEXT("Primitive BMX demo | WASD move | F mount/exit | Space hop | Alt brake | Ctrl wheelie | Shift sprint\n")+Interaction->LastStatus);
}

ASARuntimeDemoMode::ASARuntimeDemoMode() {DefaultPawnClass=ASADemoCharacter::StaticClass();}
void ASARuntimeDemoMode::StartPlay()
{
    // Build the test ground before Super starts BeginPlay on actors.
    auto* Floor=GetWorld()->SpawnActor<AActor>();
    auto* Ground=NewObject<UStaticMeshComponent>(Floor);
    Floor->SetRootComponent(Ground); Floor->AddInstanceComponent(Ground);
    Ground->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Ground->SetCollisionProfileName(TEXT("BlockAll"));Ground->RegisterComponent();
    Floor->SetActorLocation(FVector(0,0,-10));Floor->SetActorScale3D(FVector(40,40,.2));
    UClass* BikeClass=DemoBikeClass.LoadSynchronous();
    if(!BikeClass) BikeClass=ASADemoBike::StaticClass();
    if(auto* Bike=GetWorld()->SpawnActor<AActor>(BikeClass,FVector(170,0,54.55f),FRotator::ZeroRotator))
    {
        // The rider's spring arm must not retract against the bicycle it is riding.
        // Leave world collision intact so walls still shorten the camera arm.
        TArray<UPrimitiveComponent*> Parts; Bike->GetComponents(Parts);
        for(auto* Part:Parts) Part->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    }
    GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,400),FRotator(-45,-30,0));
    Super::StartPlay();
}
