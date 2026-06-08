#include "RoomBase.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ARoomBase::ARoomBase()
{
    PrimaryActorTick.bCanEverTick = false;

    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    RootComponent = RootScene;

    FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
    FloorMesh->SetupAttachment(RootComponent);

    UpWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("up"));
    UpWall->SetupAttachment(RootComponent);

    DownWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("down"));
    DownWall->SetupAttachment(RootComponent);

    LeftWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("left"));
    LeftWall->SetupAttachment(RootComponent);

    RightWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("right"));
    RightWall->SetupAttachment(RootComponent);
}

void ARoomBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildRoom();
}

void ARoomBase::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Room BeginPlay: Up=%d, Down=%d, Left=%d, Right=%d, Configured=%d"),
        bHasUpNeighbor, bHasDownNeighbor, bHasLeftNeighbor, bHasRightNeighbor, bIsConfigured);

}

void ARoomBase::SetNeighbors(bool bUp, bool bDown, bool bLeft, bool bRight)
{
    UE_LOG(LogTemp, Warning, TEXT("SetNeighbors called: Up=%d, Down=%d, Left=%d, Right=%d"),
        bUp, bDown, bLeft, bRight);

    bHasUpNeighbor = bUp;
    bHasDownNeighbor = bDown;
    bHasLeftNeighbor = bLeft;
    bHasRightNeighbor = bRight;

    bIsConfigured = true;  // Помечаем как сконфигурированную

    UpdateWallVisibility();
}

void ARoomBase::UpdateWallVisibility()
{
    UE_LOG(LogTemp, Warning, TEXT("UpdateWallVisibility: Up=%d, Down=%d, Left=%d, Right=%d, Configured=%d"),
        bHasUpNeighbor, bHasDownNeighbor, bHasLeftNeighbor, bHasRightNeighbor, bIsConfigured);

    SetUpWallVisible(!bHasUpNeighbor);
    SetDownWallVisible(!bHasDownNeighbor);
    SetLeftWallVisible(!bHasLeftNeighbor);
    SetRightWallVisible(!bHasRightNeighbor);
}

void ARoomBase::RebuildRoom()
{
    // Если комната уже сконфигурирована генератором, не перестраиваем
    if (bIsConfigured)
    {
        UE_LOG(LogTemp, Warning, TEXT("RebuildRoom skipped - room already configured"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("RebuildRoom called - building geometry"));

    UStaticMesh* BoxMesh = GetDefaultBoxMesh();

    // Настройка пола
    if (FloorMesh)
    {
        if (!FloorMesh->GetStaticMesh())
        {
            FloorMesh->SetStaticMesh(BoxMesh);
        }

        FVector FloorScale = FVector(
            RoomSize.X / 100.0f,
            RoomSize.Y / 100.0f,
            10.0f / 100.0f
        );

        FloorMesh->SetRelativeScale3D(FloorScale);
        FloorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

        if (FloorMaterial)
        {
            FloorMesh->SetMaterial(0, FloorMaterial);
        }
    }

    float HalfWidth = RoomSize.X / 2.0f;
    float HalfHeight = RoomSize.Y / 2.0f;
    float WallHeight = RoomSize.Z;

    // Верхняя стена (+Y)
    if (UpWall)
    {
        if (!UpWall->GetStaticMesh())
        {
            UpWall->SetStaticMesh(BoxMesh);
        }

        FVector WallScale = FVector(
            RoomSize.X / 100.0f,
            WallThickness / 100.0f,
            WallHeight / 100.0f
        );

        UpWall->SetRelativeScale3D(WallScale);
        UpWall->SetRelativeLocation(FVector(0.0f, HalfHeight, WallOffset.Z));

        if (WallMaterial)
        {
            UpWall->SetMaterial(0, WallMaterial);
        }
    }

    // Нижняя стена (-Y)
    if (DownWall)
    {
        if (!DownWall->GetStaticMesh())
        {
            DownWall->SetStaticMesh(BoxMesh);
        }

        FVector WallScale = FVector(
            RoomSize.X / 100.0f,
            WallThickness / 100.0f,
            WallHeight / 100.0f
        );

        DownWall->SetRelativeScale3D(WallScale);
        DownWall->SetRelativeLocation(FVector(0.0f, -HalfHeight, WallOffset.Z));

        if (WallMaterial)
        {
            DownWall->SetMaterial(0, WallMaterial);
        }
    }

    // Левая стена (-X)
    if (LeftWall)
    {
        if (!LeftWall->GetStaticMesh())
        {
            LeftWall->SetStaticMesh(BoxMesh);
        }

        FVector WallScale = FVector(
            WallThickness / 100.0f,
            RoomSize.Y / 100.0f,
            WallHeight / 100.0f
        );

        LeftWall->SetRelativeScale3D(WallScale);
        LeftWall->SetRelativeLocation(FVector(-HalfWidth, 0.0f, WallOffset.Z));

        if (WallMaterial)
        {
            LeftWall->SetMaterial(0, WallMaterial);
        }
    }

    // Правая стена (+X)
    if (RightWall)
    {
        if (!RightWall->GetStaticMesh())
        {
            RightWall->SetStaticMesh(BoxMesh);
        }

        FVector WallScale = FVector(
            WallThickness / 100.0f,
            RoomSize.Y / 100.0f,
            WallHeight / 100.0f
        );

        RightWall->SetRelativeScale3D(WallScale);
        RightWall->SetRelativeLocation(FVector(HalfWidth, 0.0f, WallOffset.Z));

        if (WallMaterial)
        {
            RightWall->SetMaterial(0, WallMaterial);
        }
    }

    // НЕ вызываем UpdateWallVisibility() здесь!
    // Стены обновятся при вызове SetNeighbors или BeginPlay
}

void ARoomBase::SetUpWallVisible(bool bVisible)
{
    if (UpWall)
    {
        UpWall->SetVisibility(bVisible);
        UpWall->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ARoomBase::SetDownWallVisible(bool bVisible)
{
    if (DownWall)
    {
        DownWall->SetVisibility(bVisible);
        DownWall->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ARoomBase::SetLeftWallVisible(bool bVisible)
{
    if (LeftWall)
    {
        LeftWall->SetVisibility(bVisible);
        LeftWall->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ARoomBase::SetRightWallVisible(bool bVisible)
{
    if (RightWall)
    {
        RightWall->SetVisibility(bVisible);
        RightWall->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ARoomBase::SetAllWallsVisible(bool bVisible)
{
    SetUpWallVisible(bVisible);
    SetDownWallVisible(bVisible);
    SetLeftWallVisible(bVisible);
    SetRightWallVisible(bVisible);
}

UStaticMesh* ARoomBase::GetDefaultBoxMesh()
{
    static UStaticMesh* CachedBoxMesh = nullptr;

    if (!CachedBoxMesh)
    {
        CachedBoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));

        if (!CachedBoxMesh)
        {
            CachedBoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        }

        if (!CachedBoxMesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("ARoomBase: Failed to load default cube mesh!"));
        }
    }

    return CachedBoxMesh;
}