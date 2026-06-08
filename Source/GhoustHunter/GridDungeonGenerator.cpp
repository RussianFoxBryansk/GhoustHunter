#include "GridDungeonGenerator.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

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

// ========== ОСНОВНАЯ ГЕНЕРАЦИЯ ==========

void AGridDungeonGenerator::GenerateDungeon()
{
    ClearDungeon();

    // Установка сида
    if (bUseRandomSeed || Seed == 0)
    {
        CurrentSeed = FMath::Rand();
    }
    else
    {
        CurrentSeed = Seed;
    }
    FMath::RandInit(CurrentSeed);

    UE_LOG(LogTemp, Log, TEXT("Generating dungeon with seed: %d, grid: %dx%d at location: %s"),
        CurrentSeed, GridWidth, GridHeight, *GetActorLocation().ToString());

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

    SpawnRooms();

    if (bDrawDebugGrid)
    {
        DebugDrawGrid();
    }

    UE_LOG(LogTemp, Log, TEXT("Final rooms count: %d"), SpawnedRooms.Num());
}

void AGridDungeonGenerator::CreateMinimalDungeon()
{
    // Принудительно создаем простой крест из комнат
    Grid.SetNum(GridHeight);
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        Grid[Y].SetNum(GridWidth);
        for (int32 X = 0; X < GridWidth; X++)
        {
            Grid[Y][X].bIsRoom = false;
        }
    }

    int32 CenterX = GridWidth / 2;
    int32 CenterY = GridHeight / 2;

    // Создаем крест
    for (int32 I = -3; I <= 3; I++)
    {
        if (IsValidCell(CenterX + I, CenterY))
            Grid[CenterY][CenterX + I].bIsRoom = true;
        if (IsValidCell(CenterX, CenterY + I))
            Grid[CenterY + I][CenterX].bIsRoom = true;
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
            if (X == 0 || X == GridWidth - 1 || Y == 0 || Y == GridHeight - 1)
            {
                Grid[Y][X].bIsRoom = false;
            }
            else
            {
                // Центр всегда комната для связности
                if (X == GridWidth / 2 && Y == GridHeight / 2)
                {
                    Grid[Y][X].bIsRoom = true;
                }
                else
                {
                    Grid[Y][X].bIsRoom = FMath::RandRange(0, 100) < InitFillPercent;
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
                // Комната умирает только если нет соседей
                if (Neighbors < 2)
                {
                    NewGrid[Y][X].bIsRoom = false;
                }
            }
            else
            {
                // Рождение комнаты если есть 2-3 соседа
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
    }
}

// ========== СВЯЗНОСТЬ ==========

void AGridDungeonGenerator::RemoveIsolatedRooms()
{
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
        }
    }

    while (Y != End.Y)
    {
        Y += StepY;
        if (IsValidCell(X, Y))
        {
            Grid[Y][X].bIsRoom = true;
        }
    }
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

    bool bHasRightNeighbor = HasRoomAt(X + 1, Y);
    bool bHasLeftNeighbor = HasRoomAt(X - 1, Y);
    bool bHasDownNeighbor = HasRoomAt(X, Y - 1);
    bool bHasUpNeighbor = HasRoomAt(X, Y + 1);

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
            FVector CellCenter = CenterOffset + FVector(X * CellSize + HalfCell, Y * CellSize + HalfCell, 100.0f);
            FColor Color = Grid[Y][X].bIsRoom ? FColor::Green : FColor::Red;

            DrawDebugBox(GetWorld(), CellCenter, FVector(HalfCell - 10, HalfCell - 10, 50), Color, false, DebugLineDuration);
            DrawDebugString(GetWorld(), CellCenter, FString::Printf(TEXT("%d,%d"), X, Y), nullptr, Color, DebugLineDuration);
        }
    }
}