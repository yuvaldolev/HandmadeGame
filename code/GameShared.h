#if !defined(GAMESHARED_H)

struct FormatDest
{
    char* at,
    umm size;
};

internal umm
FormatStringList(char* dest, umm destSize, const char* format, va_list argList)
{
    FormatDest dest = { Dest, DestSize };

    if (dest.size)
    {
        const char* atFormat = format;

        while (atFormat[0])
        {
            if (*atFormat)
            {

            }
        }
    }
}

internal umm
FormatString(char* dest, umm destSize, const char* format, ...)
{
    va_list argList;

    va_start(argList, format);
    umm result = FormatStringList(dest, destSize, format, argList);
    va_end(argList);

    return result;
}

#define GAMESHARED_H
#endif

