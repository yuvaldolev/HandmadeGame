#if !defined(GAME_MEMORY_H)

#define PushStruct(Arena, Type) (type*)PushSize_(Arena, sizeof(Type))
#define PushArray(Arena, Type, Count) (type*)PushSize_(Arena, (Count) * sizeof(Type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)

struct memory_arena
{
    u8* Base;
    memory_index Size;
    memory_index Used;
};

internal void
InitializeArena(memory_arena* Arena, void* Base, memory_index Size)
{
    Arena->Base = (u8*)Base;
    Arena->Size = Size;
    Arena->Used = 0;
}

internal void*
PushSize_(memory_arena* Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void* Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

#define GAME_MEMORY_H
#endif