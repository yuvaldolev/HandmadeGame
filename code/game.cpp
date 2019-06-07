#include "game.h"

#include "game_log.cpp"
#include "game_tile.cpp"

#include <math.h> // TODO(yuval & eran): Temporary

#define RGB(R, G, B) ((RoundF32ToU32(R * 255.0f) << 16) | \
(RoundF32ToU32(G * 255.0f) << 8) | \
(RoundF32ToU32(B * 255.0f)))

internal loaded_bitmap
DEBUGLoadBMP(thread_context* Thread,
             debug_platform_read_entire_file* ReadEntireFile,
             const char* FileName)
{
    loaded_bitmap Result = { };
    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);
    
    if (ReadResult.ContentsSize != 0)
    {
        bitmap_header* Header = (bitmap_header*)ReadResult.Contents;
        Result.Pixels = (u32*)((u8*)ReadResult.Contents + Header->BitmapOffset);
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        
        Assert(Header->Compression == 3);
        
        u32 RedMask = Header->RedMask;
        u32 GreenMask = Header->GreenMask;
        u32 BlueMask = Header->BlueMask;
        u32 AlphaMask = ~(Header->BlueMask | Header->RedMask | Header->GreenMask);
        
        bit_scan_result RedShift = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenShift = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueShift = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaShift = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedShift.Found);
        Assert(GreenShift.Found);
        Assert(BlueShift.Found);
        Assert(AlphaShift.Found);
        
        u32* SourceDest = Result.Pixels;
        
        for (s32 Y = 0; Y < Header->Height; ++Y)
        {
            for (s32 X = 0; X < Header->Width; ++X)
            {
                u32 C = *SourceDest;
                *SourceDest++ = ((((C >> AlphaShift.Index) & 0xFF) << 24) |
                                 (((C >> RedShift.Index) & 0xFF) << 16) |
                                 (((C >> GreenShift.Index) & 0xFF) << 8) |
                                 (((C >> BlueShift.Index) & 0xFF) << 0));
            }
        }
    }
    
    return Result;
}

internal void
DrawBitmap(game_offscreen_buffer* Buffer, loaded_bitmap* Bitmap,
           f32 RealX, f32 RealY,
           s32 AlignX = 0, s32 AlignY = 0)
{
    RealX -= (f32)AlignX;
    RealY -= (f32)AlignY;
    
    s32 MinX = RoundF32ToS32(RealX);
    s32 MinY = RoundF32ToS32(RealY);
    s32 MaxX = RoundF32ToS32(RealX + (f32)Bitmap->Width);
    s32 MaxY = RoundF32ToS32(RealY + (f32)Bitmap->Height);
    
    s32 SourceOffsetX = 0;
    s32 SourceOffsetY = 0;
    
    if (MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX = 0;
    }
    
    if (MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }
    
    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }
    
    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }
    
    u32* SourceRow = Bitmap->Pixels + Bitmap->Width * (Bitmap->Height - 1);
    SourceRow += -SourceOffsetY * Bitmap->Width + SourceOffsetX;
    
    u8* DestRow = ((u8*)Buffer->Memory + Buffer->BytesPerPixel * MinX +
                   Buffer->Pitch * MinY);
    
    for (s32 Y = MinY; Y < MaxY; ++Y)
    {
        u32* Dest = (u32 *)DestRow;
        u32* Source = SourceRow;
        
        for (s32 X = MinX; X < MaxX; ++X)
        {
            f32 Alpha = (f32)((*Source >> 24) & 0xFF) / 255.0f;
            f32 SourceR = (f32)((*Source >> 16) & 0xFF);
            f32 SourceG = (f32)((*Source >> 8) & 0xFF);
            f32 SourceB = (f32)((*Source >> 0) & 0xFF);
            
            f32 DestR = (f32)((*Dest >> 16) & 0xFF);
            f32 DestG = (f32)((*Dest >> 8) & 0xFF);
            f32 DestB = (f32)((*Dest >> 0) & 0xFF);
            
            f32 R = (1.0f - Alpha) * DestR + Alpha * SourceR;
            f32 G = (1.0f - Alpha) * DestG + Alpha * SourceG;
            f32 B = (1.0f - Alpha) * DestB + Alpha * SourceB;
            
            *Dest = (((u32)(R + 0.5f) << 16) |
                     ((u32)(G + 0.5f) << 8) |
                     ((u32)(B + 0.5f) << 0));
            
            Dest++;
            Source++;
        }
        
        DestRow += Buffer->Pitch;
        SourceRow -= Bitmap->Width;
    }
}

internal void
DrawRectangle(game_offscreen_buffer* Buffer, v2 vMin, v2 vMax, f32 R, f32 G, f32 B)
{
    s32 MinX = RoundF32ToS32(vMin.X);
    s32 MinY = RoundF32ToS32(vMin.Y);
    s32 MaxX = RoundF32ToS32(vMax.X);
    s32 MaxY = RoundF32ToS32(vMax.Y);
    
    if (MinX < 0)
    {
        MinX = 0;
    }
    
    if (MinY < 0)
    {
        MinY = 0;
    }
    
    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }
    
    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }
    
    u32 Color = RGB(R, G, B);
    
    u8* Row = ((u8*)Buffer->Memory +
               MinX * Buffer->BytesPerPixel +
               MinY * Buffer->Pitch);
    
    for (s32 Y = MinY; Y < MaxY; ++Y)
    {
        u32* Pixel = (u32*)Row;
        
        for (s32 X = MinX; X < MaxX; ++X)
        {
            *Pixel++ = Color;
        }
        
        Row += Buffer->Pitch;
    }
}

internal void
GameOutputSound(game_state* GameState, game_sound_output_buffer* Buffer, const s32 ToneHz)
{
    const s16 TONE_VOLUME = 10000;
    const s32 WAVE_PERIOD = Buffer->SamplesPerSecond / ToneHz;
    
    s16* SampleOut = (s16*)Buffer->Samples;
    
    for (u32 SampleIndex = 0;
         SampleIndex < Buffer->SampleCount;
         ++SampleIndex)
    {
#if 0
        f32 SineValue = sinf(GameState->tSine);
        s16 SampleValue = (s16)(SineValue * TONE_VOLUME);
#else
        s16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
#if 0
        GameState->tSine += 2.0f * Pi32 * (1.0f / (f32)WAVE_PERIOD);
        
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

inline entity
GetEntity(game_state* GameState, entity_residence Residence, u32 Index)
{
    entity Entity = { };
    
    if ((Index > 0) && (Index < GameState->EntityCount))
    {
        Entity.Residence = Residence;
        Entity.Dormant = &GameState->DormantEntities[Index];
        Entity.Low = &GameState->LowEntities[Index];
        Entity.High = &GameState->HighEntities[Index];
    }
    
    return Entity;
}

internal u32
AddEntity(game_state* GameState)
{
    u32 EntityIndex = GameState->EntityCount++;
    
    Assert(GameState->EntityCount < ArrayCount(GameState->DormantEntities));
    Assert(GameState->EntityCount < ArrayCount(GameState->LowEntities));
    Assert(GameState->EntityCount < ArrayCount(GameState->HighEntities));
    
    GameState->EntityResidence[EntityIndex] = EntityResidence_Dormant;
    
    GameState->DormantEntities[EntityIndex] = { };
    GameState->LowEntities[EntityIndex] = { };
    GameState->HighEntities[EntityIndex] = { };
    
    return EntityIndex;
}

platform_api Platform;

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Platform = Memory->PlatformAPI;
    
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           ArrayCount(Input->Controllers[0].Buttons));
    
    Assert(sizeof(game_memory) <= Memory->PermanentStorageSize);
    
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    
    if (!Memory->IsInitialized)
    {
        InitializeArena(&GameState->LoggingArena,
                        (u8*)Memory->PermanentStorage + sizeof(game_state),
                        (u64)(Memory->PermanentStorageSize * 0.0001f));
        
        LogInit(&GameState->LoggingArena, LogLevel_Debug, "[%V] [%d] %f:%U:%L: %m%n");
        
        GameState->Backdrop = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                           "../data/test/test_background.bmp");
        
        hero_bitmaps* Bitmaps = GameState->HeroBitmaps;
        
        Bitmaps->Head = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_right_head.bmp");
        Bitmaps->Cape = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_right_cape.bmp");
        Bitmaps->Torso = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                      "../data/test/test_hero_right_torso.bmp");
        Bitmaps->AlignX = 67;
        Bitmaps->AlignY = 182;
        ++Bitmaps;
        
        Bitmaps->Head = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_back_head.bmp");
        Bitmaps->Cape = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_back_cape.bmp");
        Bitmaps->Torso = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                      "../data/test/test_hero_back_torso.bmp");
        Bitmaps->AlignX = 72;
        Bitmaps->AlignY = 182;
        ++Bitmaps;
        
        Bitmaps->Head = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_left_head.bmp");
        Bitmaps->Cape = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_left_cape.bmp");
        Bitmaps->Torso = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                      "../data/test/test_hero_left_torso.bmp");
        Bitmaps->AlignX = 72;
        Bitmaps->AlignY = 182;
        ++Bitmaps;
        
        Bitmaps->Head = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_front_head.bmp");
        Bitmaps->Cape = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                     "../data/test/test_hero_front_cape.bmp");
        Bitmaps->Torso = DEBUGLoadBMP(Thread, Platform.DEBUGReadEntireFile,
                                      "../data/test/test_hero_front_torso.bmp");
        Bitmaps->AlignX = 72;
        Bitmaps->AlignY = 182;
        
        GameState->HeroFacingDirection = 3;
        
        GameState->CameraP.AbsTileX = 17 / 2;
        GameState->CameraP.AbsTileY = 9 / 2;
        
        GameState->PlayerP.AbsTileX = 1;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.Offset.X = 5.0f;
        GameState->PlayerP.Offset.Y = 5.0f;
        
        InitializeArena(&GameState->WorldArena,
                        GameState->LoggingArena.Base + GameState->LoggingArena.Size,
                        Memory->PermanentStorageSize - sizeof(game_state) -
                        GameState->LoggingArena.Size);
        
        GameState->World = PushStruct(&GameState->WorldArena, world);
        world* World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);
        
        tile_map* TileMap = World->TileMap;
        
        // NOTE: This is set to be using 256x256 tile chunks
        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);
        
        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;
        TileMap->TileChunks = PushArray(&GameState->WorldArena, tile_chunk,
                                        TileMap->TileChunkCountX * TileMap->TileChunkCountY *
                                        TileMap->TileChunkCountZ);
        
        TileMap->TileSideInMeters = 1.4f;
        
        u32 RandomNumberIndex = 0;
        
        const u32 TILES_PER_WIDTH = 17;
        const u32 TILES_PER_HEIGHT = 9;
        
        u32 ScreenX = 0;
        u32 ScreenY = 0;
        u32 AbsTileZ = 0;
        
        b32 DoorLeft = false;
        b32 DoorRight = false;
        b32 DoorTop = false;
        b32 DoorBottom = false;
        b32 DoorUp = false;
        b32 DoorDown = false;
        
        for (u32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
        {
            // TODO(yuval, eran): Random number generator
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            u32 RandomChoice;
            
            if (DoorUp || DoorDown)
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            }
            else
            {
                RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
            }
            
            b32 CreatedZDoor = false;
            
            if (RandomChoice == 2)
            {
                CreatedZDoor = true;
                
                if (AbsTileZ == 0)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if (RandomChoice == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }
            
            for (u32 TileY = 0; TileY < TILES_PER_HEIGHT; ++TileY)
            {
                for (u32 TileX = 0; TileX < TILES_PER_WIDTH; ++TileX)
                {
                    u32 AbsTileX = ScreenX * TILES_PER_WIDTH + TileX;
                    u32 AbsTileY = ScreenY * TILES_PER_HEIGHT + TileY;
                    
                    u32 TileValue = 1;
                    
                    if (TileX == 0 && (!DoorLeft || (TileY != (TILES_PER_HEIGHT / 2))))
                    {
                        TileValue = 2;
                    }
                    
                    if ((TileX == (TILES_PER_WIDTH - 1)) &&
                        (!DoorRight || (TileY != (TILES_PER_HEIGHT / 2))))
                    {
                        TileValue = 2;
                    }
                    
                    if (TileY == 0 && (!DoorBottom || (TileX != (TILES_PER_WIDTH / 2))))
                    {
                        TileValue = 2;
                    }
                    
                    if ((TileY == TILES_PER_HEIGHT - 1) &&
                        (!DoorTop || (TileX != (TILES_PER_WIDTH / 2))))
                    {
                        TileValue = 2;
                    }
                    
                    if ((TileX == 10) && (TileY == 6))
                    {
                        if (DoorUp)
                        {
                            TileValue = 3;
                        }
                        else if (DoorDown)
                        {
                            TileValue = 4;
                        }
                    }
                    
                    SetTileValue(&GameState->WorldArena, World->TileMap,
                                 AbsTileX, AbsTileY, AbsTileZ, TileValue);
                }
            }
            
            DoorLeft = DoorRight;
            DoorBottom = DoorTop;
            
            DoorRight = false;
            DoorTop = false;
            
            if (CreatedZDoor)
            {
                DoorUp = !DoorUp;
                DoorDown = !DoorDown;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }
            
            if (RandomChoice == 2)
            {
                if (AbsTileZ == 0)
                {
                    AbsTileZ = 1;
                }
                else
                {
                    AbsTileZ = 0;
                }
            }
            else if (RandomChoice == 1)
            {
                ++ScreenX;
            }
            else
            {
                ++ScreenY;
            }
        }
        
        Memory->IsInitialized = true;
    }
    
    f32 PlayerHeight = 1.4f;
    f32 PlayerWidth = PlayerHeight * 0.75f;
    
    world* World = GameState->World;
    tile_map* TileMap = World->TileMap;
    
    s32 TileSideInPixels = 60;
    f32 MetersToPixels = (f32)TileSideInPixels / TileMap->TileSideInMeters;
    
    f32 LowerLeftX = -(f32)TileSideInPixels / 2.0f;
    f32 LowerLeftY = (f32)OffscreenBuffer->Height;
    
    for (u32 ControllerIndex = 0;
         ControllerIndex < ArrayCount(Input->Controllers);
         ++ControllerIndex)
    {
        game_controller* Controller = &Input->Controllers[ControllerIndex];
        
        if (Controller->IsConnected)
        {
            // NOTE: Delta coordinates are meters per second
            v2 dPlayer = { };
            
            if (Controller->IsAnalog)
            {
                // TODO(yuval, eran): Analog controller tuning
                dPlayer.X = Controller->StickAverageX;
                dPlayer.Y = Controller->StickAverageY;
            }
            else
            {
                // TODO(yuval, eran): Digital controller tuning
                if (Controller->MoveUp.EndedDown)
                {
                    dPlayer.Y = 1.0f;
                    GameState->HeroFacingDirection = 1;
                }
                
                if (Controller->MoveDown.EndedDown)
                {
                    dPlayer.Y = -1.0f;
                    GameState->HeroFacingDirection = 3;
                }
                
                if (Controller->MoveLeft.EndedDown)
                {
                    dPlayer.X = -1.0f;
                    GameState->HeroFacingDirection = 2;
                }
                
                if (Controller->MoveRight.EndedDown)
                {
                    dPlayer.X = 1.0f;
                    GameState->HeroFacingDirection = 0;
                }
            }
            
            f32 PlayerSpeed = 2.0f;
            
            if (Controller->Run.EndedDown)
            {
                PlayerSpeed = 10.f;
            }
            
            dPlayer *= PlayerSpeed;
            
            if ((dPlayer.X != 0.0f) && (dPlayer.Y != 0.0f))
            {
                dPlayer *= 0.7071067812f;
            }
            
            tile_map_position NewPlayerP = GameState->PlayerP;
            NewPlayerP.Offset += dPlayer * Input->dtForFrame;
            NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
            
            tile_map_position PlayerLeft = NewPlayerP;
            PlayerLeft.Offset.X -= 0.5f * PlayerWidth;
            PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);
            
            tile_map_position PlayerRight = NewPlayerP;
            PlayerRight.Offset.X += 0.5f * PlayerWidth;
            PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
            
            if (IsTileMapPointEmpty(TileMap, NewPlayerP) &&
                IsTileMapPointEmpty(TileMap, PlayerLeft) &&
                IsTileMapPointEmpty(TileMap, PlayerRight))
            {
                if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP))
                {
                    u32 NewTileValue = GetTileValue(TileMap, NewPlayerP);
                    
                    if (NewTileValue == 3)
                    {
                        ++NewPlayerP.AbsTileZ;
                    }
                    else if (NewTileValue == 4)
                    {
                        -- NewPlayerP.AbsTileZ;
                    }
                }
                
                GameState->PlayerP = NewPlayerP;
            }
            
            GameState->CameraP.AbsTileZ = GameState->PlayerP.AbsTileZ;
            
            tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);
            
            if (Diff.dXY.X > (9.0f * TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileX += 17;
            }
            else if (Diff.dXY.X < -(9.0f * TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileX -= 17;
            }
            
            if (Diff.dXY.Y > (5.0f * TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileY += 9;
            }
            else if (Diff.dXY.Y < -(5.0f * TileMap->TileSideInMeters))
            {
                GameState->CameraP.AbsTileY -= 9;
            }
        }
    }
    
    DrawBitmap(OffscreenBuffer, &GameState->Backdrop, 0, 0);
    
    f32 ScreenCenterX = (f32)OffscreenBuffer->Width / 2.0f;
    f32 ScreenCenterY  = (f32)OffscreenBuffer->Height / 2.0f;
    
    for (s32 RelRow = -10; RelRow < 10; ++RelRow)
    {
        for (s32 RelColumn = -20; RelColumn < 20; ++RelColumn)
        {
            u32 Column = GameState->CameraP.AbsTileX + RelColumn;
            u32 Row =  GameState->CameraP.AbsTileY + RelRow;
            u32 TileID = GetTileValue(TileMap, Column, Row, GameState->CameraP.AbsTileZ);
            
            if (TileID > 1)
            {
                f32 TileColor = 0.5f;
                
                if (TileID == 2)
                {
                    TileColor = 1.0f;
                }
                
                if (TileID > 2)
                {
                    TileColor = 0.25f;
                }
                
                if ((Column == GameState->CameraP.AbsTileX) &&
                    (Row == GameState->CameraP.AbsTileY))
                {
                    TileColor = 0.0f;
                }
                
                v2 TileSide = { 0.5f * TileSideInPixels, 0.5f * TileSideInPixels };
                v2 Cen = { ScreenCenterX - GameState->CameraP.Offset.X * MetersToPixels +
                        (f32)(RelColumn * TileSideInPixels),
                    ScreenCenterY + GameState->CameraP.Offset.Y * MetersToPixels -
                        (f32)(RelRow * TileSideInPixels) };
                
                v2 Min = Cen - TileSide;
                v2 Max = Cen + TileSide;
                
                DrawRectangle(OffscreenBuffer, Min, Max, TileColor, TileColor, TileColor);
            }
        }
    }
    
    f32 PlayerR = 1.0f;
    f32 PlayerG = 1.0f;
    f32 PlayerB = 0.0f;
    
    tile_map_difference Diff = Subtract(TileMap, &GameState->PlayerP, &GameState->CameraP);
    
    f32 PlayerGroundPointX = ScreenCenterX + Diff.dXY.X * MetersToPixels;
    f32 PlayerGroundPointY = ScreenCenterY - Diff.dXY.Y * MetersToPixels;
    v2 PlayerLeftTop = { PlayerGroundPointX - 0.5f * PlayerWidth * MetersToPixels,
        PlayerGroundPointY - PlayerHeight * MetersToPixels };
    v2 PlayerWidthHeight = { PlayerWidth, PlayerHeight};
    
    DrawRectangle(OffscreenBuffer,
                  PlayerLeftTop,
                  PlayerLeftTop + MetersToPixels * PlayerWidthHeight,
                  PlayerR, PlayerG, PlayerB);
    
    hero_bitmaps* HeroBitmaps = &GameState->HeroBitmaps[GameState->HeroFacingDirection];
    
    DrawBitmap(OffscreenBuffer, &HeroBitmaps->Torso,
               PlayerGroundPointX, PlayerGroundPointY,
               HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBitmap(OffscreenBuffer, &HeroBitmaps->Cape,
               PlayerGroundPointX, PlayerGroundPointY,
               HeroBitmaps->AlignX, HeroBitmaps->AlignY);
    DrawBitmap(OffscreenBuffer, &HeroBitmaps->Head,
               PlayerGroundPointX, PlayerGroundPointY,
               HeroBitmaps->AlignX, HeroBitmaps->AlignY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state* GameState = (game_state*)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 1);
}
