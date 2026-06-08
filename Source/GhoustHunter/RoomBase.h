#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "RoomBase.generated.h"

UCLASS(Blueprintable, BlueprintType)
class GHOUSTHUNTER_API ARoomBase : public AActor
{
    GENERATED_BODY()

public:
    ARoomBase();

    // Компоненты
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    USceneComponent* RootScene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    UStaticMeshComponent* FloorMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    UStaticMeshComponent* UpWall;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    UStaticMeshComponent* DownWall;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    UStaticMeshComponent* LeftWall;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room|Components")
    UStaticMeshComponent* RightWall;

    // Свойства комнаты
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Settings")
    FVector RoomSize = FVector(1000.0f, 1000.0f, 500.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Settings")
    float WallThickness = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Settings")
    FVector WallOffset = FVector(0.0f, 0.0f, 250.0f);

    // Материалы
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Visual")
    UMaterialInterface* FloorMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room|Visual")
    UMaterialInterface* WallMaterial;

    // Функции управления стенами
    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetUpWallVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetDownWallVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetLeftWallVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetRightWallVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetAllWallsVisible(bool bVisible);

    // Функции информации о соседях
    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    bool HasUpNeighbor() const { return bHasUpNeighbor; }

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    bool HasDownNeighbor() const { return bHasDownNeighbor; }

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    bool HasLeftNeighbor() const { return bHasLeftNeighbor; }

    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    bool HasRightNeighbor() const { return bHasRightNeighbor; }

    // Установка соседей (вызывается генератором)
    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void SetNeighbors(bool bUp, bool bDown, bool bLeft, bool bRight);

    // Принудительное обновление видимости стен
    UFUNCTION(BlueprintCallable, Category = "Room|Walls")
    void UpdateWallVisibility();

    // Перестроить геометрию по текущим настройкам
    UFUNCTION(BlueprintCallable, Category = "Room|Build")
    void RebuildRoom();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    bool bHasUpNeighbor = false;
    bool bHasDownNeighbor = false;
    bool bHasLeftNeighbor = false;
    bool bHasRightNeighbor = false;

    bool bIsConfigured = false;
    UStaticMesh* GetDefaultBoxMesh();
};