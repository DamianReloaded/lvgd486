#ifdef CUSTOM_DOS_ALLOCATOR

#include <stddef.h>
#include <stdlib.h>

void *operator new(size_t size)
{
    void *pointer = malloc(size);

    if (pointer == nullptr)
    {
        abort();
    }

    return pointer;
}

void *operator new[](size_t size)
{
    void *pointer = malloc(size);

    if (pointer == nullptr)
    {
        abort();
    }

    return pointer;
}

void operator delete(void *pointer)
{
    free(pointer);
}

void operator delete[](void *pointer)
{
    free(pointer);
}

void operator delete(void *pointer, size_t)
{
    free(pointer);
}

void operator delete[](void *pointer, size_t)
{
    free(pointer);
}

#endif