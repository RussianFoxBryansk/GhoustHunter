#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "BoxSpawner.generated.h"

UCLASS()
class GHOUSTHUNTER_API ABoxSpawner : public AActor
{
	GENERATED_BODY()

public:
	ABoxSpawner();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner")
	UBoxComponent* SpawnAreaComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Blueprints")
	TArray<TSubclassOf<AActor>> BlueprintClassesToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Settings", meta = (ClampMin = "1", ClampMax = "100"))
	int32 NumberOfObjectsToSpawn = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Settings")
	FVector SpawnAreaSize = FVector(500.0f, 500.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Settings")
	bool bRandomRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner|Settings")
	bool bSpawnOnBeginPlay = true;

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnObjects();

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void ClearSpawnedObjects();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSpawnObjects();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerClearSpawnedObjects();

protected:
	UPROPERTY(Replicated)
	TArray<AActor*> SpawnedActors;

	FVector GetRandomSpawnLocation() const;

	AActor* SpawnSingleBlueprint(const FVector& Location, TSubclassOf<AActor> BlueprintClass);
};