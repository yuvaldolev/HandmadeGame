#include <Cocoa/Cocoa.h>

#include "game.h"

#include "mac_game.h"

global_variable b32 globalRunning;
global_variable MacOffscreenBuffer globalBackbuffer;

@interface MacWindowDelegate : NSObject<NSWindowDelegate>
@end

@implementation MacWindowDelegate

- (void)windowWillClose:(id)sender
{
    globalRunning = false;
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

internal void
MacUpdateWindow(NSWindow* window, MacOffscreenBuffer* buffer)
{
    //u8* bufferMemory = (u8*)buffer->memory;
    
    @autoreleasepool
    {
        NSBitmapImageRep* rep = [[[NSBitmapImageRep alloc]
                    initWithBitmapDataPlanes:&buffer->memory
                    pixelsWide:buffer->width
                    pixelsHigh:buffer->height
                    bitsPerSample:8
                    samplesPerPixel:4
                    hasAlpha:YES
                    isPlanar:NO
                    colorSpaceName:NSDeviceRGBColorSpace
                    bytesPerRow:buffer->pitch
                    bitsPerPixel:buffer->bytesPerPixel * 8] autorelease];
        
        NSSize imageSize = NSMakeSize(buffer->width, buffer->height);
        NSImage* image = [[[NSImage alloc] initWithSize:imageSize] autorelease];
        [image addRepresentation:rep];
        window.contentView.layer.contents = image;
    }
}

internal void
MacResizeBackbuffer(MacOffscreenBuffer* buffer, s32 width, s32 height, NSWindow* window)
{
    const s32 BYTES_PER_PIXEL = 4;
    
    /*if (buffer->memory)
    {
        munmap(buffer->memory,
               buffer->width * buffer->height * buffer->bytesPerPixel);
    }*/
    
    //buffer->width = width;
    //buffer->height = height;
    buffer->width = window.contentView.bounds.size.width;
    buffer->height = window.contentView.bounds.size.height;
    buffer->pitch = buffer->width * BYTES_PER_PIXEL;
    buffer->bytesPerPixel = BYTES_PER_PIXEL;
    /*buffer->memory = mmap(0,
                          width * height * BYTES_PER_PIXEL,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1, 0);*/
    
    buffer->memory = (u8*)malloc(buffer->pitch * buffer->height);
}

int main(int argc, const char* argv[])
{
    @autoreleasepool
    {
        s32 renderWidth = 1280;
        s32 renderHeight = 720;
        
        NSRect screenRect = [[NSScreen mainScreen] frame];
        
        MacWindowDelegate* windowDelegate = [[MacWindowDelegate alloc] init];
        
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
        [window setDelegate:windowDelegate];
        [window setTitle:@"Handmade Game"];
        [window makeKeyAndOrderFront:nil];
        window.contentView.wantsLayer = YES;
        
        MacResizeBackbuffer(&globalBackbuffer, renderWidth, renderHeight, window);
        
        s32 xOffset = 0;
        s32 yOffset = 0;
        
        globalRunning = true;
        
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
            
            RenderGradient(&globalBackbuffer, xOffset++, yOffset);
            MacUpdateWindow(window, &globalBackbuffer);
        }
    }
}