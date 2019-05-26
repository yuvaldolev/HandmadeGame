#include <Cocoa/Cocoa.h>

#include "game.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#include <AudioToolbox/AudioToolbox.h>
#include <IOKit/hid/IOHIDLib.h>

#include <dlfcn.h>
#include <libproc.h>
#include <stdio.h> // TODO(yuval): Remove this temporary include
#include <sys/stat.h>

#include <mach/mach_time.h>

#include "mac_game.h"


global_variable b32 globalRunning;
global_variable b32 globalPause;
global_variable MacOffscreenBuffer globalBackbuffer;
global_variable NSOpenGLContext* globalGLContext;
global_variable mach_timebase_info_data_t globalTimebaseInfo;

@interface MacAppDelegate : NSObject<NSApplicationDelegate, NSWindowDelegate>
@end

@implementation MacAppDelegate
- (void)applicationDidFinishLaunching:(id)sender
{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	return YES;
}

- (void)applicationWillTerminate:(NSApplication*)sender
{
	printf("applicationWillTerminate\n");
}

- (void)windowWillClose:(id)sender
{
    globalRunning = false;
}

- (void)windowDidResize:(NSNotification*)notification
{
	NSWindow* window = [notification object];
	NSRect frame = [window frame];
    
	[globalGLContext update];
    
	// OpenGL reshape. Typically done in the view
	glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    glViewport(0, 0, frame.size.width, frame.size.height);
    
    printf("Window Resized!\n");
}
@end

internal void
MacBuildAppPathFileName(char* dest, memory_index destCount,
                        MacState* state, const char* fileName)
{
    CatStrings(dest, destCount,
               state->appFileName,
               state->onePastLastAppFileNameSlash - state->appFileName,
               fileName, StringLength(fileName));
}

internal void
MacGetInputFileLocation(char* dest, memory_index destCount,
                        b32 isInputStream, MacState* state,
                        s32 slotIndex)
{
    char temp[64];
    FormatString(temp, sizeof(temp), "loop_edit_%d_%s.gi",
                 slotIndex, isInputStream ? "input" : "state");
    
    MacBuildAppPathFileName(dest, destCount, state, temp);
}

internal MacReplayBuffer*
MacGetReplayBuffer(MacState* state, u32 index)
{
    Assert(index < ArrayCount(state->replayBuffers));
    MacReplayBuffer* result = &state->replayBuffers[index];
    return result;
}

internal void
MacBeginRecordingInput(MacState* state, s32 inputRecordingIndex)
{
    MacReplayBuffer* replayBuffer = MacGetReplayBuffer(state, inputRecordingIndex - 1);
    
    if (replayBuffer->memoryBlock)
    {
        state->inputRecordingIndex = inputRecordingIndex;
        
        char fileName[MAC_STATE_FILE_NAME_COUNT];
        MacGetInputFileLocation(fileName, sizeof(fileName),
                                true, state, inputRecordingIndex);
        
        state->recordingHandle = open(fileName,
                                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
        
        // @Replace memcpy with Copy
        memcpy(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
    }
}

internal void
MacEndRecordingInput(MacState* state)
{
    close(state->recordingHandle);
    state->inputRecordingIndex = 0;
}

internal void
MacBeginInputPlayBack(MacState* state, s32 inputPlayingIndex)
{
    MacReplayBuffer* replayBuffer = MacGetReplayBuffer(state, inputPlayingIndex - 1);
    
    if (replayBuffer->memoryBlock)
    {
        state->inputPlayingIndex = inputPlayingIndex;
        
        char fileName[MAC_STATE_FILE_NAME_COUNT];
        MacGetInputFileLocation(fileName, sizeof(fileName),
                                true, state, inputPlayingIndex);
        
        state->playBackHandle = open(fileName, O_RDONLY);
        
        // @Replace memcpy with Copy
        memcpy(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
    }
}

internal void
MacEndInputPlayBack(MacState* state)
{
    close(state->playBackHandle);
    state->inputPlayingIndex = 0;
}

internal void
MacRecordInput(MacState* state, GameInput* newInput)
{
    memory_index bytesWritten = write(state->recordingHandle, newInput, sizeof(*newInput));
    // TODO(yuval): Check if bytes written is different then the size of newInput
}

internal void
MacPlayBackInput(MacState* state, GameInput* newInput)
{
    memory_index bytesRead = read(state->playBackHandle, newInput, sizeof(*newInput));
    
    if (!bytesRead)
    {
        s32 playingIndex = state->inputPlayingIndex;
        MacEndInputPlayBack(state);
        MacBeginInputPlayBack(state, playingIndex);
    }
}

internal f32
MacGetSecondsElapsed(u64 start, u64 end)
{
    // NOTE(yuval): Elapsed nanoseconds calculation
    f32 result = ((f32)(end - start) *
                  ((f32)globalTimebaseInfo.numer) /
                  ((f32)globalTimebaseInfo.denom));
    
    // NOTE(yuval): Conversion to seconds
    result *= 1.0E-9;
    
    return result;
}

internal void
MacProcessKeyboardMessage(GameButtonState* newState, b32 isDown)
{
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}

internal void
MacFillSoundBuffer(MacSoundOutput* soundOutput)
{
    s16* sourceSample = soundOutput->soundBuffer.samples;
    
    for (s32 sampleIndex = 0;
         sampleIndex < soundOutput->soundBuffer.sampleCount;
         ++sampleIndex)
    {
        *soundOutput->writeCursor++ = *sourceSample++;
        *soundOutput->writeCursor++ = *sourceSample++;
        
        if (((u8*)soundOutput->writeCursor) >=
            ((u8*)soundOutput->coreAudioBuffer + soundOutput->soundBufferSize))
        {
            soundOutput->writeCursor = soundOutput->coreAudioBuffer;
        }
    }
}

OSStatus
MacAudioUnitCallback(void* inRefCon,
                     AudioUnitRenderActionFlags* ioActionFlags,
                     const AudioTimeStamp* inTimeStamp,
                     UInt32 inBusNumber,
                     UInt32 inNumberFrames,
                     AudioBufferList* ioData)
{
    MacSoundOutput* soundOutput = (MacSoundOutput*)inRefCon;
    
    if (soundOutput->readCursor == soundOutput->writeCursor)
    {
        soundOutput->soundBuffer.sampleCount = 0;
    }
    
    u32 sampleCount = inNumberFrames;
    if (soundOutput->soundBuffer.sampleCount < sampleCount)
    {
        sampleCount = soundOutput->soundBuffer.sampleCount;
    }
    
    s16* outputBufferL = (s16*)ioData->mBuffers[0].mData;
    s16* outputBufferR = (s16*)ioData->mBuffers[1].mData;
    
    for (u32 sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        outputBufferL[sampleIndex] = *soundOutput->readCursor++;
        outputBufferR[sampleIndex] = *soundOutput->readCursor++;
        
        if ((u8*)soundOutput->readCursor >=
            (((u8*)soundOutput->coreAudioBuffer) + soundOutput->soundBufferSize))
        {
            soundOutput->readCursor = soundOutput->coreAudioBuffer;
        }
    }
    
    for (u32 sampleIndex = sampleCount; sampleIndex < inNumberFrames; ++sampleIndex)
    {
        outputBufferL[sampleIndex] = 0;
        outputBufferR[sampleIndex] = 0;
    }
    
    return noErr;
}

internal void
MacInitCoreAudio(MacSoundOutput* soundOutput)
{
    AudioComponentDescription componentDescription = { };
    componentDescription.componentType = kAudioUnitType_Output;
    componentDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
    componentDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent outputComponent = AudioComponentFindNext(0, &componentDescription);
    
    AudioUnit audioUnit;
    AudioComponentInstanceNew(outputComponent, &audioUnit);
    AudioUnitInitialize(audioUnit);
    
    AudioStreamBasicDescription audioDescriptor;
    audioDescriptor.mSampleRate = soundOutput->soundBuffer.samplesPerSecond;
    audioDescriptor.mFormatID = kAudioFormatLinearPCM;
    audioDescriptor.mFormatFlags = (kAudioFormatFlagIsSignedInteger |
                                    kAudioFormatFlagIsNonInterleaved |
                                    kAudioFormatFlagIsPacked);
    audioDescriptor.mFramesPerPacket = 1;
    audioDescriptor.mChannelsPerFrame = 2;
    audioDescriptor.mBitsPerChannel = sizeof(s16) * 8;
    audioDescriptor.mBytesPerFrame = sizeof(s16);
    audioDescriptor.mBytesPerPacket = audioDescriptor.mFramesPerPacket *
        audioDescriptor.mBytesPerFrame;
    
    AudioUnitSetProperty(audioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input,
                         0, &audioDescriptor,
                         sizeof(audioDescriptor));
    
    AURenderCallbackStruct renderCallback;
    renderCallback.inputProc = MacAudioUnitCallback;
    renderCallback.inputProcRefCon = soundOutput;
    
    AudioUnitSetProperty(audioUnit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Global,
                         0, &renderCallback,
                         sizeof(renderCallback));
    
    AudioOutputUnitStart(audioUnit);
}

internal void
MacHIDAdded(void* context, IOReturn result,
            void* sender, IOHIDDeviceRef device)
{
    CFStringRef manufacturerCFSR = (CFStringRef)IOHIDDeviceGetProperty(device,
                                                                       CFSTR(kIOHIDManufacturerKey));
    CFStringRef productCFSR = (CFStringRef)IOHIDDeviceGetProperty(device,
                                                                  CFSTR(kIOHIDProductKey));
    
    const char* manufacturer = CFStringGetCStringPtr(manufacturerCFSR,
                                                     kCFStringEncodingMacRoman);
    const char* product = CFStringGetCStringPtr(productCFSR,
                                                kCFStringEncodingMacRoman);
    
    if (!manufacturer)
    {
        manufacturer = "[unknown]";
    }
    
    if (!product)
    {
        product = "[unknown]";
    }
    
    // TODO(yuval): @Replace with LogDebug
    printf("Gamepad was detected: %s %s\n", manufacturer, product);
}

internal void
MacHIDRemoved(void* context, IOReturn result,
              void* sender, IOHIDDeviceRef device)
{
    printf("Gamepad was unplugged\n");
}

internal void
MacSetupGamepad()
{
    IOHIDManagerRef HIDManager = IOHIDManagerCreate(kCFAllocatorDefault,
                                                    kIOHIDOptionsTypeNone);
    
    if (HIDManager)
    {
        CFStringRef keys[2];
        keys[0] = CFSTR(kIOHIDDeviceUsagePageKey);
        keys[1] = CFSTR(kIOHIDDeviceUsageKey);
        
        const s32 USAGE_VALUES_COUNT = 3;
        
        s32 usageValues[USAGE_VALUES_COUNT] = {
            kHIDUsage_GD_Joystick,
            kHIDUsage_GD_GamePad,
            kHIDUsage_GD_MultiAxisController
        };
        
        CFDictionaryRef dictionaries[USAGE_VALUES_COUNT];
        
        for (s32 usageValueIndex = 0;
             usageValueIndex < USAGE_VALUES_COUNT;
             ++usageValueIndex)
        {
            CFNumberRef values[2];
            
            s32 pageGDValue = kHIDPage_GenericDesktop;
            values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                       &pageGDValue);
            
            values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
                                       &usageValues[usageValueIndex]);
            
            dictionaries[usageValueIndex] = CFDictionaryCreate(kCFAllocatorDefault,
                                                               (const void**)keys,
                                                               (const void**)values,
                                                               2,
                                                               &kCFTypeDictionaryKeyCallBacks,
                                                               &kCFTypeDictionaryValueCallBacks);
            
            CFRelease(values[0]);
            CFRelease(values[1]);
        }
        
        CFArrayRef criteria = CFArrayCreate(kCFAllocatorDefault,
                                            (const void**)dictionaries,
                                            USAGE_VALUES_COUNT,
                                            &kCFTypeArrayCallBacks);
        
        IOHIDManagerSetDeviceMatchingMultiple(HIDManager, criteria);
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
        
        for (s32 usageValueIndex = 0;
             usageValueIndex < USAGE_VALUES_COUNT;
             ++usageValueIndex)
        {
            CFRelease(dictionaries[usageValueIndex]);
        }
        
        CFRelease(criteria);
    }
    else
    {
        // TODO(yuval): Diagnostic
    }
}

internal void
MacUpdateWindow(MacOffscreenBuffer* buffer)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GLfloat vertices[] = {
        -1, -1, 0,
        -1, 1, 0,
        1, 1, 0,
        1, -1, 0,
    };
    
    GLfloat texCoords[] = {
        0, 1,
        0, 0,
        1, 0,
        1, 1,
    };
    
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glBindTexture(GL_TEXTURE_2D, buffer->textureID);
    
    glEnable(GL_TEXTURE_2D);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    buffer->width, buffer->height,
                    GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                    buffer->memory);
    
    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
    glColor4f(1, 1, 1, 1);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glDisable(GL_TEXTURE_2D);
    
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

internal void
MacResizeBackbuffer(MacOffscreenBuffer* buffer, s32 width, s32 height)
{
    const s32 BYTES_PER_PIXEL = 4;
    
    if (buffer->memory)
    {
        munmap(buffer->memory,
               buffer->width * buffer->height * buffer->bytesPerPixel);
    }
    
    buffer->width = width;
    buffer->height = height;
    buffer->pitch = buffer->width * BYTES_PER_PIXEL;
    buffer->bytesPerPixel = BYTES_PER_PIXEL;
    buffer->memory = mmap(0,
                          buffer->width * buffer->height * BYTES_PER_PIXEL,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);
}

internal void
MacProcessPendingMessages(MacState* state, GameController* keyboardController)
{
    NSEvent* event;
    
    do
    {
        event = [NSApp nextEventMatchingMask:NSEventMaskAny
                untilDate:nil
                inMode:NSDefaultRunLoopMode
                dequeue:YES];
        
        switch ([event type])
        {
            case NSEventTypeKeyDown:
            case NSEventTypeKeyUp:
            {
                unichar keyChar = [[event charactersIgnoringModifiers] characterAtIndex:0];
                b32 isDown = ([event type] == NSKeyDown);
                BOOL isRepeated = [event isARepeat];
                
                NSEventModifierFlags modifierFlags = [event modifierFlags];
                b32 optionKeyIsDown = modifierFlags & NSEventModifierFlagOption;
                
                if (keyChar == NSF4FunctionKey && optionKeyIsDown)
                {
                    globalRunning = false;
                }
                else if (isRepeated == NO)
                {
                    switch (keyChar)
                    {
                        case 'w':
                        {
                            MacProcessKeyboardMessage(&keyboardController->moveUp, isDown);
                        } break;
                        
                        case 'a':
                        {
                            MacProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
                        } break;
                        
                        case 's':
                        {
                            MacProcessKeyboardMessage(&keyboardController->moveDown, isDown);
                        } break;
                        
                        case 'd':
                        {
                            MacProcessKeyboardMessage(&keyboardController->moveRight, isDown);
                        } break;
                        
                        case 'q':
                        {
                            MacProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
                        } break;
                        
                        case 'e':
                        {
                            MacProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
                        } break;
                        
                        case NSUpArrowFunctionKey:
                        {
                            MacProcessKeyboardMessage(&keyboardController->actionUp, isDown);
                        } break;
                        
                        case NSDownArrowFunctionKey:
                        {
                            MacProcessKeyboardMessage(&keyboardController->actionDown, isDown);
                        } break;
                        
                        case NSLeftArrowFunctionKey:
                        {
                            MacProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
                        } break;
                        
                        case NSRightArrowFunctionKey:
                        {
                            MacProcessKeyboardMessage(&keyboardController->actionRight, isDown);
                        } break;
                        
                        case 0x1B: // NOTE: Escape Key
                        {
                            MacProcessKeyboardMessage(&keyboardController->back, isDown);
                        } break;
                        
                        case ' ': // NOTE: Space Key
                        {
                            MacProcessKeyboardMessage(&keyboardController->start, isDown);
                        } break;
                        
#if GAME_INTERNAL
                        case 'p':
                        {
                            if (isDown)
                            {
                                globalPause = !globalPause;
                            }
                        } break;
                        
                        case 'l':
                        {
                            if (isDown)
                            {
                                if (state->inputPlayingIndex == 0)
                                {
                                    if (state->inputRecordingIndex == 0)
                                    {
                                        MacBeginRecordingInput(state, 1);
                                    }
                                    else
                                    {
                                        MacEndRecordingInput(state);
                                        MacBeginInputPlayBack(state, 1);
                                    }
                                }
                                else
                                {
                                    MacEndInputPlayBack(state);
                                    
                                    // TODO(yuval): Zero the whole input
                                    // NOT JUST THE KEYBOARD CONTROLLER
                                    GameController zeroController = { };
                                    *keyboardController = zeroController;
                                }
                            }
                        } break;
#endif
                    }
                }
                
            } break;
            
            default:
            {
                [NSApp sendEvent:event];
            } break;
        }
    } while (event != nil);
}

internal void
MacUnloadGameCode(MacGameCode* gameCode)
{
    if (gameCode->gameCodeDLL)
    {
        dlclose(gameCode->gameCodeDLL);
        gameCode->gameCodeDLL = 0;
    }
    
    gameCode->isValid = false;
    gameCode->UpdateAndRender = 0;
    gameCode->GetSoundSamples = 0;
    gameCode->Log = 0;
}

inline time_t
MacGetLastWriteTime(const char* fileName)
{
    time_t lastWriteTime = 0;
    
    struct stat fileStat;
    if (stat(fileName, &fileStat) == 0)
    {
        lastWriteTime = fileStat.st_mtimespec.tv_sec;
    }
    
    return lastWriteTime;
}

internal MacGameCode
MacLoadGameCode(const char* sourceGameCodeDLLFullPath)
{
    MacGameCode result = { };
    
    result.DLLLastWriteTime = MacGetLastWriteTime(sourceGameCodeDLLFullPath);
    
    result.gameCodeDLL = dlopen(sourceGameCodeDLLFullPath, RTLD_LAZY | RTLD_GLOBAL);
    if (result.gameCodeDLL)
    {
        result.UpdateAndRender = (GameUpdateAndRenderType*)
            dlsym(result.gameCodeDLL, "GameUpdateAndRender");
        result.GetSoundSamples = (GameGetSoundSamplesType*)
            dlsym(result.gameCodeDLL, "GameGetSoundSamples");
        result.Log = (LogType*)
            dlsym(result.gameCodeDLL, "Log");
        
        result.isValid = (result.UpdateAndRender &&
                          result.GetSoundSamples &&
                          result.Log);
    }
    
    if (!result.isValid)
    {
        result.UpdateAndRender = 0;
        result.GetSoundSamples = 0;
        result.Log = 0;
    }
    
    return result;
}

internal void
MacGetAppFileName(MacState* state)
{
    pid_t PID = getpid();
    s32 sizeOfFileName = proc_pidpath(PID, state->appFileName, sizeof(state->appFileName));
    
    if (sizeOfFileName > 0)
    {
        printf("Process PID: %d    Path: %s\n", PID, state->appFileName);
    }
    else
    {
        // TODO(yuval): Diagnostic
    }
    
    state->onePastLastAppFileNameSlash = state->appFileName + sizeOfFileName;
    
    for (char* scan = state->appFileName; *scan; ++scan)
    {
        if (*scan == '/')
        {
            state->onePastLastAppFileNameSlash = scan + 1;
        }
    }
}

int
main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        MacState macState = { };
        
        MacGetAppFileName(&macState);
        
        char sourceGameCodeDLLFullPath[MAC_STATE_FILE_NAME_COUNT];
        MacBuildAppPathFileName(sourceGameCodeDLLFullPath,
                                sizeof(sourceGameCodeDLLFullPath),
                                &macState, "game.dylib");
        
        mach_timebase_info(&globalTimebaseInfo);
        
        const s32 RENDER_WIDTH = 1280;
        const s32 RENDER_HEIGHT = 720;
        
        MacResizeBackbuffer(&globalBackbuffer, RENDER_WIDTH, RENDER_HEIGHT);
        
        // NOTE(yuval): NSApplication Creation
        NSApplication* app = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // NOTE(yuval): MacAppDelegate Creation
        MacAppDelegate* appDelegate = [[MacAppDelegate alloc] init];
        [app setDelegate:appDelegate];
        
        [NSApp finishLaunching];
        
        // NOTE(yuval): NSWindow Creation
        NSRect screenRect = [[NSScreen mainScreen] frame];
        
        NSRect initialFrame = NSMakeRect((screenRect.size.width - RENDER_WIDTH) * 0.5,
                                         (screenRect.size.height - RENDER_HEIGHT) * 0.5,
                                         RENDER_WIDTH,
                                         RENDER_HEIGHT);
        
        NSWindow* window = [[NSWindow alloc] initWithContentRect:initialFrame
                styleMask:NSWindowStyleMaskTitled |
                NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable |
                NSWindowStyleMaskResizable
                backing:NSBackingStoreBuffered
                defer:NO];
        
        [window setBackgroundColor:NSColor.blackColor];
        [window setDelegate:appDelegate];
        [window setTitle:@"Handmade Game"];
        [window makeKeyAndOrderFront:nil];
        
        CGDirectDisplayID displayID = CGMainDisplayID();
        CGDisplayModeRef displayMode = CGDisplayCopyDisplayMode(displayID);
        f64 monitorRefreshRate = CGDisplayModeGetRefreshRate(displayMode);
        
        f32 gameUpdateHz = (f32)monitorRefreshRate / 2.0f;
        f32 targetSecondsPerFrame = 1.0f / gameUpdateHz;
        
        // NOTE(yuval): Audio Setup
        MacSoundOutput soundOutput = { };
        
        soundOutput.soundBuffer.samplesPerSecond = 48000;
        soundOutput.soundBufferSize = soundOutput.soundBuffer.samplesPerSecond *
            sizeof(s16) * 2;
        
        // TODO(yuval): @Incomplete error checking for mmap faliures is required
        soundOutput.soundBuffer.samples = (s16*)mmap(0,
                                                     soundOutput.soundBufferSize,
                                                     PROT_READ | PROT_WRITE,
                                                     MAP_PRIVATE | MAP_ANONYMOUS,
                                                     -1, 0);
        
        soundOutput.coreAudioBuffer = (s16*)mmap(0,
                                                 soundOutput.soundBufferSize,
                                                 PROT_READ | PROT_WRITE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS,
                                                 -1, 0);
        
#if GAME_INTERNAL
        void* baseAddress = (void*)Gigabytes(8);
        s32 gameMemoryAllocationFlags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
#else
        void* baseAddress = 0;
        s32 gameMemoryAllocationFlags = MAP_PRIVATE | MAP_ANONYMOUS;
#endif
        
        GameMemory gameMemory = { };
        gameMemory.permanentStorageSize = Megabytes(64);
        gameMemory.transientStorageSize = Gigabytes(1);
        
        macState.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
        
        macState.gameMemoryBlock = mmap(baseAddress,
                                        macState.totalSize,
                                        PROT_READ | PROT_WRITE,
                                        gameMemoryAllocationFlags,
                                        -1, 0);
        
        for (s32 replayIndex = 0;
             replayIndex < ArrayCount(macState.replayBuffers);
             ++replayIndex)
        {
            MacReplayBuffer* replayBuffer = &macState.replayBuffers[replayIndex];
            
            MacGetInputFileLocation(replayBuffer->replayFileName,
                                    sizeof(replayBuffer->replayFileName),
                                    false, &macState, replayIndex + 1);
            
            replayBuffer->fileHandle = open(replayBuffer->replayFileName,
                                            O_RDWR | O_CREAT | O_TRUNC, 0644);
            
            lseek(replayBuffer->fileHandle, macState.totalSize - 1, SEEK_SET);
            write(replayBuffer->fileHandle, "", 1);
            
            replayBuffer->memoryBlock = mmap(0,
                                             macState.totalSize,
                                             PROT_READ | PROT_WRITE,
                                             MAP_PRIVATE,
                                             replayBuffer->fileHandle, 0);
            
            close(replayBuffer->fileHandle);
            
            if (replayBuffer->memoryBlock == MAP_FAILED)
            {
                // TODO(yuval): @Diagnostic
                printf("Memory Mapped File For Replay Buffer %d Failed!\n", replayIndex + 1);
            }
        }
        
        if (macState.gameMemoryBlock != MAP_FAILED &&
            soundOutput.soundBuffer.samples != MAP_FAILED &&
            soundOutput.coreAudioBuffer != MAP_FAILED)
        {
            ZeroSize(soundOutput.soundBuffer.samples, soundOutput.soundBufferSize);
            ZeroSize(soundOutput.coreAudioBuffer, soundOutput.soundBufferSize);
            
            soundOutput.readCursor = soundOutput.coreAudioBuffer;
            soundOutput.writeCursor = soundOutput.coreAudioBuffer;
            
            MacInitCoreAudio(&soundOutput);
            
            gameMemory.permanentStorage = macState.gameMemoryBlock;
            gameMemory.transientStorage = (u8*)gameMemory.permanentStorage +
                gameMemory.permanentStorageSize;
            
            GameInput inputs[2] = { };
            GameInput* newInput = &inputs[0];
            GameInput* oldInput = &inputs[1];
            
            // NOTE(yuval): OpenGL Cocoa Setup (VSYNC is used for now)
            NSOpenGLPixelFormatAttribute openGLAttributes[] = {
                NSOpenGLPFAAccelerated,
                NSOpenGLPFADoubleBuffer, // Uses vsync
                NSOpenGLPFADepthSize, 24,
                0
            };
            
            NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:openGLAttributes];
            globalGLContext = [[NSOpenGLContext alloc] initWithFormat:format shareContext:0];
            [format release];
            
            GLint swapInt = 1; // Uses vsync
            [globalGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
            
            [globalGLContext setView:[window contentView]];
            [globalGLContext makeCurrentContext];
            
            // NOTE(yuval): OpenGL Non-Cocoa Setup
            // TODO(yuval): @Add error checking for all OpenGL calls
            glDisable(GL_DEPTH_TEST);
            glLoadIdentity();
            glViewport(0, 0, globalBackbuffer.width, globalBackbuffer.height);
            
            glGenTextures(1, &globalBackbuffer.textureID);
            
            glBindTexture(GL_TEXTURE_2D, globalBackbuffer.textureID);
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         globalBackbuffer.width,
                         globalBackbuffer.height,
                         0, GL_BGRA,
                         GL_UNSIGNED_INT_8_8_8_8_REV,
                         0);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            
            // NOTE(yuval): Game Code Loading
            MacGameCode game = MacLoadGameCode(sourceGameCodeDLLFullPath);
            
            // NOTE(yuval): Main Run Loop
            b32 firstRun = true;
            
            globalRunning = true;
            globalPause = false;
            
            u64 lastCounter = mach_absolute_time();
            u64 flipWallClock = mach_absolute_time();
            
            while (globalRunning)
            {
                time_t newDLLWriteTime = MacGetLastWriteTime(sourceGameCodeDLLFullPath);
                
                if (newDLLWriteTime != game.DLLLastWriteTime)
                {
                    MacUnloadGameCode(&game);
                    game = MacLoadGameCode(sourceGameCodeDLLFullPath);
                }
                
                GameController* oldKeyboardController = &oldInput->controllers[0];
                GameController* newKeyboardController = &newInput->controllers[0];
                
                // TODO(yuval): Zero the struct using a function
                GameController zeroController = { };
                *newKeyboardController = zeroController;
                newKeyboardController->isConnected = true;
                newKeyboardController->isAnalog = false;
                
                for (s32 buttonIndex = 0;
                     buttonIndex < ArrayCount(newKeyboardController->buttons);
                     ++buttonIndex)
                {
                    newKeyboardController->buttons[buttonIndex].endedDown =
                        oldKeyboardController->buttons[buttonIndex].endedDown;
                }
                
                MacProcessPendingMessages(&macState, newKeyboardController);
                
                if (!globalPause)
                {
                    [globalGLContext makeCurrentContext];
                    
                    newInput->dtForFrame = targetSecondsPerFrame;
                    
                    // NOTE(yuval): Game Update And Render
                    ThreadContext thread = { };
                    
                    GameOffscreenBuffer offscreenBuffer = { };
                    offscreenBuffer.memory = globalBackbuffer.memory;
                    offscreenBuffer.width = globalBackbuffer.width;
                    offscreenBuffer.height = globalBackbuffer.height;
                    offscreenBuffer.pitch = globalBackbuffer.pitch;
                    offscreenBuffer.bytesPerPixel = globalBackbuffer.bytesPerPixel;
                    
                    if (macState.inputRecordingIndex)
                    {
                        MacRecordInput(&macState, newInput);
                    }
                    
                    if (macState.inputPlayingIndex)
                    {
                        MacPlayBackInput(&macState, newInput);
                    }
                    
                    if (game.UpdateAndRender)
                    {
                        game.UpdateAndRender(&thread, &gameMemory, newInput, &offscreenBuffer);
                    }
                    
                    // NOTE(yuval): Audio Update
                    soundOutput.soundBuffer.sampleCount = soundOutput.soundBuffer.samplesPerSecond /
                        gameUpdateHz;
                    
                    if (game.GetSoundSamples)
                    {
                        game.GetSoundSamples(&thread, &gameMemory, &soundOutput.soundBuffer);
                    }
                    
                    MacFillSoundBuffer(&soundOutput);
                    
                    if (firstRun)
                    {
                        if (game.GetSoundSamples)
                        {
                            game.GetSoundSamples(&thread, &gameMemory, &soundOutput.soundBuffer);
                        }
                        
                        MacFillSoundBuffer(&soundOutput);
                        
                        firstRun = false;
                    }
                    
                    // NOTE(yuval): Enforcing A Video Refresh Rate
                    u64 workCounter = mach_absolute_time();
                    f32 workSecondsElapsed = MacGetSecondsElapsed(lastCounter, workCounter);
                    
                    f32 secondsElapsedForFrame = workSecondsElapsed;
                    if (secondsElapsedForFrame < targetSecondsPerFrame)
                    {
                        u64 timeToWait = (u64)((targetSecondsPerFrame - workSecondsElapsed - 0.001) * 1.0E9 *
                                               globalTimebaseInfo.denom /
                                               globalTimebaseInfo.numer);
                        
                        u64 now = mach_absolute_time();
                        mach_wait_until(now + timeToWait);
                        
                        secondsElapsedForFrame = MacGetSecondsElapsed(lastCounter,
                                                                      mach_absolute_time());
                        
                        while (secondsElapsedForFrame < targetSecondsPerFrame)
                        {
                            secondsElapsedForFrame = MacGetSecondsElapsed(lastCounter,
                                                                          mach_absolute_time());
                        }
                    }
                    
                    flipWallClock = mach_absolute_time();
                    
                    MacUpdateWindow(&globalBackbuffer);
                    
                    f32 msPerFrame = (1000.0f * MacGetSecondsElapsed(lastCounter, flipWallClock));
                    f32 fps = (1000.0f / msPerFrame);
                    
                    // TODO(yuval): @Replace this with LogDebug
                    printf("%.2fms/f %.2ff/s\n", msPerFrame, fps);
                    
                    [globalGLContext flushBuffer]; // NOTE(yuval): Uses vsync
                    
                    // TODO(yuval, eran): Metaprogramming SWAP
                    GameInput* temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;
                    
                    lastCounter = flipWallClock;
                }
            }
            
#if 1
            for (s32 replayIndex = 0;
                 replayIndex < ArrayCount(macState.replayBuffers);
                 ++replayIndex)
            {
                MacReplayBuffer* replayBuffer = &macState.replayBuffers[replayIndex];
                
                munmap(replayBuffer->memoryBlock,
                       macState.totalSize);
            }
#endif
        }
    }
}