#include "GridDungeonGenerator.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include <functional> 
#include <algorithm> 

AGridDungeonGenerator::AGridDungeonGenerator()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AGridDungeonGenerator::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoGenerateOnStart && DefaultRoomClass)
    {
        GenerateDungeon();
    }
}

#if WITH_EDITOR
void AGridDungeonGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (bAutoGenerateOnStart && DefaultRoomClass && GetWorld() && !GetWorld()->IsGameWorld())
    {
        GenerateDungeon();
    }
}
#endif

// ========== НОВЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С СИДОМ ==========

int32 AGridDungeonGenerator::GetRandomSeed()
{
    // Просто возвращает случайное число, не влияя на генерацию
    return FMath::Rand();
}

void AGridDungeonGenerator::SetSeedAndGenerate(int32 NewSeed)
{
    Seed = NewSeed;
    bUseRandomSeed = false;
    GenerateDungeon();
}

// ========== ОСНОВНАЯ ГЕНЕРАЦИЯ ==========

void AGridDungeonGenerator::GenerateDungeon()
{
    ClearDungeon();

    // Определяем сид
    if (bUseRandomSeed || Seed == 0)
    {
        CurrentSeed = FMath::Rand();
    }
    else
    {
        CurrentSeed = Seed;
    }

    // Инициализируем свой генератор случайных чисел
    RNG.Initialize(CurrentSeed);

    // Также инициализируем глобальный (для совместимости)
    FMath::RandInit(CurrentSeed);

    UE_LOG(LogTemp, Log, TEXT("Generating dungeon with seed: %d, grid: %dx%d"), CurrentSeed, GridWidth, GridHeight);

    bool bSuccess = false;
    int32 Attempts = 0;
    const int32 MaxAttempts = 10;

    while (!bSuccess && Attempts < MaxAttempts)
    {
        InitializeGrid();

        for (int32 Step = 0; Step < SimulationSteps; Step++)
        {
            ApplyCellularRules();
        }

        FillInteriorHoles();
        RemoveIsolatedRooms();
        EnsureAllRoomsConnected();

        TArray<FIntPoint> RoomPositions = GetAllRoomPositions();

        if (RoomPositions.Num() >= MinRoomCount)
        {
            bSuccess = true;
            UE_LOG(LogTemp, Log, TEXT("Generation successful after %d attempts, rooms: %d"), Attempts + 1, RoomPositions.Num());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Attempt %d failed, only %d rooms (need %d), retrying..."), Attempts + 1, RoomPositions.Num(), MinRoomCount);
            Attempts++;
        }
    }

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to generate dungeon after %d attempts!"), MaxAttempts);
        CreateMinimalDungeon();
    }

    if (bApplyMazePostProcess)
    {
        PostProcessWallsToCreateMaze();
    }

    ClassifyRegions();

    if (bPrintRegionTypes)
    {
        DebugPrintRegions();
    }

    SpawnRooms();

    if (bDrawDebugGrid)
    {
        DebugDrawGrid();
    }

    UE_LOG(LogTemp, Log, TEXT("Final rooms count: %d"), SpawnedRooms.Num());
}

void AGridDungeonGenerator::CreateMinimalDungeon()
{
    Grid.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Grid[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Grid[Y][X].bIsRoom = false;
            Grid[Y][X].BlockedWalls = 0;
        }
    }

    int32 CenterX = GridWidth / 2;
    int32 CenterY = GridHeight / 2;

    for (int32 I = -3; I <= 3; I++)
    {
        if (IsValidCell(CenterX + I, CenterY))
        {
            Grid[CenterY][CenterX + I].bIsRoom = true;
            Grid[CenterY][CenterX + I].BlockedWalls = 0;
        }
        if (IsValidCell(CenterX, CenterY + I))
        {
            Grid[CenterY + I][CenterX].bIsRoom = true;
            Grid[CenterY + I][CenterX].BlockedWalls = 0;
        }
    }
}

void AGridDungeonGenerator::ClearDungeon()
{
    for (ARoomBase* Room : SpawnedRooms)
    {
        if (Room && IsValid(Room))
        {
            Room->Destroy();
        }
    }
    SpawnedRooms.Empty();
    Grid.Empty();
    RegionCache.Empty();
}

void AGridDungeonGenerator::RegenerateWithSameSeed()
{
    Seed = CurrentSeed;
    bUseRandomSeed = false;
    GenerateDungeon();
}

// ========== ГЕНЕРАЦИЯ СЕТКИ ==========

void AGridDungeonGenerator::InitializeGrid()
{
    Grid.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Grid[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Grid[Y][X].BlockedWalls = 0;

            if (X == 0 || X == GridWidth - 1 || Y == 0 || Y == GridHeight - 1)
            {
                Grid[Y][X].bIsRoom = false;
            }
            else
            {
                if (X == GridWidth / 2 && Y == GridHeight / 2)
                {
                    Grid[Y][X].bIsRoom = true;
                }
                else
                {
                    // Используем RNG вместо FMath::RandRange
                    Grid[Y][X].bIsRoom = RNG.RandRange(0, 100) < InitFillPercent;
                }
            }
        }
    }
}

void AGridDungeonGenerator::ApplyCellularRules()
{
    TArray<TArray<FRoomCell>> NewGrid = Grid;

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (X == 0 || X == GridWidth - 1 || Y == 0 || Y == GridHeight - 1)
            {
                NewGrid[Y][X].bIsRoom = false;
                continue;
            }

            int32 Neighbors = CountNeighbors(X, Y, false);

            if (Grid[Y][X].bIsRoom)
            {
                if (Neighbors < 2)
                {
                    NewGrid[Y][X].bIsRoom = false;
                }
            }
            else
            {
                if (Neighbors >= 2 && Neighbors <= 3)
                {
                    NewGrid[Y][X].bIsRoom = true;
                }
            }
        }
    }

    Grid = NewGrid;
}

int32 AGridDungeonGenerator::CountNeighbors(int32 X, int32 Y, bool bCountDiagonals) const
{
    int32 Count = 0;

    if (HasRoomAt(X + 1, Y)) Count++;
    if (HasRoomAt(X - 1, Y)) Count++;
    if (HasRoomAt(X, Y + 1)) Count++;
    if (HasRoomAt(X, Y - 1)) Count++;

    if (bCountDiagonals)
    {
        if (HasRoomAt(X + 1, Y + 1)) Count++;
        if (HasRoomAt(X - 1, Y + 1)) Count++;
        if (HasRoomAt(X + 1, Y - 1)) Count++;
        if (HasRoomAt(X - 1, Y - 1)) Count++;
    }

    return Count;
}

// ========== ЗАПОЛНЕНИЕ ДЫР ==========

void AGridDungeonGenerator::FillInteriorHoles()
{
    TArray<FIntPoint> HolesToFill;

    for (int32 Y = 1; Y < GridHeight - 1; Y++)
    {
        for (int32 X = 1; X < GridWidth - 1; X++)
        {
            if (!Grid[Y][X].bIsRoom)
            {
                bool bSurroundedByRooms =
                    HasRoomAt(X + 1, Y) &&
                    HasRoomAt(X - 1, Y) &&
                    HasRoomAt(X, Y + 1) &&
                    HasRoomAt(X, Y - 1);

                if (bSurroundedByRooms)
                {
                    HolesToFill.Add(FIntPoint(X, Y));
                }
            }
        }
    }

    for (const FIntPoint& Hole : HolesToFill)
    {
        Grid[Hole.Y][Hole.X].bIsRoom = true;
        Grid[Hole.Y][Hole.X].BlockedWalls = 0;
    }
}

// ========== СВЯЗНОСТЬ ==========

void AGridDungeonGenerator::RemoveIsolatedRooms()
{
    if (!bRemoveIsolatedRooms) return;

    TArray<FIntPoint> LargestCluster = GetLargestCluster();

    if (LargestCluster.Num() == 0) return;

    TSet<FIntPoint> KeepRooms;
    for (const FIntPoint& Room : LargestCluster)
    {
        KeepRooms.Add(Room);
    }

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (Grid[Y][X].bIsRoom && !KeepRooms.Contains(FIntPoint(X, Y)))
            {
                Grid[Y][X].bIsRoom = false;
            }
        }
    }
}

void AGridDungeonGenerator::EnsureAllRoomsConnected()
{
    if (!bEnsureConnectivity) return;

    TArray<FIntPoint> RoomPositions = GetAllRoomPositions();
    if (RoomPositions.Num() <= 1) return;

    TArray<TArray<FIntPoint>> Clusters = GetAllClusters();

    if (Clusters.Num() <= 1) return;

    UE_LOG(LogTemp, Log, TEXT("Found %d disconnected clusters, connecting them..."), Clusters.Num());

    for (int32 I = 0; I < Clusters.Num() - 1; I++)
    {
        FIntPoint Point1 = FindBestConnectionPoint(Clusters[I], Clusters[I + 1]);
        FIntPoint Point2 = FindBestConnectionPoint(Clusters[I + 1], Clusters[I]);
        AddCorridor(Point1, Point2);
    }

    FillInteriorHoles();
}

TArray<FIntPoint> AGridDungeonGenerator::GetLargestCluster()
{
    TArray<TArray<bool>> Visited;
    Visited.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Visited[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Visited[Y][X] = false;
        }
    }

    TArray<FIntPoint> LargestCluster;
    int32 LargestSize = 0;

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (Grid[Y][X].bIsRoom && !Visited[Y][X])
            {
                TArray<FIntPoint> Cluster = GetClusterAt(X, Y, Visited);

                if (Cluster.Num() > LargestSize)
                {
                    LargestSize = Cluster.Num();
                    LargestCluster = Cluster;
                }
            }
        }
    }

    return LargestCluster;
}

TArray<FIntPoint> AGridDungeonGenerator::GetClusterAt(int32 StartX, int32 StartY, TArray<TArray<bool>>& OutVisited)
{
    TArray<FIntPoint> Cluster;
    TQueue<FIntPoint> Queue;

    Queue.Enqueue(FIntPoint(StartX, StartY));
    OutVisited[StartY][StartX] = true;

    while (!Queue.IsEmpty())
    {
        FIntPoint Current;
        Queue.Dequeue(Current);
        Cluster.Add(Current);

        for (const FIntPoint& Neighbor : GetAdjacentRooms(Current.X, Current.Y))
        {
            if (!OutVisited[Neighbor.Y][Neighbor.X])
            {
                OutVisited[Neighbor.Y][Neighbor.X] = true;
                Queue.Enqueue(Neighbor);
            }
        }
    }

    return Cluster;
}

TArray<TArray<FIntPoint>> AGridDungeonGenerator::GetAllClusters()
{
    TArray<TArray<bool>> Visited;
    Visited.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Visited[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Visited[Y][X] = false;
        }
    }

    TArray<TArray<FIntPoint>> AllClusters;

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (Grid[Y][X].bIsRoom && !Visited[Y][X])
            {
                TArray<FIntPoint> Cluster = GetClusterAt(X, Y, Visited);
                if (Cluster.Num() > 0)
                {
                    AllClusters.Add(Cluster);
                }
            }
        }
    }

    return AllClusters;
}

FIntPoint AGridDungeonGenerator::FindBestConnectionPoint(const TArray<FIntPoint>& ClusterFrom, const TArray<FIntPoint>& ClusterTo)
{
    int32 BestDist = INT_MAX;
    FIntPoint BestPoint = ClusterFrom[0];

    for (const FIntPoint& PointFrom : ClusterFrom)
    {
        for (const FIntPoint& PointTo : ClusterTo)
        {
            int32 Dist = GetManhattanDistance(PointFrom, PointTo);
            if (Dist < BestDist)
            {
                BestDist = Dist;
                BestPoint = PointFrom;
            }
        }
    }

    return BestPoint;
}

void AGridDungeonGenerator::AddCorridor(const FIntPoint& Start, const FIntPoint& End)
{
    int32 X = Start.X;
    int32 Y = Start.Y;

    int32 StepX = (End.X > X) ? 1 : (End.X < X) ? -1 : 0;
    int32 StepY = (End.Y > Y) ? 1 : (End.Y < Y) ? -1 : 0;

    while (X != End.X)
    {
        X += StepX;
        if (IsValidCell(X, Y))
        {
            Grid[Y][X].bIsRoom = true;
            Grid[Y][X].BlockedWalls = 0;
        }
    }

    while (Y != End.Y)
    {
        Y += StepY;
        if (IsValidCell(X, Y))
        {
            Grid[Y][X].bIsRoom = true;
            Grid[Y][X].BlockedWalls = 0;
        }
    }
}

// ========== ЛАБИРИНТ (ПОСТОБРАБОТКА СТЕН) ==========

void AGridDungeonGenerator::PostProcessWallsToCreateMaze()
{
    if (WallRestoreChance <= 0.0f) return;

    TArray<TArray<FIntPoint>> AllRegions;
    TArray<TArray<bool>> Visited;

    Visited.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Visited[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Visited[Y][X] = false;
        }
    }

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (HasRoomAt(X, Y) && !Visited[Y][X])
            {
                TArray<FIntPoint> Region;
                TQueue<FIntPoint> Queue;
                Queue.Enqueue(FIntPoint(X, Y));
                Visited[Y][X] = true;

                while (!Queue.IsEmpty())
                {
                    FIntPoint Current;
                    Queue.Dequeue(Current);
                    Region.Add(Current);

                    TArray<FIntPoint> Neighbors;
                    if (HasRoomAt(Current.X + 1, Current.Y) && !IsWallBlocked(Current, 0))
                        Neighbors.Add(FIntPoint(Current.X + 1, Current.Y));
                    if (HasRoomAt(Current.X - 1, Current.Y) && !IsWallBlocked(Current, 2))
                        Neighbors.Add(FIntPoint(Current.X - 1, Current.Y));
                    if (HasRoomAt(Current.X, Current.Y + 1) && !IsWallBlocked(Current, 1))
                        Neighbors.Add(FIntPoint(Current.X, Current.Y + 1));
                    if (HasRoomAt(Current.X, Current.Y - 1) && !IsWallBlocked(Current, 3))
                        Neighbors.Add(FIntPoint(Current.X, Current.Y - 1));

                    for (const FIntPoint& Neighbor : Neighbors)
                    {
                        if (!Visited[Neighbor.Y][Neighbor.X])
                        {
                            Visited[Neighbor.Y][Neighbor.X] = true;
                            Queue.Enqueue(Neighbor);
                        }
                    }
                }

                if (Region.Num() > 0)
                {
                    AllRegions.Add(Region);
                }
            }
        }
    }

    for (const TArray<FIntPoint>& Region : AllRegions)
    {
        int32 MinX = GridWidth, MaxX = -1, MinY = GridHeight, MaxY = -1;
        for (const FIntPoint& Cell : Region)
        {
            MinX = FMath::Min(MinX, Cell.X);
            MaxX = FMath::Max(MaxX, Cell.X);
            MinY = FMath::Min(MinY, Cell.Y);
            MaxY = FMath::Max(MaxY, Cell.Y);
        }

        int32 Width = MaxX - MinX + 1;
        int32 Height = MaxY - MinY + 1;

        if (Width >= 4 && Height >= 4 && Region.Num() >= 8)
        {
            CreateInternalMaze(Region, MinX, MaxX, MinY, MaxY);
        }
        else if (Region.Num() >= 6)
        {
            AddRandomWallsToRegion(Region);
        }
    }
}

void AGridDungeonGenerator::CreateInternalMaze(const TArray<FIntPoint>& Region, int32 MinX, int32 MaxX, int32 MinY, int32 MaxY)
{
    int32 MazeWidth = MaxX - MinX + 1;
    int32 MazeHeight = MaxY - MinY + 1;

    // Сначала удаляем ВСЕ внутренние стены
    for (int32 Y = MinY; Y <= MaxY; Y++)
    {
        for (int32 X = MinX; X <= MaxX; X++)
        {
            if (HasRoomAt(X, Y))
            {
                if (HasRoomAt(X, Y + 1))
                {
                    Grid[Y][X].BlockedWalls &= ~(1 << 1);
                    Grid[Y + 1][X].BlockedWalls &= ~(1 << 3);
                }
                if (HasRoomAt(X + 1, Y))
                {
                    Grid[Y][X].BlockedWalls &= ~(1 << 0);
                    Grid[Y][X + 1].BlockedWalls &= ~(1 << 2);
                }
            }
        }
    }

    TArray<TPair<FIntPoint, int32>> AllWalls;

    for (int32 Y = MinY; Y <= MaxY; Y++)
    {
        for (int32 X = MinX; X <= MaxX; X++)
        {
            if (!HasRoomAt(X, Y)) continue;

            if (HasRoomAt(X + 1, Y))
            {
                AllWalls.Add(TPair<FIntPoint, int32>(FIntPoint(X, Y), 0));
            }
            if (HasRoomAt(X, Y + 1))
            {
                AllWalls.Add(TPair<FIntPoint, int32>(FIntPoint(X, Y), 1));
            }
        }
    }

    // Используем RNG для перемешивания
    for (int32 I = AllWalls.Num() - 1; I > 0; I--)
    {
        int32 J = RNG.RandRange(0, I);
        AllWalls.Swap(I, J);
    }

    TMap<FIntPoint, FIntPoint> Parent;
    for (const FIntPoint& Cell : Region)
    {
        Parent.Add(Cell, Cell);
    }

    std::function<FIntPoint(FIntPoint)> Find = [&](FIntPoint P) -> FIntPoint
        {
            if (Parent[P] == P) return P;
            Parent[P] = Find(Parent[P]);
            return Parent[P];
        };

    auto Union = [&](FIntPoint A, FIntPoint B)
        {
            FIntPoint RootA = Find(A);
            FIntPoint RootB = Find(B);
            if (RootA != RootB) Parent[RootA] = RootB;
        };

    TArray<TPair<FIntPoint, int32>> MSTWalls;

    for (const auto& Wall : AllWalls)
    {
        FIntPoint RoomA = Wall.Key;
        int32 Dir = Wall.Value;
        FIntPoint RoomB = (Dir == 0) ? FIntPoint(RoomA.X + 1, RoomA.Y) : FIntPoint(RoomA.X, RoomA.Y + 1);

        if (Find(RoomA) != Find(RoomB))
        {
            Union(RoomA, RoomB);
        }
        else
        {
            // Используем RNG для случайного восстановления стен
            if (RNG.FRand() < WallRestoreChance)
            {
                MSTWalls.Add(Wall);
            }
        }
    }

    for (const auto& Wall : MSTWalls)
    {
        FIntPoint RoomA = Wall.Key;
        int32 Dir = Wall.Value;
        FIntPoint RoomB = (Dir == 0) ? FIntPoint(RoomA.X + 1, RoomA.Y) : FIntPoint(RoomA.X, RoomA.Y + 1);
        RestoreWallBetween(RoomA, RoomB);
    }
}

void AGridDungeonGenerator::AddRandomWallsToRegion(const TArray<FIntPoint>& Region)
{
    if (Region.Num() < 5) return;

    TArray<TPair<FIntPoint, int32>> PossibleWalls;

    for (const FIntPoint& Cell : Region)
    {
        if (HasRoomAt(Cell.X + 1, Cell.Y) && !IsWallBlocked(Cell, 0))
        {
            PossibleWalls.Add(TPair<FIntPoint, int32>(Cell, 0));
        }
        if (HasRoomAt(Cell.X, Cell.Y + 1) && !IsWallBlocked(Cell, 1))
        {
            PossibleWalls.Add(TPair<FIntPoint, int32>(Cell, 1));
        }
    }

    // Используем RNG для перемешивания
    for (int32 I = PossibleWalls.Num() - 1; I > 0; I--)
    {
        int32 J = RNG.RandRange(0, I);
        PossibleWalls.Swap(I, J);
    }

    int32 WallsToAdd = FMath::Min(FMath::CeilToInt(PossibleWalls.Num() * 0.2f), PossibleWalls.Num() - 3);

    for (int32 I = 0; I < WallsToAdd; I++)
    {
        const auto& Wall = PossibleWalls[I];
        FIntPoint RoomA = Wall.Key;
        int32 Direction = Wall.Value;
        FIntPoint RoomB = (Direction == 0) ? FIntPoint(RoomA.X + 1, RoomA.Y) : FIntPoint(RoomA.X, RoomA.Y + 1);

        int32 ConnectionsA = 0;
        int32 ConnectionsB = 0;

        if (HasRoomAt(RoomA.X + 1, RoomA.Y) && !IsWallBlocked(RoomA, 0)) ConnectionsA++;
        if (HasRoomAt(RoomA.X - 1, RoomA.Y) && !IsWallBlocked(RoomA, 2)) ConnectionsA++;
        if (HasRoomAt(RoomA.X, RoomA.Y + 1) && !IsWallBlocked(RoomA, 1)) ConnectionsA++;
        if (HasRoomAt(RoomA.X, RoomA.Y - 1) && !IsWallBlocked(RoomA, 3)) ConnectionsA++;

        if (HasRoomAt(RoomB.X + 1, RoomB.Y) && !IsWallBlocked(RoomB, 0)) ConnectionsB++;
        if (HasRoomAt(RoomB.X - 1, RoomB.Y) && !IsWallBlocked(RoomB, 2)) ConnectionsB++;
        if (HasRoomAt(RoomB.X, RoomB.Y + 1) && !IsWallBlocked(RoomB, 1)) ConnectionsB++;
        if (HasRoomAt(RoomB.X, RoomB.Y - 1) && !IsWallBlocked(RoomB, 3)) ConnectionsB++;

        if (ConnectionsA > 1 && ConnectionsB > 1)
        {
            RestoreWallBetween(RoomA, RoomB);
        }
    }
}

void AGridDungeonGenerator::RestoreWallBetween(const FIntPoint& RoomA, const FIntPoint& RoomB)
{
    if (RoomB.X == RoomA.X + 1)
    {
        BlockWallAt(RoomA, 0);
        BlockWallAt(RoomB, 2);
    }
    else if (RoomB.X == RoomA.X - 1)
    {
        BlockWallAt(RoomA, 2);
        BlockWallAt(RoomB, 0);
    }
    else if (RoomB.Y == RoomA.Y + 1)
    {
        BlockWallAt(RoomA, 1);
        BlockWallAt(RoomB, 3);
    }
    else if (RoomB.Y == RoomA.Y - 1)
    {
        BlockWallAt(RoomA, 3);
        BlockWallAt(RoomB, 1);
    }
}

void AGridDungeonGenerator::BlockWallAt(const FIntPoint& Room, int32 Direction)
{
    if (!IsValidCell(Room.X, Room.Y)) return;
    Grid[Room.Y][Room.X].BlockedWalls |= (1 << Direction);
}

bool AGridDungeonGenerator::IsWallBlocked(const FIntPoint& Room, int32 Direction) const
{
    if (!IsValidCell(Room.X, Room.Y)) return true;
    return (Grid[Room.Y][Room.X].BlockedWalls & (1 << Direction)) != 0;
}

bool AGridDungeonGenerator::AreRoomsAdjacent(const FIntPoint& A, const FIntPoint& B) const
{
    return (FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y)) == 1;
}

// ========== КЛАССИФИКАЦИЯ РЕГИОНОВ ==========

void AGridDungeonGenerator::ClassifyRegions()
{
    RegionCache.Empty();

    TArray<TArray<bool>> Visited;
    Visited.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Visited[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Visited[Y][X] = false;
        }
    }

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (HasRoomAt(X, Y) && !Visited[Y][X])
            {
                TArray<FIntPoint> Region;
                TQueue<FIntPoint> Queue;
                Queue.Enqueue(FIntPoint(X, Y));
                Visited[Y][X] = true;

                while (!Queue.IsEmpty())
                {
                    FIntPoint Current;
                    Queue.Dequeue(Current);
                    Region.Add(Current);

                    TArray<FIntPoint> Neighbors;
                    if (HasRoomAt(Current.X + 1, Current.Y) && !IsWallBlocked(Current, 0))
                        Neighbors.Add(FIntPoint(Current.X + 1, Current.Y));
                    if (HasRoomAt(Current.X - 1, Current.Y) && !IsWallBlocked(Current, 2))
                        Neighbors.Add(FIntPoint(Current.X - 1, Current.Y));
                    if (HasRoomAt(Current.X, Current.Y + 1) && !IsWallBlocked(Current, 1))
                        Neighbors.Add(FIntPoint(Current.X, Current.Y + 1));
                    if (HasRoomAt(Current.X, Current.Y - 1) && !IsWallBlocked(Current, 3))
                        Neighbors.Add(FIntPoint(Current.X, Current.Y - 1));

                    for (const FIntPoint& Neighbor : Neighbors)
                    {
                        if (!Visited[Neighbor.Y][Neighbor.X])
                        {
                            Visited[Neighbor.Y][Neighbor.X] = true;
                            Queue.Enqueue(Neighbor);
                        }
                    }
                }

                ERegionType Type = CalculateRegionType(Region);

                for (const FIntPoint& Cell : Region)
                {
                    RegionCache.Add(Cell, Type);
                }
            }
        }
    }
}

ERegionType AGridDungeonGenerator::CalculateRegionType(const TArray<FIntPoint>& Region) const
{
    if (Region.Num() == 0) return ERegionType::Default;

    int32 MinX = GridWidth, MaxX = -1, MinY = GridHeight, MaxY = -1;
    for (const FIntPoint& Cell : Region)
    {
        MinX = FMath::Min(MinX, Cell.X);
        MaxX = FMath::Max(MaxX, Cell.X);
        MinY = FMath::Min(MinY, Cell.Y);
        MaxY = FMath::Max(MaxY, Cell.Y);
    }

    int32 Width = MaxX - MinX + 1;
    int32 Height = MaxY - MinY + 1;

    if (Width == 1 || Height == 1)
    {
        return ERegionType::Corridor;
    }
    else if ((Width == 2 && Height == 2) || (Width == 2 && Height == 3) || (Width == 3 && Height == 2))
    {
        return ERegionType::SmallRoom;
    }
    else if (Width >= 3 && Height >= 3)
    {
        return ERegionType::LargeRoom;
    }

    return ERegionType::Default;
}

ERegionType AGridDungeonGenerator::GetRegionTypeAt(int32 X, int32 Y) const
{
    if (const ERegionType* Type = RegionCache.Find(FIntPoint(X, Y)))
    {
        return *Type;
    }
    return ERegionType::Default;
}

// ========== СОЗДАНИЕ КОМНАТ ==========

void AGridDungeonGenerator::SpawnRooms()
{
    UWorld* World = GetWorld();
    if (!World || !DefaultRoomClass) return;

    FVector GeneratorLocation = GetActorLocation();
    TArray<FIntPoint> RoomPositions = GetAllRoomPositions();

    for (const FIntPoint& Pos : RoomPositions)
    {
        float OffsetX = (Pos.X - GridWidth / 2.0f) * CellSize;
        float OffsetY = (Pos.Y - GridHeight / 2.0f) * CellSize;
        FVector Location = GeneratorLocation + FVector(OffsetX, OffsetY, 0.0f);

        TSubclassOf<ARoomBase> RoomClass = SelectRoomType(Pos.X, Pos.Y);
        ARoomBase* NewRoom = World->SpawnActor<ARoomBase>(RoomClass, Location, FRotator::ZeroRotator);

        if (NewRoom)
        {
            ConfigureRoomWalls(NewRoom, Pos.X, Pos.Y);
            SpawnedRooms.Add(NewRoom);
        }
    }
}

TSubclassOf<ARoomBase> AGridDungeonGenerator::SelectRoomType(int32 X, int32 Y) const
{
    if (AlternativeRoomClasses.Num() == 0)
    {
        return DefaultRoomClass;
    }

    int32 Hash = (X * 73856093) ^ (Y * 19349663) ^ (CurrentSeed * 83492791);
    int32 Index = FMath::Abs(Hash) % (AlternativeRoomClasses.Num() + 1);

    if (Index > 0 && Index <= AlternativeRoomClasses.Num())
    {
        return AlternativeRoomClasses[Index - 1];
    }

    return DefaultRoomClass;
}

void AGridDungeonGenerator::ConfigureRoomWalls(ARoomBase* Room, int32 X, int32 Y)
{
    if (!Room) return;

    bool bHasRightNeighbor = HasRoomAt(X + 1, Y) && !IsWallBlocked(FIntPoint(X, Y), 0);
    bool bHasLeftNeighbor = HasRoomAt(X - 1, Y) && !IsWallBlocked(FIntPoint(X, Y), 2);
    bool bHasUpNeighbor = HasRoomAt(X, Y + 1) && !IsWallBlocked(FIntPoint(X, Y), 1);
    bool bHasDownNeighbor = HasRoomAt(X, Y - 1) && !IsWallBlocked(FIntPoint(X, Y), 3);

    Room->SetNeighbors(bHasUpNeighbor, bHasDownNeighbor, bHasLeftNeighbor, bHasRightNeighbor);
}

// ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========

bool AGridDungeonGenerator::IsValidCell(int32 X, int32 Y) const
{
    return X >= 0 && X < GridWidth && Y >= 0 && Y < GridHeight;
}

bool AGridDungeonGenerator::HasRoomAt(int32 X, int32 Y) const
{
    return IsValidCell(X, Y) && Grid[Y][X].bIsRoom;
}

TArray<FIntPoint> AGridDungeonGenerator::GetAdjacentRooms(int32 X, int32 Y) const
{
    TArray<FIntPoint> Adjacent;

    if (HasRoomAt(X + 1, Y)) Adjacent.Add(FIntPoint(X + 1, Y));
    if (HasRoomAt(X - 1, Y)) Adjacent.Add(FIntPoint(X - 1, Y));
    if (HasRoomAt(X, Y + 1)) Adjacent.Add(FIntPoint(X, Y + 1));
    if (HasRoomAt(X, Y - 1)) Adjacent.Add(FIntPoint(X, Y - 1));

    return Adjacent;
}

TArray<FIntPoint> AGridDungeonGenerator::GetAllRoomPositions() const
{
    TArray<FIntPoint> Positions;

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (Grid[Y][X].bIsRoom)
            {
                Positions.Add(FIntPoint(X, Y));
            }
        }
    }

    return Positions;
}

int32 AGridDungeonGenerator::GetManhattanDistance(const FIntPoint& A, const FIntPoint& B) const
{
    return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

ARoomBase* AGridDungeonGenerator::GetRoomAt(int32 X, int32 Y) const
{
    FVector GeneratorLocation = GetActorLocation();

    for (ARoomBase* Room : SpawnedRooms)
    {
        if (Room && IsValid(Room))
        {
            FVector RelativeLocation = Room->GetActorLocation() - GeneratorLocation;
            int32 RoomX = FMath::RoundToInt(RelativeLocation.X / CellSize + GridWidth / 2.0f);
            int32 RoomY = FMath::RoundToInt(RelativeLocation.Y / CellSize + GridHeight / 2.0f);

            if (RoomX == X && RoomY == Y)
            {
                return Room;
            }
        }
    }

    return nullptr;
}

// ========== ОТЛАДКА ==========

void AGridDungeonGenerator::DebugDrawGrid()
{
    if (!GetWorld()) return;

    float HalfCell = CellSize / 2.0f;
    FVector GeneratorLocation = GetActorLocation();
    FVector CenterOffset = GeneratorLocation - FVector((GridWidth / 2.0f) * CellSize, (GridHeight / 2.0f) * CellSize, 0.0f);

    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (!Grid[Y][X].bIsRoom) continue;

            FVector CellCenter = CenterOffset + FVector(X * CellSize + HalfCell, Y * CellSize + HalfCell, 100.0f);

            FColor Color = FColor::Green;
            ERegionType Type = GetRegionTypeAt(X, Y);
            switch (Type)
            {
            case ERegionType::Corridor: Color = FColor::Yellow; break;
            case ERegionType::SmallRoom: Color = FColor::Orange; break;
            case ERegionType::LargeRoom: Color = FColor::Red; break;
            default: Color = FColor::Green; break;
            }

            DrawDebugBox(GetWorld(), CellCenter, FVector(HalfCell - 10, HalfCell - 10, 50), Color, false, DebugLineDuration);
            DrawDebugString(GetWorld(), CellCenter, FString::Printf(TEXT("%d,%d"), X, Y), nullptr, Color, DebugLineDuration);
        }
    }
}

void AGridDungeonGenerator::DebugPrintRegions()
{
    UE_LOG(LogTemp, Warning, TEXT("=== REGION TYPES ==="));
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        FString Line;
        for (int32 X = 0; X < GridWidth; X++)
        {
            if (!HasRoomAt(X, Y))
            {
                Line.Append(" . ");
                continue;
            }

            ERegionType Type = GetRegionTypeAt(X, Y);
            switch (Type)
            {
            case ERegionType::Corridor: Line.Append(" C "); break;
            case ERegionType::SmallRoom: Line.Append(" S "); break;
            case ERegionType::LargeRoom: Line.Append(" L "); break;
            default: Line.Append(" R "); break;
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Line);
    }
}