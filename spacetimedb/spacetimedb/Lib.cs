using SpacetimeDB;

/// <summary>
/// Orakai / CubusCore authoritative world state.
///
/// The client keeps a disposable local cache (see FCubusChunkStore) and
/// deterministically regenerates terrain from the world seed. SpacetimeDB is
/// authoritative only for the things that cannot be regenerated: where players
/// are, and the deltas players have applied on top of generated terrain and
/// foliage.
/// </summary>
public static partial class Module
{
    // -------------------------------------------------------------------------
    // World metadata (singleton, Id == 0)
    // -------------------------------------------------------------------------
    // Mirrors FCubusChunkStoreContext so edits can be invalidated when the
    // seed or generation version changes.
    [SpacetimeDB.Table(Accessor = "world_config", Public = true)]
    public partial struct WorldConfig
    {
        [SpacetimeDB.PrimaryKey]
        public int Id;

        public long WorldSeed;
        public uint GenerationVersion;
    }

    // -------------------------------------------------------------------------
    // Player coordinates
    // -------------------------------------------------------------------------
    [SpacetimeDB.Table(Accessor = "player_position", Public = true)]
    public partial struct PlayerPosition
    {
        [SpacetimeDB.PrimaryKey]
        public Identity Player;

        public double X;
        public double Y;
        public double Z;
        public float Yaw;
        public float Pitch;

        public Timestamp UpdatedAt;
    }

    // Connection presence, driven by the client_connected / client_disconnected
    // lifecycle reducers.
    [SpacetimeDB.Table(Accessor = "player_session", Public = true)]
    public partial struct PlayerSession
    {
        [SpacetimeDB.PrimaryKey]
        public Identity Player;

        public bool Online;
        public Timestamp LastSeen;
    }

    // -------------------------------------------------------------------------
    // Edited chunks (per-voxel deltas)
    // -------------------------------------------------------------------------
    [SpacetimeDB.Table(Accessor = "voxel_edit", Public = true)]
    public partial struct VoxelEdit
    {
        // "cx:cy:cz:lx:ly:lz" - deterministic identity for upsert semantics.
        [SpacetimeDB.PrimaryKey]
        public string Key;

        // Packed chunk coordinate so a client can load every delta for a chunk
        // in a single indexed query.
        [SpacetimeDB.Index.BTree]
        public long ChunkKey;

        public int ChunkX;
        public int ChunkY;
        public int ChunkZ;
        public int LocalX;
        public int LocalY;
        public int LocalZ;

        public int MaterialId;
        public bool IsWater;

        public Identity EditedBy;
        public Timestamp EditedAt;
    }

    // -------------------------------------------------------------------------
    // Edited foliage (per-instance deltas)
    // -------------------------------------------------------------------------
    [SpacetimeDB.Table(Accessor = "foliage_edit", Public = true)]
    public partial struct FoliageEdit
    {
        // "wx:wy:wz" world voxel of the foliage instance.
        [SpacetimeDB.PrimaryKey]
        public string Key;

        // Packed owning chunk coordinate for chunk-scoped queries.
        [SpacetimeDB.Index.BTree]
        public long ChunkKey;

        public int WorldX;
        public int WorldY;
        public int WorldZ;

        // Removed == true means the player deleted foliage the generator would
        // otherwise place here. Otherwise the fields below describe the instance
        // the player placed or modified.
        public bool Removed;
        public int TypeId;
        public float Yaw;
        public float Scale;

        public Identity EditedBy;
        public Timestamp EditedAt;
    }

    // -------------------------------------------------------------------------
    // Key / coordinate helpers
    // -------------------------------------------------------------------------
    private const int ChunkSize = 32;

    // Packs a chunk coordinate (each component clamped to +-2^20) into a single
    // long. Matches PackChunkKey on the Unreal client.
    private static long PackChunkKey(int cx, int cy, int cz)
    {
        long ux = cx & 0x1FFFFF;
        long uy = cy & 0x1FFFFF;
        long uz = cz & 0x1FFFFF;
        return (ux << 42) | (uy << 21) | uz;
    }

    // Floor division so negative world coordinates map to the correct chunk.
    private static int FloorDiv(int value, int divisor)
    {
        int q = value / divisor;
        if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
        {
            q--;
        }
        return q;
    }

    // -------------------------------------------------------------------------
    // Lifecycle reducers
    // -------------------------------------------------------------------------
    [SpacetimeDB.Reducer(ReducerKind.Init)]
    public static void Init(ReducerContext ctx)
    {
        if (ctx.Db.world_config.Id.Find(0) is null)
        {
            ctx.Db.world_config.Insert(new WorldConfig
            {
                Id = 0,
                WorldSeed = 1,
                GenerationVersion = 1,
            });
        }
        Log.Info("Orakai module initialized.");
    }

    [SpacetimeDB.Reducer(ReducerKind.ClientConnected)]
    public static void ClientConnected(ReducerContext ctx)
    {
        var session = new PlayerSession
        {
            Player = ctx.Sender,
            Online = true,
            LastSeen = ctx.Timestamp,
        };

        if (ctx.Db.player_session.Player.Find(ctx.Sender) is null)
        {
            ctx.Db.player_session.Insert(session);
        }
        else
        {
            ctx.Db.player_session.Player.Update(session);
        }

        Log.Info($"{ctx.Sender} connected.");
    }

    [SpacetimeDB.Reducer(ReducerKind.ClientDisconnected)]
    public static void ClientDisconnected(ReducerContext ctx)
    {
        if (ctx.Db.player_session.Player.Find(ctx.Sender) is PlayerSession session)
        {
            session.Online = false;
            session.LastSeen = ctx.Timestamp;
            ctx.Db.player_session.Player.Update(session);
        }

        Log.Info($"{ctx.Sender} disconnected.");
    }

    // -------------------------------------------------------------------------
    // World config
    // -------------------------------------------------------------------------
    [SpacetimeDB.Reducer]
    public static void SetWorldConfig(ReducerContext ctx, long worldSeed, uint generationVersion)
    {
        var config = new WorldConfig
        {
            Id = 0,
            WorldSeed = worldSeed,
            GenerationVersion = generationVersion,
        };

        if (ctx.Db.world_config.Id.Find(0) is null)
        {
            ctx.Db.world_config.Insert(config);
        }
        else
        {
            ctx.Db.world_config.Id.Update(config);
        }
    }

    // -------------------------------------------------------------------------
    // Player coordinates
    // -------------------------------------------------------------------------
    [SpacetimeDB.Reducer]
    public static void UpdatePlayerPosition(
        ReducerContext ctx,
        double x,
        double y,
        double z,
        float yaw,
        float pitch)
    {
        var row = new PlayerPosition
        {
            Player = ctx.Sender,
            X = x,
            Y = y,
            Z = z,
            Yaw = yaw,
            Pitch = pitch,
            UpdatedAt = ctx.Timestamp,
        };

        if (ctx.Db.player_position.Player.Find(ctx.Sender) is null)
        {
            ctx.Db.player_position.Insert(row);
        }
        else
        {
            ctx.Db.player_position.Player.Update(row);
        }
    }

    // -------------------------------------------------------------------------
    // Voxel / chunk edits
    // -------------------------------------------------------------------------
    [SpacetimeDB.Reducer]
    public static void ApplyVoxelEdit(
        ReducerContext ctx,
        int chunkX,
        int chunkY,
        int chunkZ,
        int localX,
        int localY,
        int localZ,
        int materialId,
        bool isWater)
    {
        string key = $"{chunkX}:{chunkY}:{chunkZ}:{localX}:{localY}:{localZ}";

        var row = new VoxelEdit
        {
            Key = key,
            ChunkKey = PackChunkKey(chunkX, chunkY, chunkZ),
            ChunkX = chunkX,
            ChunkY = chunkY,
            ChunkZ = chunkZ,
            LocalX = localX,
            LocalY = localY,
            LocalZ = localZ,
            MaterialId = materialId,
            IsWater = isWater,
            EditedBy = ctx.Sender,
            EditedAt = ctx.Timestamp,
        };

        if (ctx.Db.voxel_edit.Key.Find(key) is null)
        {
            ctx.Db.voxel_edit.Insert(row);
        }
        else
        {
            ctx.Db.voxel_edit.Key.Update(row);
        }
    }

    // Removes a voxel delta, restoring the deterministically generated voxel.
    [SpacetimeDB.Reducer]
    public static void ClearVoxelEdit(
        ReducerContext ctx,
        int chunkX,
        int chunkY,
        int chunkZ,
        int localX,
        int localY,
        int localZ)
    {
        string key = $"{chunkX}:{chunkY}:{chunkZ}:{localX}:{localY}:{localZ}";
        if (ctx.Db.voxel_edit.Key.Find(key) is not null)
        {
            ctx.Db.voxel_edit.Key.Delete(key);
        }
    }

    // -------------------------------------------------------------------------
    // Foliage edits
    // -------------------------------------------------------------------------
    [SpacetimeDB.Reducer]
    public static void ApplyFoliageEdit(
        ReducerContext ctx,
        int worldX,
        int worldY,
        int worldZ,
        bool removed,
        int typeId,
        float yaw,
        float scale)
    {
        string key = $"{worldX}:{worldY}:{worldZ}";
        int chunkX = FloorDiv(worldX, ChunkSize);
        int chunkY = FloorDiv(worldY, ChunkSize);
        int chunkZ = FloorDiv(worldZ, ChunkSize);

        var row = new FoliageEdit
        {
            Key = key,
            ChunkKey = PackChunkKey(chunkX, chunkY, chunkZ),
            WorldX = worldX,
            WorldY = worldY,
            WorldZ = worldZ,
            Removed = removed,
            TypeId = typeId,
            Yaw = yaw,
            Scale = scale,
            EditedBy = ctx.Sender,
            EditedAt = ctx.Timestamp,
        };

        if (ctx.Db.foliage_edit.Key.Find(key) is null)
        {
            ctx.Db.foliage_edit.Insert(row);
        }
        else
        {
            ctx.Db.foliage_edit.Key.Update(row);
        }
    }

    [SpacetimeDB.Reducer]
    public static void ClearFoliageEdit(
        ReducerContext ctx,
        int worldX,
        int worldY,
        int worldZ)
    {
        string key = $"{worldX}:{worldY}:{worldZ}";
        if (ctx.Db.foliage_edit.Key.Find(key) is not null)
        {
            ctx.Db.foliage_edit.Key.Delete(key);
        }
    }
}
