#if !defined(GAME_ASSERT_H)

#if GAME_SLOW
#define Assert_(expression, shouldCrash) \
if (!(expression)) \
{ \
    AssertionFailure_(#expression, __FILE__, __FUNCTION__, __LINE__, shouldCrash); \
}

#define Assert(expression) Assert_((expression), true)
#define SoftAssert(expression) Assert_((expression), false)

// TODO(yuval, eran): Temporary
#include <stdio.h>

internal void
AssertionFailure_(const char* condition, const char* file,
                  const char* function, long line, b32 shouldCrash)
{
    char message[256];
    snprintf(message, sizeof(message), "Assertion Failure: %s at %s:%s:%ld",
             condition, file, function, line);
    
    platform.DisplayMessageBox("ASSERSION FAILURE", message);
    
    if (shouldCrash)
    {
        *(volatile int*)0 = 0;
    }
}
#else
#define Assert(expression)
#define SoftAssert(expression)
#endif

#define GAME_ASSERT_H
#endif