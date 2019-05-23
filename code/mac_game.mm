#include <Cocoa/Cocoa.h>

#include "game.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#include <AudioToolbox/AudioToolbox.h>

#include <mach/mach_time.h>

#include "mac_game.h"

global_variable b32 globalRunning;
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
RenderGradient(MacOffscreenBuffer* buffer, s32 blueOffset, s32 greenOffset)
{
    u8* row = (u8*)buffer->memory;
    
    for (s32 y = 0; y < buffer->height; ++y)
    {
        u32* pixel = (u32*)row;
        
        for (s32 x = 0; x < buffer->width; ++x)
        {
            u8 blue = (x + blueOffset);
            u8 green = (y + greenOffset);
            
            *pixel++ = ((0xFF << 24) | (green << 8) | blue);
        }
        
        row += buffer->pitch;
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
    
    for (u32 sampleIndex = 0; sampleIndex < inNumberFrames; ++sampleIndex)
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
MacUpdateWindow(MacOffscreenBuffer* buffer)
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    GLfloat vertices[] = {
        -1, -1, 0,
        -1, 1, 0,
        1, 1, 0,
        1, -1, 0,
    };
    
    GLfloat texCoords[] = {
        0, 0,
        0, 1,
        1, 1,
        1, 0,
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
    
    buffer->memory = (u8*)malloc(buffer->pitch * buffer->height);
}

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        mach_timebase_info(&globalTimebaseInfo);
        
        s32 renderWidth = 1280;
        s32 renderHeight = 720;
        
        MacResizeBackbuffer(&globalBackbuffer, renderWidth, renderHeight);
        
        // NOTE(yuval): NSApplication Creation
        NSApplication* app = [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // NOTE(yuval): MacAppDelegate Creation
        MacAppDelegate* appDelegate = [[MacAppDelegate alloc] init];
        [app setDelegate:appDelegate];
        
        [NSApp finishLaunching];
        
        // NOTE(yuval): NSWindow Creation
        NSRect screenRect = [[NSScreen mainScreen] frame];
        
        NSRect initialFrame = NSMakeRect((screenRect.size.width - renderWidth) * 0.5,
                                         (screenRect.size.height - renderHeight) * 0.5,
                                         renderWidth,
                                         renderHeight);
        
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
        
        if (soundOutput.soundBuffer.samples != MAP_FAILED &&
            soundOutput.coreAudioBuffer != MAP_FAILED)
        {
            ZeroSize(soundOutput.soundBuffer.samples, soundOutput.soundBufferSize);
            ZeroSize(soundOutput.coreAudioBuffer, soundOutput.soundBufferSize);
            
            soundOutput.readCursor = soundOutput.coreAudioBuffer;
            soundOutput.writeCursor = soundOutput.coreAudioBuffer;
            
            MacInitCoreAudio(&soundOutput);
            
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
            
            // NOTE(yuval): Final Pre-Loop Setup
            s32 xOffset = 0;
            s32 yOffset = 0;
            
            // NOTE(yuval): Main Run Loop
            globalRunning = true;
            
            u64 lastCounter = mach_absolute_time();
            u64 flipWallClock = mach_absolute_time();
            
            while (globalRunning)
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
                        default:
                        {
                            [NSApp sendEvent:event];
                        } break;
                    }
                } while (event != nil);
                
                [globalGLContext makeCurrentContext];
                
                RenderGradient(&globalBackbuffer, xOffset++, yOffset);
                
                flipWallClock = mach_absolute_time();
                
                MacUpdateWindow(&globalBackbuffer);
                
                f32 msPerFrame = (1000.0f * MacGetSecondsElapsed(lastCounter, flipWallClock));
                f32 fps = (1000.0f / msPerFrame);
                
                // TODO(yuval): @Replace this with LogDebug
                printf("%.2fms/f %.2ff/s\n", msPerFrame, fps);
                
                [globalGLContext flushBuffer]; // NOTE(yuval): Uses vsync
                
                lastCounter = flipWallClock;
            }
        }
    }
}