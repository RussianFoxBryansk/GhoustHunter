#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoomBase.h"
#include "GridDungeonGenerator.generated.h"

USTRUCT(BlueprintType)
struct FRoomCell
{
    GENERATED_BODY()

    UPROPERTY()
    bool bIsRoom = false;

    UPROPERTY()
    int32 RoomTypeIndex = 0;

    FRoomCell() {}
};

UCLASS(Blueprintable, BlueprintType)
class GHOUSTHUNTER_API AGridDungeonGenerator : public AActor
{
    GENERATED_BODY()

public:
    AGridDungeonGenerator();

    // ========== Õ¿—“–Œ… » —≈“ » ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    int32 GridWidth = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    int32 GridHeight = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    float CellSize = 1100.0f; // –‡ÁÏÂ ÍÓÏÌ‡Ú˚ + ÌÂ·ÓÎ¸¯ÓÈ Á‡ÁÓ

    // ========== Õ¿—“–Œ… »  ŒÃÕ¿“ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
    TSubclassOf<ARoomBase> DefaultRoomClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
    TArray<TSubclassOf<ARoomBase>> AlternativeRoomClasses;

    // ========== Õ¿—“–Œ… »  À≈“Œ◊ÕŒ√Œ ¿¬“ŒÃ¿“¿ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "100"))
    int32 InitFillPercent = 45;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "8"))
    int32 DeathLimit = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "8"))
    int32 BirthLimit = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "1", ClampMax = "10"))
    int32 SimulationSteps = 5;

    // ========== Õ¿—“–Œ… » œŒ—“Œ¡–¿¡Œ“ » ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    bool bRemoveIsolatedRooms = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    bool bEnsureConnectivity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    int32 MinRoomCount = 10;

    // ========== Õ¿—“–Œ… » —»ƒ¿ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    bool bUseRandomSeed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    bool bAutoGenerateOnStart = true;

    // ========== Œ“À¿ƒ ¿ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
    bool bDrawDebugGrid = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
    float DebugLineDuration = 5.0f;

    // ========== œ”¡À»◊Õ€≈ ‘”Õ ÷»» ==========
    UFUNCTION(BlueprintCallable, Category = "Dungeon")
    void GenerateDungeon();

    UFUNCTION(BlueprintCallable, Category = "Dungeon")
    void ClearDungeon();

    UFUNCTION(BlueprintCallable, Category = "Dungeon")
    void RegenerateWithSameSeed();

    UFUNCTION(BlueprintPure, Category = "Dungeon")
    int32 GetCurrentSeed() const { return CurrentSeed; }

    UFUNCTION(BlueprintPure, Category = "Dungeon")
    int32 GetRoomCount() const { return SpawnedRooms.Num(); }

    UFUNCTION(BlueprintPure, Category = "Dungeon")
    TArray<ARoomBase*> GetSpawnedRooms() const { return SpawnedRooms; }

    UFUNCTION(BlueprintCallable, Category = "Dungeon")
    ARoomBase* GetRoomAt(int32 X, int32 Y) const;

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    TArray<TArray<FRoomCell>> Grid;
    TArray<ARoomBase*> SpawnedRooms;
    int32 CurrentSeed;

    // ========== √≈Õ≈–¿÷»ﬂ —≈“ » ==========
    void InitializeGrid();
    void SimulateCellularAutomaton();
    int32 CountNeighbors(int32 X, int32 Y, bool bCountDiagonals = true) const;
    void ApplyCellularRules();

    // ========== œŒ—“Œ¡–¿¡Œ“ ¿ ==========
    void RemoveIsolatedRooms();
    void EnsureConnectivity();
    void AddCorridor(const FIntPoint& Start, const FIntPoint& End);
    void AddRandomConnections(int32 ExtraConnections = 2);

    // ========== FLOOD FILL ƒÀﬂ  ŒÃœŒÕ≈Õ“ ==========
    TArray<TArray<bool>> GetRoomClusters();
    TArray<FIntPoint> GetLargestCluster();
    TArray<FIntPoint> GetClusterAt(int32 StartX, int32 StartY, TArray<TArray<bool>>& OutVisited);

    // ========== —Œ«ƒ¿Õ»≈  ŒÃÕ¿“ ==========
    void SpawnRooms();
    TSubclassOf<ARoomBase> SelectRoomType(int32 X, int32 Y) const;
    void ConfigureRoomWalls(ARoomBase* Room, int32 X, int32 Y);

    // ========== ¬—œŒÃŒ√¿“≈À‹Õ€≈ ‘”Õ ÷»» ==========
    bool IsValidCell(int32 X, int32 Y) const;
    bool HasRoomAt(int32 X, int32 Y) const;
    TArray<FIntPoint> GetAdjacentRooms(int32 X, int32 Y) const;
    TArray<FIntPoint> GetAllRoomPositions() const;
    int32 GetManhattanDistance(const FIntPoint& A, const FIntPoint& B) const;

    // ========== Œ“À¿ƒ ¿ ==========
    void DebugDrawGrid();

    void CreateMinimalDungeon();
    void FillInteriorHoles();
    void EnsureAllRoomsConnected();
    TArray<TArray<FIntPoint>> GetAllClusters();
    FIntPoint FindBestConnectionPoint(const TArray<FIntPoint>& ClusterFrom, const TArray<FIntPoint>& ClusterTo);
};