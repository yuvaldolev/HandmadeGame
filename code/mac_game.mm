#include <Cocoa/Cocoa.h>

#include "game_platform.h"
#include "game_shared.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#include <AudioToolbox/AudioToolbox.h>
#include <IOKit/hid/IOHIDLib.h>
#include <Carbon/Carbon.h>

#include <dlfcn.h>
#include <libproc.h>
#include <sys/stat.h>
#include <stdio.h> // TODO(yuval): Remove this temporary include

#include <mach/mach_time.h>

#include "mac_game.h"

platform_api Platform;

global_variable f32 GlobalAspectRatio;
global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable mac_offscreen_buffer GlobalBackbuffer;
global_variable NSOpenGLContext* GlobalGLContext;
global_variable mach_timebase_info_data_t GlobalTimebaseInfo;
global_variable game_controller GlobalGamepadController;

@interface mac_app_delegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end

@implementation mac_app_delegate
- (void)applicationDidFinishLaunching:(id)Sender
{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)Sender
{
	return YES;
}

- (void)applicationWillTerminate:(NSNotification *)Notification
{
	printf("applicationWillTerminate\n");
}

- (void)windowWillClose:(id)Sender
{
    GlobalRunning = false;
}

- (NSSize)windowWillResize:(NSWindow*)Window toSize:(NSSize)FrameSize
{
    // NOTE: Maintaining the proper aspect ratio
    FrameSize.height = (f32)FrameSize.width / GlobalAspectRatio;
    return FrameSize;
}

- (void)windowDidResize:(NSNotification*)Notification
{
	NSWindow* Window = [Notification object];
	NSRect Frame = [Window frame];
    
	[GlobalGLContext update];
    
	// OpenGL reshape. Typically done in the view
	glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    glViewport(0, 0, (GLsizei)Frame.size.width, (GLsizei)Frame.size.height);
    
    printf("Window Resized!\n");
}
@end

PLATFORM_DISPLAY_MESSAGE_BOX(PlatformDisplayMessageBox)
{
    NSAlert *Alert = [[[NSAlert alloc] init] autorelease];
    
    NSString* TitleString = [NSString stringWithUTF8String:Title];
    NSString* MessageString = [NSString stringWithUTF8String:Message];
    [Alert setMessageText:TitleString];
    [Alert setInformativeText:MessageString];
    
    [Alert runModal];
}

PLATFORM_GET_DATE_TIME(PlatformGetDateTime)
{
    NSCalendar* Gregorian = [[NSCalendar alloc]
            initWithCalendarIdentifier:NSGregorianCalendar];
    
    NSDate* Now = [NSDate date];
    u32 UnitFlags = (NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay |
                     NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond |
                     NSCalendarUnitNanosecond);
    
    NSDateComponents* DateComponents = [Gregorian components:UnitFlags fromDate:Now];
    
    platform_date_time Result;
    Result.Day = (u16)[DateComponents day];
    Result.Month = (u16)[DateComponents month];
    Result.Year = (u16)[DateComponents year];
    Result.Hour = (u16)[DateComponents hour];
    Result.Minute = (u16)[DateComponents minute];
    Result.Second = (u16)[DateComponents second];
    Result.Milliseconds = (u16)(1.0E-6 * [DateComponents nanosecond]);
    
    return Result;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (Memory)
    {
        free(Memory);
    }
}

// TODO(yuval & eran): Renable Logging in DEBUG read & write
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = { };
    
    int FileHandle = open(FileName, O_RDONLY);
    
    if (FileHandle != -1)
    {
        struct stat FileStat;
        
        if (fstat(FileHandle, &FileStat) == 0)
        {
            u32 FileSize32 = SafeTruncateToU32(FileStat.st_size);
            
            // TODO(yuval, eran): Allocate with platform functions instread of malloc
            Result.Contents = malloc(FileSize32);
            
            if (Result.Contents)
            {
                memory_index BytesRead = read(FileHandle, Result.Contents, FileSize32);
                
                if (BytesRead == FileSize32)
                {
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // TODO(yuval, eran): Diagnostic
            }
        }
        else
        {
            // TODO(yuval, eran): Diagnostic
        }
        
        close(FileHandle);
    }
    else
    {
        // TODO(yuval, eran): Diagnostic
    }
    
    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 Result = false;
    
    int FileHandle = open(FileName, O_WRONLY | O_CREAT, 0644);
    
    if (FileHandle != -1)
    {
        memory_index BytesWritten = write(FileHandle, Memory, MemorySize);
        Result = (BytesWritten == MemorySize);
        
        close(FileHandle);
    }
    else
    {
        // TODO(yuval, eran): Diagnostic
    }
    
    return Result;
}

internal void
MacBuildAppPathFileName(char* Dest, memory_index DestCount,
                        mac_state* State, const char* FileName)
{
    CatStrings(Dest, DestCount,
               State->AppFileName,
               State->OnePastLastAppFileNameSlash - State->AppFileName,
               FileName, StringLength(FileName));
}

internal void
MacGetInputFileLocation(char* Dest, memory_index DestCount,
                        b32 IsInputStream, mac_state* State,
                        s32 SlotIndex)
{
    char Temp[64];
    FormatString(Temp, sizeof(Temp), "loop_edit_%d_%s.gi",
                 SlotIndex, IsInputStream ? "input" : "state");
    
    MacBuildAppPathFileName(Dest, DestCount, State, Temp);
}

internal mac_replay_buffer*
MacGetReplayBuffer(mac_state* State, u32 Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    mac_replay_buffer* Result = &State->ReplayBuffers[Index];
    return Result;
}

internal void
MacBeginRecordingInput(mac_state* State, s32 InputRecordingIndex)
{
    mac_replay_buffer* ReplayBuffer = MacGetReplayBuffer(State, InputRecordingIndex - 1);
    
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;
        
        char FileName[MAC_STATE_FILE_NAME_COUNT];
        MacGetInputFileLocation(FileName, sizeof(FileName),
                                true, State, InputRecordingIndex);
        
        State->RecordingHandle = open(FileName,
                                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        
        // @Replace memcpy with Copy
        memcpy(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void
MacEndRecordingInput(mac_state* State)
{
    close(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
MacBeginInputPlayBack(mac_state* State, s32 InputPlayingIndex)
{
    mac_replay_buffer* ReplayBuffer = MacGetReplayBuffer(State, InputPlayingIndex - 1);
    
    if (ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;
        
        char FileName[MAC_STATE_FILE_NAME_COUNT];
        MacGetInputFileLocation(FileName, sizeof(FileName),
                                true, State, InputPlayingIndex);
        
        State->PlayBackHandle = open(FileName, O_RDONLY);
        
        // @Replace memcpy with Copy
        memcpy(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void
MacEndInputPlayBack(mac_state* State)
{
    close(State->PlayBackHandle);
    State->InputPlayingIndex = 0;
}

internal void
MacRecordInput(mac_state* State, game_input* NewInput)
{
    memory_index BytesWritten = write(State->RecordingHandle, NewInput, sizeof(*NewInput));
    // TODO(yuval): Check if bytes written is different then the size of NewInput
}

internal void
MacPlayBackInput(mac_state* State, game_input* NewInput)
{
    memory_index BytesRead = read(State->PlayBackHandle, NewInput, sizeof(*NewInput));
    
    if (!BytesRead)
    {
        s32 PlayingIndex = State->InputPlayingIndex;
        MacEndInputPlayBack(State);
        MacBeginInputPlayBack(State, PlayingIndex);
    }
}

internal f32
MacGetSecondsElapsed(u64 Start, u64 End)
{
    // NOTE(yuval): Elapsed nanoseconds calculation
    f32 Result = ((f32)(End - Start) *
                  ((f32)GlobalTimebaseInfo.numer) /
                  ((f32)GlobalTimebaseInfo.denom));
    
    // NOTE(yuval): Conversion to seconds
    Result *= 1.0E-9;
    
    return Result;
}

internal void
MacFillSoundBuffer(mac_sound_output* SoundOutput)
{
    s16* SourceSample = SoundOutput->SoundBuffer.Samples;
    
    for (u32 SampleIndex = 0;
         SampleIndex < SoundOutput->SoundBuffer.SampleCount;
         ++SampleIndex)
    {
        *SoundOutput->WriteCursor++ = *SourceSample++;
        *SoundOutput->WriteCursor++ = *SourceSample++;
        
        if (((u8*)SoundOutput->WriteCursor) >=
            ((u8*)SoundOutput->CoreAudioBuffer + SoundOutput->SoundBufferSize))
        {
            SoundOutput->WriteCursor = SoundOutput->CoreAudioBuffer;
        }
    }
}

OSStatus
MacAudioUnitCallback(void* InRefCon,
                     AudioUnitRenderActionFlags* IOActionFlags,
                     const AudioTimeStamp* InTimeStamp,
                     UInt32 InBusNumber,
                     UInt32 InNumberFrames,
                     AudioBufferList* IOData)
{
    mac_sound_output* SoundOutput = (mac_sound_output*)InRefCon;
    
    if (SoundOutput->ReadCursor == SoundOutput->WriteCursor)
    {
        SoundOutput->SoundBuffer.SampleCount = 0;
    }
    
    u32 SampleCount = InNumberFrames;
    if (SoundOutput->SoundBuffer.SampleCount < SampleCount)
    {
        SampleCount = SoundOutput->SoundBuffer.SampleCount;
    }
    
    s16* OutputBufferL = (s16*)IOData->mBuffers[0].mData;
    s16* OutputBufferR = (s16*)IOData->mBuffers[1].mData;
    
    for (u32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
    {
        OutputBufferL[SampleIndex] = *SoundOutput->ReadCursor++;
        OutputBufferR[SampleIndex] = *SoundOutput->ReadCursor++;
        
        if ((u8*)SoundOutput->ReadCursor >=
            (((u8*)SoundOutput->CoreAudioBuffer) + SoundOutput->SoundBufferSize))
        {
            SoundOutput->ReadCursor = SoundOutput->CoreAudioBuffer;
        }
    }
    
    for (u32 SampleIndex = SampleCount; SampleIndex < InNumberFrames; ++SampleIndex)
    {
        OutputBufferL[SampleIndex] = 0;
        OutputBufferR[SampleIndex] = 0;
    }
    
    return noErr;
}

internal void
MacInitCoreAudio(mac_sound_output* SoundOutput)
{
    AudioComponentDescription ComponentDescription = { };
    ComponentDescription.componentType = kAudioUnitType_Output;
    ComponentDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
    ComponentDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent OutputComponent = AudioComponentFindNext(0, &ComponentDescription);
    
    AudioUnit AudioUnit;
    AudioComponentInstanceNew(OutputComponent, &AudioUnit);
    AudioUnitInitialize(AudioUnit);
    
    AudioStreamBasicDescription AudioDescriptor;
    AudioDescriptor.mSampleRate = SoundOutput->SoundBuffer.SamplesPerSecond;
    AudioDescriptor.mFormatID = kAudioFormatLinearPCM;
    AudioDescriptor.mFormatFlags = (kAudioFormatFlagIsSignedInteger |
                                    kAudioFormatFlagIsNonInterleaved |
                                    kAudioFormatFlagIsPacked);
    AudioDescriptor.mFramesPerPacket = 1;
    AudioDescriptor.mChannelsPerFrame = 2;
    AudioDescriptor.mBitsPerChannel = sizeof(s16) * 8;
    AudioDescriptor.mBytesPerFrame = sizeof(s16);
    AudioDescriptor.mBytesPerPacket = AudioDescriptor.mFramesPerPacket *
        AudioDescriptor.mBytesPerFrame;
    
    AudioUnitSetProperty(AudioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input,
                         0, &AudioDescriptor,
                         sizeof(AudioDescriptor));
    
    AURenderCallbackStruct RenderCallback;
    RenderCallback.inputProc = MacAudioUnitCallback;
    RenderCallback.inputProcRefCon = SoundOutput;
    
    AudioUnitSetProperty(AudioUnit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Global,
                         0, &RenderCallback,
                         sizeof(RenderCallback));
    
    AudioOutputUnitStart(AudioUnit);
}

internal void
MacProcessKeyboardMessage(game_button_state* NewState, b32 IsDown)
{
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal f32
MacNormalizeHIDStick(long StickValue, long MinValue, long MaxValue)
{
    const f32 DEAD_ZONE = 10.0f;
    
    f32 Result = 0;
    f32 ZeroValue = MinValue + MaxValue / 2;
    
    if (MinValue != MaxValue &&
        ((StickValue > ZeroValue + DEAD_ZONE) ||
         (StickValue < ZeroValue - DEAD_ZONE)))
    {
        Result = 2.0f * ((f32)(StickValue - MinValue) / (MaxValue - MinValue)) - 1;
    }
    
    return Result;
}


internal void
MacProcessHIDDigitalButton(game_button_state* NewState, b32 IsDown)
{
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void
MacHIDAction(void* Context, IOReturn Result,
             void* Sender, IOHIDValueRef Value)
{
    if (IOHIDValueGetLength(Value) <= 2)
    {
        IOHIDElementRef ElementRef = IOHIDValueGetElement(Value);
        
        IOHIDElementCookie Cookie = IOHIDElementGetCookie(ElementRef);
        u32 UsagePage = IOHIDElementGetUsagePage(ElementRef);
        u32 Usage = IOHIDElementGetUsage(ElementRef);
        CFIndex LogicalMin = IOHIDElementGetLogicalMin(ElementRef);
        CFIndex LogicalMax = IOHIDElementGetLogicalMax(ElementRef);
        
        CFIndex ElementValue = IOHIDValueGetIntegerValue(Value);
        
        // NOTE(yuval): Usage Pages
        //   1 - Generic Desktop (mouse, joystick)
        //   2 - Simulation Controls
        //   3 - VR Controls
        //   4 - Sports Controls
        //   5 - Game Controls
        //   6 - Generic Device Controls (battery, wireless, security code)
        //   7 - Keyboard/Keypad
        //   8 - LED
        //   9 - Button
        //   A - Ordinal
        //   B - Telephony
        //   C - Consumer
        //   D - Digitizers
        //  10 - Unicode
        //  14 - Alphanumeric Display
        //  40 - Medical Instrument
        
#if 0
        printf("HID Event: Cookie: %ld  Page/Usage = %u/%u  Min/Max = %ld/%ld  Value = %ld\n",
               (long)Cookie, UsagePage, Usage, LogicalMin, LogicalMax, ElementValue);
#endif
        
        // TODO(yuval): Handle Multiple Controllers
        if (UsagePage == 1)
        {
            switch (Usage)
            {
                // TODO(yuval): Maybe add fake digital button processing for the stick X/Y
                case 0x30: // NOTE: Stick X
                {
                    GlobalGamepadController.StickAverageX =
                        MacNormalizeHIDStick(ElementValue, LogicalMin, LogicalMax);
                } break;
                
                case 0x31: // NOTE: Stick Y
                {
                    GlobalGamepadController.StickAverageY =
                        -MacNormalizeHIDStick(ElementValue, LogicalMin, LogicalMax);
                } break;
                
                case 0x39: // NOTE: DPAD
                {
                    // TODO(yuval): Maybe add fake stick movement to the dpad
                    MacProcessHIDDigitalButton(&GlobalGamepadController.MoveUp, 0);
                    MacProcessHIDDigitalButton(&GlobalGamepadController.MoveDown, 0);
                    MacProcessHIDDigitalButton(&GlobalGamepadController.MoveLeft, 0);
                    MacProcessHIDDigitalButton(&GlobalGamepadController.MoveRight, 0);
                    
                    switch (ElementValue)
                    {
                        case 0: // NOTE: Up
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveUp, 1);
                        } break;
                        
                        case 1: // NOTE: Up & Right
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveUp, 1);
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveRight, 1);
                        } break;
                        
                        case 2: // NOTE: Right
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveRight, 1);
                        } break;
                        
                        case 3: // NOTE: Right & Down
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveRight, 1);
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveDown, 1);
                        } break;
                        
                        case 4: // NOTE: Down
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveDown, 1);
                        } break;
                        
                        case 5: // NOTE: Down & Left
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveDown, 1);
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveLeft, 1);
                        } break;
                        
                        case 6: // NOTE: Left
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveLeft, 1);
                        } break;
                        
                        case 7: // NOTE: Left & Up
                        {
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveLeft, 1);
                            MacProcessHIDDigitalButton(&GlobalGamepadController.MoveUp, 1);
                        } break;
                        
                        case 8: // NOTE: Centered
                        {
                        } break;
                    }
                } break;
            }
        }
        else if (UsagePage == 9)
        {
            if (ElementValue == 0 || ElementValue == 1)
            {
                switch (Usage)
                {
                    case 1: // NOTE: X Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.ActionLeft,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 2: // NOTE: A Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.ActionDown,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 3: // NOTE: B Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.ActionRight,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 4: // NOTE: Y Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.ActionUp,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 9: // NOTE: Back Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.Back,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 10: // NOTE: Start Button
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.Start,
                                                   (b32)ElementValue);
                    } break;
                    
                    case 11: // NOTE: Run
                    {
                        MacProcessHIDDigitalButton(&GlobalGamepadController.Run,
                                                   (b32)ElementValue);
                    }
                }
            }
        }
    }
    else
    {
        // NOTE(yuval): Might cause an access violation when using a PS3 controller
    }
}

internal void
MacHIDAdded(void* Context, IOReturn Result,
            void* Sender, IOHIDDeviceRef Device)
{
    CFStringRef ManufacturerCFSR = (CFStringRef)IOHIDDeviceGetProperty(Device,
                                                                       CFSTR(kIOHIDManufacturerKey));
    CFStringRef ProductCFSR = (CFStringRef)IOHIDDeviceGetProperty(Device,
                                                                  CFSTR(kIOHIDProductKey));
    
    const char* Manufacturer = CFStringGetCStringPtr(ManufacturerCFSR,
                                                     kCFStringEncodingMacRoman);
    const char* Product = CFStringGetCStringPtr(ProductCFSR,
                                                kCFStringEncodingMacRoman);
    
    if (!Manufacturer)
    {
        Manufacturer = "[unknown]";
    }
    
    if (!Product)
    {
        Product = "[unknown]";
    }
    
    // TODO(yuval): @Replace with LogDebug
    printf("Gamepad was detected: %s - %s\n", Manufacturer, Product);
    
    IOHIDDeviceRegisterInputValueCallback(Device, MacHIDAction, 0);
    
    game_controller ZeroController = { };
    GlobalGamepadController = ZeroController;
    GlobalGamepadController.IsConnected = true;
}

internal void
MacHIDRemoved(void* Context, IOReturn Result,
              void* Sender, IOHIDDeviceRef Device)
{
    printf("Gamepad was unplugged\n");
    GlobalGamepadController.IsConnected = false;
}

internal void
MacSetupGamepad()
{
    IOHIDManagerRef HIDManager = IOHIDManagerCreate(kCFAllocatorDefault,
                                                    kIOHIDOptionsTypeNone);
    
    if (HIDManager)
    {
        CFStringRef Keys[2];
        Keys[0] = CFSTR(kIOHIDDeviceUsagePageKey);
        Keys[1] = CFSTR(kIOHIDDeviceUsageKey);
        
        const s32 USAGE_VALUES_COUNT = 3;
        
        s32 UsageValues[USAGE_VALUES_COUNT] = {
            kHIDUsage_GD_Joystick,
            kHIDUsage_GD_GamePad,
            kHIDUsage_GD_MultiAxisController
        };
        
        CFDictionaryRef Dictionaries[USAGE_VALUES_COUNT];
        
        for (s32 UsageValueIndex = 0;
             UsageValueIndex < USAGE_VALUES_COUNT;
             ++UsageValueIndex)
        {
            CFNumberRef Values[2];
            
            s32 PageGDValue = kHIDPage_GenericDesktop;
            Values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                       &PageGDValue);
            
            Values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                       &UsageValues[UsageValueIndex]);
            
            Dictionaries[UsageValueIndex] = CFDictionaryCreate(kCFAllocatorDefault,
                                                               (const void**)Keys,
                                                               (const void**)Values,
                                                               2,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
            
            CFRelease(Values[0]);
            CFRelease(Values[1]);
        }
        
        CFArrayRef Criteria = CFArrayCreate(kCFAllocatorDefault,
                                            (const void**)Dictionaries,
                                            USAGE_VALUES_COUNT,
                                            &kCFTypeArrayCallBacks);
        
        IOHIDManagerSetDeviceMatchingMultiple(HIDManager, Criteria);
        IOHIDManagerRegisterDeviceMatchingCallback(HIDManager, MacHIDAdded, 0);
        IOHIDManagerRegisterDeviceRemovalCallback(HIDManager, MacHIDRemoved, 0);
        IOHIDManagerScheduleWithRunLoop(HIDManager,
                                        CFRunLoopGetCurrent(),
                                        kCFRunLoopDefaultMode);
        
        if (IOHIDManagerOpen(HIDManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
        {
            // TODO(yuval): @Replace with a call to the logging subsystem
            printf("ERROR While Initializing the IOHIDManager\n");
        }
        
        for (s32 UsageValueIndex = 0;
             UsageValueIndex < USAGE_VALUES_COUNT;
             ++UsageValueIndex)
        {
            CFRelease(Dictionaries[UsageValueIndex]);
        }
        
        CFRelease(Criteria);
    }
    else
    {
        // TODO(yuval): Diagnostic
    }
}

internal void
MacUpdateWindow(mac_offscreen_buffer* Buffer)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GLfloat Vertices[] = {
        -1, -1, 0,
        -1, 1, 0,
        1, 1, 0,
        1, -1, 0,
    };
    
    GLfloat TexCoords[] = {
        0, 1,
        0, 0,
        1, 0,
        1, 1,
    };
    
    glVertexPointer(3, GL_FLOAT, 0, Vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, TexCoords);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glBindTexture(GL_TEXTURE_2D, Buffer->TextureID);
    
    glEnable(GL_TEXTURE_2D);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    Buffer->Width, Buffer->Height,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    Buffer->Memory);
    
    GLushort Indices[] = { 0, 1, 2, 0, 2, 3 };
    glColor4f(1, 1, 1, 1);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, Indices);
    glDisable(GL_TEXTURE_2D);
    
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

internal void
MacResizeBackbuffer(mac_offscreen_buffer* Buffer, s32 Width, s32 Height)
{
    const s32 BYTES_PER_PIXEL = 4;
    
    if (Buffer->Memory)
    {
        munmap(Buffer->Memory,
               Buffer->Width * Buffer->Height * Buffer->BytesPerPixel);
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->Pitch = Buffer->Width * BYTES_PER_PIXEL;
    Buffer->BytesPerPixel = BYTES_PER_PIXEL;
    Buffer->Memory = mmap(0,
                          Buffer->Width * Buffer->Height * BYTES_PER_PIXEL,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
}

internal void
MacProcessPendingMessages(NSWindow* Window, mac_state* State, game_controller* KeyboardController)
{
    NSEvent* Event;
    
    do
    {
        Event = [NSApp nextEventMatchingMask:NSEventMaskAny
                untilDate:nil
                inMode:NSDefaultRunLoopMode
                dequeue:YES];
        
        switch ([Event type])
        {
            case NSEventTypeKeyDown:
            case NSEventTypeKeyUp:
            {
                u16 KeyCode = [Event keyCode];
                b32 IsDown = ([Event type] == NSKeyDown);
                BOOL IsRepeated = [Event isARepeat];
                
                NSEventModifierFlags ModifierFlags = [Event modifierFlags];
                bool OptionKeyIsDown = ModifierFlags & NSEventModifierFlagOption;
                
                if (KeyCode == kVK_F4 && OptionKeyIsDown)
                {
                    GlobalRunning = false;
                }
#if GAME_INTERNAL
                else if (KeyCode == kVK_F11 && OptionKeyIsDown)
                {
                    if (IsDown)
                    {
                        [Window toggleFullScreen:nil];
                    }
                }
#endif
                else if (IsRepeated == NO)
                {
                    switch (KeyCode)
                    {
                        case kVK_ANSI_W:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                        } break;
                        
                        case kVK_ANSI_A:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                        } break;
                        
                        case kVK_ANSI_S:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                        } break;
                        
                        case kVK_ANSI_D:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                        } break;
                        
                        case kVK_ANSI_Q:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                        } break;
                        
                        case kVK_ANSI_E:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                        } break;
                        
                        case kVK_UpArrow:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                        } break;
                        
                        case kVK_DownArrow:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                        } break;
                        
                        case kVK_LeftArrow:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                        } break;
                        
                        case kVK_RightArrow:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                        } break;
                        
                        case kVK_Escape:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                        } break;
                        
                        case kVK_Space:
                        {
                            MacProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                        } break;
                        
#if GAME_INTERNAL
                        case kVK_ANSI_P:
                        {
                            if (IsDown)
                            {
                                GlobalPause = !GlobalPause;
                            }
                        } break;
                        
                        case kVK_ANSI_L:
                        {
                            if (IsDown)
                            {
                                if (State->InputPlayingIndex == 0)
                                {
                                    if (State->InputRecordingIndex == 0)
                                    {
                                        MacBeginRecordingInput(State, 1);
                                    }
                                    else
                                    {
                                        MacEndRecordingInput(State);
                                        MacBeginInputPlayBack(State, 1);
                                    }
                                }
                                else
                                {
                                    MacEndInputPlayBack(State);
                                    
                                    // TODO(yuval): Zero the whole input
                                    // NOT JUST THE KEYBOARD CONTROLLER
                                    game_controller ZeroController = { };
                                    *KeyboardController = ZeroController;
                                }
                            }
                        } break;
#endif
                    }
                }
            } break;
            
            case NSEventTypeFlagsChanged:
            {
                NSEventModifierFlags ModifierFlags = [Event modifierFlags];
                bool ShiftKeyIsDown = ModifierFlags & NSEventModifierFlagShift;
                
                if (ShiftKeyIsDown != KeyboardController->Run.EndedDown)
                {
                    MacProcessKeyboardMessage(&KeyboardController->Run, ShiftKeyIsDown);
                }
                
            } break;
            
            default:
            {
                [NSApp sendEvent:Event];
            } break;
        }
    } while (Event != nil);
}

internal void
MacUnloadGameCode(mac_game_code* GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        dlclose(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }
    
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

inline time_t
MacGetLastWriteTime(const char* FileName)
{
    time_t LastWriteTime = 0;
    
    struct stat FileStat;
    if (stat(FileName, &FileStat) == 0)
    {
        LastWriteTime = FileStat.st_mtimespec.tv_sec;
    }
    
    return LastWriteTime;
}

internal mac_game_code
MacLoadGameCode(const char* SourceGameCodeDLLFullPath)
{
    mac_game_code Result = { };
    
    Result.DLLLastWriteTime = MacGetLastWriteTime(SourceGameCodeDLLFullPath);
    Result.GameCodeDLL = dlopen(SourceGameCodeDLLFullPath, RTLD_LAZY | RTLD_GLOBAL);
    
    if (Result.GameCodeDLL)
    {
        Result.UpdateAndRender = (game_update_and_render*)
            dlsym(Result.GameCodeDLL, "GameUpdateAndRender");
        Result.GetSoundSamples = (game_get_sound_samples*)
            dlsym(Result.GameCodeDLL, "GameGetSoundSamples");
        
        Result.IsValid = (Result.UpdateAndRender &&
                          Result.GetSoundSamples);
    }
    
    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }
    
    return Result;
}

internal void
MacGetAppFileName(mac_state* State)
{
    pid_t PID = getpid();
    s32 SizeOfFileName = proc_pidpath(PID, State->AppFileName, sizeof(State->AppFileName));
    
    if (SizeOfFileName > 0)
    {
        printf("Process PID: %d    Path: %s\n", PID, State->AppFileName);
    }
    else
    {
        // TODO(yuval): Diagnostic
    }
    
    State->OnePastLastAppFileNameSlash = State->AppFileName + SizeOfFileName;
    
    // TODO(yuval): Revese scan for the slash
    for (char* Scan = State->AppFileName; *Scan; ++Scan)
    {
        if (*Scan == '/')
        {
            State->OnePastLastAppFileNameSlash = Scan + 1;
        }
    }
}

int
main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        Platform.DisplayMessageBox = PlatformDisplayMessageBox;
        Platform.GetDateTime = PlatformGetDateTime;
        Platform.DEBUGFreeFileMemory =  DEBUGPlatformFreeFileMemory;
        Platform.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
        Platform.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
        
        mac_state MacState = { };
        
        MacGetAppFileName(&MacState);
        
        char SourceGameCodeDLLFullPath[MAC_STATE_FILE_NAME_COUNT];
        MacBuildAppPathFileName(SourceGameCodeDLLFullPath,
                                sizeof(SourceGameCodeDLLFullPath),
                                &MacState, "game.dylib");
        
        // NOTE(yuval): Getting the timebase info
        mach_timebase_info(&GlobalTimebaseInfo);
        
        // NOTE(yuval): Gamepad setup
        MacSetupGamepad();
        
        // NOTE(yuval): Backbuffer setup
        const s32 RENDER_WIDTH = 960;
        const s32 RENDER_HEIGHT = 540;
        
        GlobalAspectRatio = (f32)RENDER_WIDTH / (f32)RENDER_HEIGHT;
        
        MacResizeBackbuffer(&GlobalBackbuffer, RENDER_WIDTH, RENDER_HEIGHT);
        
        // NOTE(yuval): NSApplication Creation
        NSApplication* App = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // NOTE(yuval): mac_app_delegate Creation
        mac_app_delegate* AppDelegate = [[mac_app_delegate alloc] init];
        [App setDelegate:AppDelegate];
        
        [NSApp finishLaunching];
        
        // NOTE(yuval): NSWindow Creation
        NSRect ScreenRect = [[NSScreen mainScreen] frame];
        
        NSRect InitialFrame = NSMakeRect((ScreenRect.size.width - RENDER_WIDTH) * 0.5,
                                         (ScreenRect.size.height - RENDER_HEIGHT) * 0.5,
                                         RENDER_WIDTH,
                                         RENDER_HEIGHT);
        
        NSWindow* Window = [[NSWindow alloc] initWithContentRect:InitialFrame
                styleMask:NSWindowStyleMaskTitled |
                NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable |
                NSWindowStyleMaskResizable
                backing:NSBackingStoreBuffered
                defer:NO];
        
        [Window setBackgroundColor:NSColor.blackColor];
        [Window setDelegate:AppDelegate];
        [Window setTitle:@"Handmade Game"];
        [Window makeKeyAndOrderFront:nil];
        
        // NOTE: If the game is running on a development machine than
        // we should show the cursor for editor debugging puposes
#if !GAME_INTERNAL
        [Window toggleFullScreen:nil];
        CGDisplayHideCursor(kCGDirectMainDisplay);
#endif
        
        CGDirectDisplayID DisplayID = CGMainDisplayID();
        CGDisplayModeRef DisplayMode = CGDisplayCopyDisplayMode(DisplayID);
        f64 MonitorRefreshRate = CGDisplayModeGetRefreshRate(DisplayMode);
        
        f32 GameUpdateHz = (f32)MonitorRefreshRate / 2.0f;
        f32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;
        
        // NOTE(yuval): Audio Setup
        mac_sound_output SoundOutput = { };
        
        SoundOutput.SoundBuffer.SamplesPerSecond = 48000;
        SoundOutput.SoundBufferSize = SoundOutput.SoundBuffer.SamplesPerSecond *
            sizeof(s16) * 2;
        
        // TODO(yuval): @Incomplete error checking for mmap faliures is required
        SoundOutput.SoundBuffer.Samples = (s16*)mmap(0,
                                                     SoundOutput.SoundBufferSize,
                                                     PROT_READ | PROT_WRITE,
                                                     MAP_PRIVATE | MAP_ANONYMOUS,
                                                     -1, 0);
        
        SoundOutput.CoreAudioBuffer = (s16*)mmap(0,
                                                 SoundOutput.SoundBufferSize,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS,
                                                 -1, 0);
        
#if GAME_INTERNAL
        void* BaseAddress = (void*)Gigabytes(8);
        s32 GameMemoryAllocationFlags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
#else
        void* BaseAddress = 0;
        s32 GameMemoryAllocationFlags = MAP_PRIVATE | MAP_ANONYMOUS;
#endif
        
        game_memory GameMemory = { };
        GameMemory.PermanentStorageSize = Megabytes(64);
        GameMemory.TransientStorageSize = Gigabytes(1);
        
        GameMemory.PlatformAPI = Platform;
        
        MacState.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
        MacState.GameMemoryBlock = mmap(BaseAddress,
                                        MacState.TotalSize,
                                        PROT_READ | PROT_WRITE,
                                        GameMemoryAllocationFlags,
                                        -1, 0);
        
        for (u32 ReplayIndex = 0;
             ReplayIndex < ArrayCount(MacState.ReplayBuffers);
             ++ReplayIndex)
        {
            mac_replay_buffer* ReplayBuffer = &MacState.ReplayBuffers[ReplayIndex];
            
            MacGetInputFileLocation(ReplayBuffer->ReplayFileName,
                                    sizeof(ReplayBuffer->ReplayFileName),
                                    false, &MacState, ReplayIndex + 1);
            
            ReplayBuffer->FileHandle = open(ReplayBuffer->ReplayFileName,
                                            O_RDWR | O_CREAT | O_TRUNC, 0644);
            
            lseek(ReplayBuffer->FileHandle, MacState.TotalSize - 1, SEEK_SET);
            write(ReplayBuffer->FileHandle, "", 1);
            
            ReplayBuffer->MemoryBlock = mmap(0,
                                             MacState.TotalSize,
                                             PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE,
                                             ReplayBuffer->FileHandle, 0);
            
            close(ReplayBuffer->FileHandle);
            
            if (ReplayBuffer->MemoryBlock == MAP_FAILED)
            {
                // TODO(yuval): @Diagnostic
                printf("Memory Mapped File For Replay Buffer %d Failed!\n", ReplayIndex + 1);
            }
        }
        
        if (MacState.GameMemoryBlock != MAP_FAILED &&
            SoundOutput.SoundBuffer.Samples != MAP_FAILED &&
            SoundOutput.CoreAudioBuffer != MAP_FAILED)
        {
            ZeroSize(SoundOutput.SoundBuffer.Samples, SoundOutput.SoundBufferSize);
            ZeroSize(SoundOutput.CoreAudioBuffer, SoundOutput.SoundBufferSize);
            
            SoundOutput.ReadCursor = SoundOutput.CoreAudioBuffer;
            SoundOutput.WriteCursor = SoundOutput.CoreAudioBuffer;
            
            MacInitCoreAudio(&SoundOutput);
            
            GameMemory.PermanentStorage = MacState.GameMemoryBlock;
            GameMemory.TransientStorage = (u8*)GameMemory.PermanentStorage +
                GameMemory.PermanentStorageSize;
            
            game_input Inputs[2] = { };
            game_input* NewInput = &Inputs[0];
            game_input* OldInput = &Inputs[1];
            
            // NOTE(yuval): OpenGL Cocoa Setup (VSYNC is used for now)
            NSOpenGLPixelFormatAttribute OpenGLAttributes[] = {
                NSOpenGLPFAAccelerated,
                NSOpenGLPFADoubleBuffer, // Uses vsync
                NSOpenGLPFADepthSize, 24,
                0
            };
            
            NSOpenGLPixelFormat* Format = [[NSOpenGLPixelFormat alloc] initWithAttributes:OpenGLAttributes];
            GlobalGLContext = [[NSOpenGLContext alloc] initWithFormat:Format shareContext:0];
            [Format release];
            
            GLint SwapInt = 1; // Uses vsync
            [GlobalGLContext setValues:&SwapInt forParameter:NSOpenGLCPSwapInterval];
            
            [GlobalGLContext setView:[Window contentView]];
            [GlobalGLContext makeCurrentContext];
            
            // NOTE(yuval): OpenGL Non-Cocoa Setup
            // TODO(yuval): @Add error checking for all OpenGL calls
            glDisable(GL_DEPTH_TEST);
            glLoadIdentity();
            glViewport(0, 0, GlobalBackbuffer.Width, GlobalBackbuffer.Height);
            
            glGenTextures(1, &GlobalBackbuffer.TextureID);
            
            glBindTexture(GL_TEXTURE_2D, GlobalBackbuffer.TextureID);
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         GlobalBackbuffer.Width,
                         GlobalBackbuffer.Height,
                         0, GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV,
                         0);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            
            // NOTE(yuval): Game Code Loading
            mac_game_code Game = MacLoadGameCode(SourceGameCodeDLLFullPath);
            
            // NOTE(yuval): Main Run Loop
            b32 FirstRun = true;
            
            GlobalRunning = true;
            GlobalPause = false;
            
            u64 LastCounter = mach_absolute_time();
            u64 FlipWallClock = mach_absolute_time();
            
            while (GlobalRunning)
            {
                time_t NewDLLWriteTime = MacGetLastWriteTime(SourceGameCodeDLLFullPath);
                
                if (NewDLLWriteTime != Game.DLLLastWriteTime)
                {
                    MacUnloadGameCode(&Game);
                    Game = MacLoadGameCode(SourceGameCodeDLLFullPath);
                }
                
                game_controller* OldKeyboardController = &OldInput->Controllers[0];
                game_controller* NewKeyboardController = &NewInput->Controllers[0];
                
                // TODO(yuval): Zero the struct using a function
                game_controller ZeroController = { };
                *NewKeyboardController = ZeroController;
                NewKeyboardController->IsConnected = true;
                NewKeyboardController->IsAnalog = false;
                
                for (u32 ButtonIndex = 0;
                     ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                     ++ButtonIndex)
                {
                    NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                        OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                }
                
                MacProcessPendingMessages(Window, &MacState, NewKeyboardController);
                
                if (!GlobalPause)
                {
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    
                    // TODO(yuval): Add support for multiple controllers
                    game_controller* GamepadController = &NewInput->Controllers[1];
                    *GamepadController = ZeroController;
                    
                    GamepadController->IsConnected = GlobalGamepadController.IsConnected;
                    GamepadController->IsAnalog = OldInput->Controllers[1].IsAnalog;
                    GamepadController->StickAverageX = GlobalGamepadController.StickAverageX;
                    GamepadController->StickAverageY = GlobalGamepadController.StickAverageY;
                    
                    for (u32 ButtonIndex = 0;
                         ButtonIndex < ArrayCount(GamepadController->Buttons);
                         ++ButtonIndex)
                    {
                        GamepadController->Buttons[ButtonIndex] =
                            GlobalGamepadController.Buttons[ButtonIndex];
                        
                    }
                    
                    if (GamepadController->StickAverageX != 0.0f ||
                        GamepadController->StickAverageY != 0.0f)
                    {
                        GamepadController->IsAnalog = true;
                    }
                    
                    if (GamepadController->MoveUp.EndedDown ||
                        GamepadController->MoveDown.EndedDown ||
                        GamepadController->MoveLeft.EndedDown ||
                        GamepadController->MoveRight.EndedDown)
                    {
                        GamepadController->IsAnalog = false;
                    }
                    
#if 0
                    printf("Is Connected: %d\n", GamepadController->IsAnalog);
                    printf("Is Analog: %d\n", GamepadController->IsAnalog);
                    printf("Move Right: %d\n", GamepadController->MoveRight.EndedDown);
                    printf("Move Left: %d\n", GamepadController->MoveLeft.EndedDown);
                    printf("Move Up: %d\n", GamepadController->MoveUp.EndedDown);
                    printf("Move Down: %d\n\n", GamepadController->MoveDown.EndedDown);
#endif
                    
                    [GlobalGLContext makeCurrentContext];
                    
                    // NOTE(yuval): Game Update And Render
                    thread_context Thread = { };
                    
                    game_offscreen_buffer OffscreenBuffer = { };
                    OffscreenBuffer.Memory = GlobalBackbuffer.Memory;
                    OffscreenBuffer.Width = GlobalBackbuffer.Width;
                    OffscreenBuffer.Height = GlobalBackbuffer.Height;
                    OffscreenBuffer.Pitch = GlobalBackbuffer.Pitch;
                    OffscreenBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
                    
                    if (MacState.InputRecordingIndex)
                    {
                        MacRecordInput(&MacState, NewInput);
                    }
                    
                    if (MacState.InputPlayingIndex)
                    {
                        MacPlayBackInput(&MacState, NewInput);
                    }
                    
                    if (Game.UpdateAndRender)
                    {
                        Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &OffscreenBuffer);
                    }
                    
                    // NOTE(yuval): Audio Update
                    SoundOutput.SoundBuffer.SampleCount = (s32)(SoundOutput.SoundBuffer.SamplesPerSecond /
                                                                GameUpdateHz);
                    
                    if (Game.GetSoundSamples)
                    {
                        Game.GetSoundSamples(&Thread, &GameMemory, &SoundOutput.SoundBuffer);
                    }
                    
                    MacFillSoundBuffer(&SoundOutput);
                    
                    if (FirstRun)
                    {
                        if (Game.GetSoundSamples)
                        {
                            Game.GetSoundSamples(&Thread, &GameMemory, &SoundOutput.SoundBuffer);
                        }
                        
                        MacFillSoundBuffer(&SoundOutput);
                        
                        FirstRun = false;
                    }
                    
                    // NOTE(yuval): Enforcing A Video Refresh Rate
                    u64 WorkCounter = mach_absolute_time();
                    f32 WorkSecondsElapsed = MacGetSecondsElapsed(LastCounter, WorkCounter);
                    
                    f32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        u64 TimeToWait = (u64)((TargetSecondsPerFrame - WorkSecondsElapsed - 0.001)
                                               * 1.0E9 * GlobalTimebaseInfo.denom /
                                               GlobalTimebaseInfo.numer);
                        
                        u64 Now = mach_absolute_time();
                        mach_wait_until(Now + TimeToWait);
                        
                        SecondsElapsedForFrame = MacGetSecondsElapsed(LastCounter,
                                                                      mach_absolute_time());
                        
                        while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = MacGetSecondsElapsed(LastCounter,
                                                                          mach_absolute_time());
                        }
                    }
                    
                    FlipWallClock = mach_absolute_time();
                    
                    MacUpdateWindow(&GlobalBackbuffer);
                    
#if 0
                    f32 MSPerFrame = (1000.0f * MacGetSecondsElapsed(LastCounter, FlipWallClock));
                    f32 FPS = (1000.0f / MSPerFrame);
                    
                    // TODO(yuval): @Replace this with LogDebug
                    printf("%.2fms/f %.2ff/s\n", MSPerFrame, FPS);
#endif
                    [GlobalGLContext flushBuffer]; // NOTE(yuval): Uses vsync
                    
                    // TODO(yuval, eran): Metaprogramming SWAP
                    game_input* Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    
                    LastCounter = FlipWallClock;
                }
            }
            
            for (u32 ReplayIndex = 0;
                 ReplayIndex < ArrayCount(MacState.ReplayBuffers);
                 ++ReplayIndex)
            {
                mac_replay_buffer* ReplayBuffer = &MacState.ReplayBuffers[ReplayIndex];
                
                munmap(ReplayBuffer->MemoryBlock,
                       MacState.TotalSize);
            }
        }
    }
}