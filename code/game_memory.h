#if !defined(GAME_MEMORY_H)

#define PushStruct(arena, type) (type*)PushSize_(arena, sizeof(type))
#define PushArray(arena, type, count) (type*)PushSize_(arena, (count) * sizeof(type))
#define PushSize(arena, size) PushSize_(arena, size)

struct MemoryArena
{
    u8* base;
    memory_index size;
    memory_index used;
};

internal void
InitializeArena(MemoryArena* arena, void* base, memory_index size)
{
    arena->base = (u8*)base;
    arena->size = size;
    arena->used = 0;
}

internal void*
PushSize_(MemoryArena* arena, memory_index size)
{
    Assert((arena->used + size) <= arena->size);
    void* result = arena->base + arena->used;
    arena->used += size;
    return result;
}

#define GAME_MEMORY_H
#endif