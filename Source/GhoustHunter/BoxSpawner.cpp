#include "BoxSpawner.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ABoxSpawner::ABoxSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SpawnAreaComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnAreaComponent"));
	RootComponent = SpawnAreaComponent;
	SpawnAreaComponent->SetBoxExtent(SpawnAreaSize);
	SpawnAreaComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnAreaComponent->SetVisibility(true);
}

void ABoxSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoxSpawner, SpawnedActors);
}

void ABoxSpawner::BeginPlay()
{
	Super::BeginPlay();
	SpawnAreaComponent->SetBoxExtent(SpawnAreaSize);

	if (bSpawnOnBeginPlay && HasAuthority())
	{
		SpawnObjects();
	}
}

void ABoxSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ABoxSpawner::SpawnObjects()
{
	if (!HasAuthority())
	{
		ServerSpawnObjects();
		return;
	}

	ClearSpawnedObjects();

	if (BlueprintClassesToSpawn.Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	for (int32 i = 0; i < NumberOfObjectsToSpawn; i++)
	{
		int32 ClassIndex = FMath::RandRange(0, BlueprintClassesToSpawn.Num() - 1);
		TSubclassOf<AActor> SelectedClass = BlueprintClassesToSpawn[ClassIndex];

		if (SelectedClass)
		{
			FVector SpawnLocation = GetRandomSpawnLocation();
			AActor* SpawnedActor = SpawnSingleBlueprint(SpawnLocation, SelectedClass);

			if (SpawnedActor)
			{
				SpawnedActors.Add(SpawnedActor);
			}
		}
	}
}

AActor* ABoxSpawner::SpawnSingleBlueprint(const FVector& Location, TSubclassOf<AActor> BlueprintClass)
{
	UWorld* World = GetWorld();
	if (!World || !BlueprintClass) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(BlueprintClass, Location, FRotator::ZeroRotator, SpawnParams);

	if (NewActor && bRandomRotation)
	{
		FRotator RandomRotation = FRotator(
			FMath::RandRange(0.0f, 360.0f),
			FMath::RandRange(0.0f, 360.0f),
			FMath::RandRange(0.0f, 360.0f)
		);
		NewActor->SetActorRotation(RandomRotation);
	}

	return NewActor;
}

void ABoxSpawner::ClearSpawnedObjects()
{
	if (!HasAuthority())
	{
		ServerClearSpawnedObjects();
		return;
	}

	for (AActor* Actor : SpawnedActors)
	{
		if (Actor && IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	SpawnedActors.Empty();
}

void ABoxSpawner::ServerSpawnObjects_Implementation()
{
	SpawnObjects();
}

bool ABoxSpawner::ServerSpawnObjects_Validate()
{
	return true;
}

void ABoxSpawner::ServerClearSpawnedObjects_Implementation()
{
	ClearSpawnedObjects();
}

bool ABoxSpawner::ServerClearSpawnedObjects_Validate()
{
	return true;
}

FVector ABoxSpawner::GetRandomSpawnLocation() const
{
	FVector Origin = GetActorLocation();
	FVector Extent = SpawnAreaSize;

	float X = FMath::RandRange(-Extent.X, Extent.X);
	float Y = FMath::RandRange(-Extent.Y, Extent.Y);
	float Z = FMath::RandRange(0.0f, static_cast<float>(Extent.Z * 2.0f));

	return Origin + FVector(X, Y, Z);
}