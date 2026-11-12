
#include "util.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

void NORETURN Die(const char* error)
{
    fprintf(stderr, "Error: %s\n", error);
    exit(1);
}

static inline int ToLower(int value)
{
    return value + ((value >= 'A' && value <= 'Z') ? 32 : 0);
}

char *NextDelim(char **s, int sep)
{
    if (s == NULL || *s == NULL)
        return NULL;

    char *r = *s;

    while (*r == ' ' || *r == '\t')
        r++;

    char *t = strchr(r, sep);
    *s = t ? ( *t++ = 0, t ) : NULL;

    return r;
}

bool StringEqualsNoCase(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return a == b;

    for (;;)
    {
        int aa = ToLower((unsigned char)*a++);
        int bb = ToLower((unsigned char)*b++);

        if (aa != bb)
            return false;

        if (aa == 0)
            return true;
    }
}

const char *StringStartsWithNoCase(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
        return NULL;

    for (;; a++, b++)
    {
        int aa = ToLower((unsigned char)*a);
        int bb = ToLower((unsigned char)*b);

        if (bb == 0)
            return a;

        if (aa != bb)
            return NULL;
    }
}

uint8 *ReadWholeFile(const char *name, size_t *length)
{
    FILE *f = fopen(name, "rb");

    if (f == NULL)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long fileSize = ftell(f);

    if (fileSize < 0 || (unsigned long)fileSize >= SIZE_MAX)
    {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }

    size_t size = (size_t)fileSize;
    uint8 *buffer = (uint8 *)malloc(size + 1);

    if (buffer == NULL)
    {
        fclose(f);
        Die("malloc failed");
    }

    if (fread(buffer, 1, size, f) != size)
    {
        free(buffer);
        fclose(f);
        return NULL;
    }

    fclose(f);

    buffer[size] = 0;

    if (length != NULL)
        *length = size;

    return buffer;
}

char *NextLineStripComments(char **s)
{
    if (s == NULL || *s == NULL)
        return NULL;

    char *p = *s;
    char *eol = strchr(p, '\n');

    *s = eol ? eol + 1 : NULL;
    eol = eol ? eol : p + strlen(p);

    char *comment = (char *)memchr(p, '#', (size_t)(eol - p));
    eol = comment ? comment : eol;

    while (eol > p && (eol[-1] == '\r' || eol[-1] == ' ' || eol[-1] == '\t'))
        eol--;

    *eol = 0;

    while (*p == ' ' || *p == '\t')
        p++;

    return p;
}

char *NextPossiblyQuotedString(char **s)
{
    if (s == NULL || *s == NULL)
        return NULL;

    char *r = *s;
    char *t;

    while (*r == ' ' || *r == '\t')
        r++;

    if (*r == '"')
    {
        for (t = ++r; *t && *t != '"'; t++)
            ;
    }
    else
    {
        for (t = r; *t && *t != ' ' && *t != '\t'; t++)
            ;
    }

    if (*t)
        *t++ = 0;

    while (*t == ' ' || *t == '\t')
        t++;

    *s = t;

    return r;
}

char *ReplaceFilenameWithNewPath(const char *old_path, const char *new_path)
{
    if (old_path == NULL || new_path == NULL)
        return NULL;

    size_t olen = strlen(old_path);
    size_t nlen = strlen(new_path) + 1;

    while (olen && old_path[olen - 1] != '/' && old_path[olen - 1] != '\\')
        olen--;

    if (olen > SIZE_MAX - nlen)
        return NULL;

    char *result = (char *)malloc(olen + nlen);

    if (result == NULL)
        return NULL;

    memcpy(result, old_path, olen);
    memcpy(result + olen, new_path, nlen);

    return result;
}

char *SplitKeyValue(char *p)
{
    if (p == NULL)
        return NULL;

    char *equals = strchr(p, '=');

    if (equals == NULL)
        return NULL;

    char *kr = equals;

    while (kr > p && (kr[-1] == ' ' || kr[-1] == '\t'))
        kr--;

    *kr = 0;

    char *v = equals + 1;

    while (*v == ' ' || *v == '\t')
        v++;

    return v;
}

const char *SkipPrefix(const char *big, const char *little)
{
    if (big == NULL || little == NULL)
        return NULL;

    for (; *little; big++, little++)
    {
        if (*little != *big)
            return NULL;
    }

    return big;
}

void StrSet(char **rv, const char *s)
{
    if (rv == NULL)
        return;

    char *news = s ? strdup(s) : NULL;
    char *old = *rv;

    *rv = news;
    free(old);
}

char *StrFmt(const char *fmt, ...)
{
    if (fmt == NULL)
        return NULL;

    char buf[4096];

    va_list va;
    va_start(va, fmt);

    int n = vsnprintf(buf, sizeof(buf), fmt, va);

    va_end(va);

    if (n < 0 || (size_t)n >= sizeof(buf))
        Die("vsnprintf failed");

    return strdup(buf);
}

void ByteArray_Resize(ByteArray *arr, size_t new_size)
{
    if (arr == NULL)
        return;

    if (new_size > arr->capacity)
    {
        size_t minsize = arr->capacity + (arr->capacity >> 1) + 8;
        size_t new_capacity = new_size < minsize ? minsize : new_size;

        void *data = realloc(arr->data, new_capacity);

        if (data == NULL)
            Die("memory allocation failed");

        arr->data = (uint8 *)data;
        arr->capacity = new_capacity;
    }

    arr->size = new_size;
}

void ByteArray_Destroy(ByteArray *arr)
{
    if (arr == NULL)
        return;

    free(arr->data);

    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

void ByteArray_AppendData(ByteArray *arr, const uint8 *data, size_t data_size)
{
    if (arr == NULL || (data == NULL && data_size != 0))
        return;

    if (data_size > SIZE_MAX - arr->size)
        Die("ByteArray size overflow");

    size_t old_size = arr->size;

    ByteArray_Resize(arr, old_size + data_size);

    if (data_size != 0)
        memcpy(arr->data + old_size, data, data_size);
}

void ByteArray_AppendByte(ByteArray *arr, uint8 value)
{
    if (arr == NULL)
        return;

    if (arr->size == SIZE_MAX)
        Die("ByteArray size overflow");

    ByteArray_Resize(arr, arr->size + 1);
    arr->data[arr->size - 1] = value;
}

MemBlk FindIndexInMemblk(MemBlk data, size_t i)
{
    if (data.size < 2 || data.ptr == NULL)
        return (MemBlk){ 0, 0 };

    size_t end = data.size - 2;
    size_t left_off, right_off;
    size_t mx = *(uint16 *)(data.ptr + end);

    if (mx < 8192)
    {
        if (i > mx || mx * 2 > end)
            return (MemBlk){ 0, 0 };

        left_off = (i == 0) ? mx * 2 : mx * 2 + *(uint16 *)(data.ptr + i * 2 - 2);
        right_off = (i == mx) ? end : mx * 2 + *(uint16 *)(data.ptr + i * 2);
    }
    else
    {
        mx -= 8192;

        if (i > mx || mx * 4 > end)
            return (MemBlk){ 0, 0 };

        left_off = (i == 0) ? mx * 4 : mx * 4 + *(uint32 *)(data.ptr + i * 4 - 4);
        right_off = (i == mx) ? end : mx * 4 + *(uint32 *)(data.ptr + i * 4);
    }

    if (left_off > right_off || right_off > end)
        return (MemBlk){ 0, 0 };

    return (MemBlk){ data.ptr + left_off, right_off - left_off };
}

static uint64 BpsDecodeInt(const uint8 **src)
{
    uint64 data = 0;
    uint64 shift = 1;

    for (;;)
    {
        uint8 x = *(*src)++;

        data += (x & 0x7f) * shift;

        if (x & 0x80)
            break;

        shift <<= 7;
        data += shift;
    }

    return data;
}

#define CRC32_POLYNOMIAL 0xEDB88320U

static uint32 crc32(const void *data, size_t length)
{
    uint32 crc = 0xFFFFFFFFU;
    const uint8 *byteData = (const uint8 *)data;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= byteData[i];

        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ ((crc & 1) * CRC32_POLYNOMIAL);
    }

    return crc ^ 0xFFFFFFFFU;
}

static uint32 ReadLe32(const uint8 *p)
{
    return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

uint8 *ApplyBps(const uint8 *src, size_t src_size_in, const uint8 *bps, size_t bps_size, size_t *length_out)
{
    if (src == NULL || bps == NULL || length_out == NULL || bps_size < 16)
        return NULL;

    if (memcmp(bps, "BPS1", 4) != 0)
        return NULL;

    const uint8 *bps_end = bps + bps_size - 12;

    if (crc32(src, src_size_in) != ReadLe32(bps_end))
        return NULL;

    if (crc32(bps, bps_size - 4) != ReadLe32(bps + bps_size - 4))
        return NULL;

    const uint8 *cursor = bps + 4;

    uint64 src_size = BpsDecodeInt(&cursor);
    uint64 dst_size = BpsDecodeInt(&cursor);
    uint64 meta_size = BpsDecodeInt(&cursor);

    if (src_size != src_size_in || dst_size > SIZE_MAX || meta_size > (uint64)(bps_end - cursor))
        return NULL;

    cursor += (size_t)meta_size;

    size_t outputOffset = 0;
    int64_t sourceRelativeOffset = 0;
    int64_t targetRelativeOffset = 0;

    uint8 *dst = (uint8 *)malloc((size_t)dst_size);

    if (dst == NULL && dst_size != 0)
        return NULL;

    while (cursor < bps_end)
    {
        uint64 command = BpsDecodeInt(&cursor);
        uint64 length64 = (command >> 2) + 1;

        if (length64 > dst_size - outputOffset)
        {
            free(dst);
            return NULL;
        }

        size_t length = (size_t)length64;

        switch (command & 3)
        {
        case 0:
            if (outputOffset + length > src_size_in)
            {
                free(dst);
                return NULL;
            }

            memcpy(dst + outputOffset, src + outputOffset, length);
            outputOffset += length;
            break;

        case 1:
            if ((size_t)(bps_end - cursor) < length)
            {
                free(dst);
                return NULL;
            }

            memcpy(dst + outputOffset, cursor, length);
            cursor += length;
            outputOffset += length;
            break;

        case 2:
        {
            uint64 offset = BpsDecodeInt(&cursor);
            int64_t delta = (int64_t)(offset >> 1);

            sourceRelativeOffset += (offset & 1) ? -delta : delta;

            if (sourceRelativeOffset < 0 || (uint64)sourceRelativeOffset + length > src_size_in)
            {
                free(dst);
                return NULL;
            }

            memcpy(dst + outputOffset, src + sourceRelativeOffset, length);
            sourceRelativeOffset += length;
            outputOffset += length;
            break;
        }

        default:
        {
            uint64 offset = BpsDecodeInt(&cursor);
            int64_t delta = (int64_t)(offset >> 1);

            targetRelativeOffset += (offset & 1) ? -delta : delta;

            if (targetRelativeOffset < 0 || (uint64)targetRelativeOffset >= outputOffset)
            {
                free(dst);
                return NULL;
            }

            for (size_t i = 0; i < length; i++)
            {
                if ((uint64)targetRelativeOffset >= outputOffset + i)
                {
                    free(dst);
                    return NULL;
                }

                dst[outputOffset++] = dst[targetRelativeOffset++];
            }

            break;
        }
        }
    }

    if (outputOffset != dst_size || crc32(dst, (size_t)dst_size) != ReadLe32(bps_end + 4))
    {
        free(dst);
        return NULL;
    }

    *length_out = (size_t)dst_size;

    return dst;
}
