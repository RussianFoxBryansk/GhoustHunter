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

    // Битовые флаги для заблокированных стен (0=Восток, 1=Юг, 2=Запад, 3=Север)
    UPROPERTY()
    uint8 BlockedWalls = 0;

    FRoomCell() {}
};

UENUM(BlueprintType)
enum class ERegionType : uint8
{
    Corridor,   // 1 x N или N x 1
    SmallRoom,  // 2x2, 2x3, 3x2
    LargeRoom,  // 3x3 и больше
    Default
};

UCLASS(Blueprintable, BlueprintType)
class GHOUSTHUNTER_API AGridDungeonGenerator : public AActor
{
    GENERATED_BODY()

public:
    AGridDungeonGenerator();

    // ========== НАСТРОЙКИ СЕТКИ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    int32 GridWidth = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    int32 GridHeight = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Grid")
    float CellSize = 1100.0f;

    // ========== НАСТРОЙКИ КОМНАТ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
    TSubclassOf<ARoomBase> DefaultRoomClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
    TArray<TSubclassOf<ARoomBase>> AlternativeRoomClasses;

    // ========== НАСТРОЙКИ КЛЕТОЧНОГО АВТОМАТА ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "100"))
    int32 InitFillPercent = 45;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "8"))
    int32 DeathLimit = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "0", ClampMax = "8"))
    int32 BirthLimit = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|CellularAutomaton", meta = (ClampMin = "1", ClampMax = "10"))
    int32 SimulationSteps = 5;

    // ========== НАСТРОЙКИ ПОСТОБРАБОТКИ ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    bool bRemoveIsolatedRooms = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    bool bEnsureConnectivity = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess")
    int32 MinRoomCount = 10;

    // НОВЫЕ НАСТРОЙКИ
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess|Maze")
    bool bApplyMazePostProcess = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess|Maze", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WallRestoreChance = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|PostProcess|Maze")
    bool bUseMSTForConnectivity = true;

    // ========== НАСТРОЙКИ СИДА ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    bool bUseRandomSeed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Seed")
    bool bAutoGenerateOnStart = true;

    // ========== ОТЛАДКА ==========
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
    bool bDrawDebugGrid = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
    float DebugLineDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Debug")
    bool bPrintRegionTypes = false;

    // ========== ПУБЛИЧНЫЕ ФУНКЦИИ ==========
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

    UFUNCTION(BlueprintPure, Category = "Dungeon")
    ERegionType GetRegionTypeAt(int32 X, int32 Y) const;

    // ========== НОВЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С СИДОМ ==========
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Seed")
    int32 GetRandomSeed();  // Возвращает случайный сид без генерации

    UFUNCTION(BlueprintCallable, Category = "Dungeon|Seed")
    void SetSeedAndGenerate(int32 NewSeed);  // Устанавливает сид и генерирует

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    TArray<TArray<FRoomCell>> Grid;
    TArray<ARoomBase*> SpawnedRooms;
    int32 CurrentSeed;
    TMap<FIntPoint, ERegionType> RegionCache;

    // Свой генератор случайных чисел
    FRandomStream RNG;

    // ========== ГЕНЕРАЦИЯ СЕТКИ ==========
    void InitializeGrid();
    void SimulateCellularAutomaton();
    int32 CountNeighbors(int32 X, int32 Y, bool bCountDiagonals = true) const;
    void ApplyCellularRules();

    // ========== ПОСТОБРАБОТКА ==========
    void RemoveIsolatedRooms();
    void EnsureConnectivity();
    void AddCorridor(const FIntPoint& Start, const FIntPoint& End);
    void AddRandomConnections(int32 ExtraConnections = 2);
    void FillInteriorHoles();
    void EnsureAllRoomsConnected();
    void CreateMinimalDungeon();

    // ========== БАЗОВЫЕ МЕТОДЫ РАБОТЫ С СЕТКОЙ ==========
    bool IsValidCell(int32 X, int32 Y) const;
    bool HasRoomAt(int32 X, int32 Y) const;
    TArray<FIntPoint> GetAdjacentRooms(int32 X, int32 Y) const;
    TArray<FIntPoint> GetAllRoomPositions() const;
    int32 GetManhattanDistance(const FIntPoint& A, const FIntPoint& B) const;

    // ========== МЕТОДЫ ДЛЯ ЛАБИРИНТА ==========
    void PostProcessWallsToCreateMaze();
    void CreateInternalMaze(const TArray<FIntPoint>& Region, int32 MinX, int32 MaxX, int32 MinY, int32 MaxY);
    void AddRandomWallsToRegion(const TArray<FIntPoint>& Region);
    void RestoreWallBetween(const FIntPoint& RoomA, const FIntPoint& RoomB);
    void BlockWallAt(const FIntPoint& Room, int32 Direction);
    bool IsWallBlocked(const FIntPoint& Room, int32 Direction) const;
    bool AreRoomsAdjacent(const FIntPoint& A, const FIntPoint& B) const;

    // ========== КЛАССИФИКАЦИЯ РЕГИОНОВ ==========
    void ClassifyRegions();
    ERegionType CalculateRegionType(const TArray<FIntPoint>& Region) const;

    // ========== FLOOD FILL ДЛЯ КОМПОНЕНТ ==========
    TArray<FIntPoint> GetLargestCluster();
    TArray<FIntPoint> GetClusterAt(int32 StartX, int32 StartY, TArray<TArray<bool>>& OutVisited);
    TArray<TArray<FIntPoint>> GetAllClusters();
    FIntPoint FindBestConnectionPoint(const TArray<FIntPoint>& ClusterFrom, const TArray<FIntPoint>& ClusterTo);

    // ========== СОЗДАНИЕ КОМНАТ ==========
    void SpawnRooms();
    TSubclassOf<ARoomBase> SelectRoomType(int32 X, int32 Y) const;
    void ConfigureRoomWalls(ARoomBase* Room, int32 X, int32 Y);

    // ========== ОТЛАДКА ==========
    void DebugDrawGrid();
    void DebugPrintRegions();
};