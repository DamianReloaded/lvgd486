
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits>
#include <new>
#include "FixedString.hpp"

namespace vgd
{

class String
{
public:
    String()
        : m_Buffer(new char[1]), m_Length(0), m_Capacity(1)
    {
        m_Buffer[0] = '\0';
    }

    String(const char* text)
        : m_Buffer(NULL), m_Length(0), m_Capacity(0)
    {
        Assign(text);
    }

    String(const String& other)
        : m_Buffer(NULL), m_Length(0), m_Capacity(0)
    {
        Assign(other.CStr());
    }

    String(String&& other) noexcept
        : m_Buffer(other.m_Buffer),
          m_Length(other.m_Length),
          m_Capacity(other.m_Capacity)
    {
        other.m_Buffer = NULL;
        other.m_Length = 0;
        other.m_Capacity = 0;
    }

    template <size_t OtherCapacity>
    String(const FixedString<OtherCapacity>& other)
        : m_Buffer(NULL), m_Length(0), m_Capacity(0)
    {
        Assign(other.CStr());
    }

    ~String()
    {
        delete[] m_Buffer;
    }

    String& operator=(const String& other)
    {
        if (this != &other)
        {
            Assign(other.CStr());
        }

        return *this;
    }

    String& operator=(String&& other) noexcept
    {
        if (this != &other)
        {
            delete[] m_Buffer;

            m_Buffer = other.m_Buffer;
            m_Length = other.m_Length;
            m_Capacity = other.m_Capacity;

            other.m_Buffer = NULL;
            other.m_Length = 0;
            other.m_Capacity = 0;
        }

        return *this;
    }

    template <size_t OtherCapacity>
    String& operator=(const FixedString<OtherCapacity>& other)
    {
        Assign(other.CStr());
        return *this;
    }

    String& operator=(const char* text)
    {
        Assign(text);
        return *this;
    }

    // Capacity and access

    size_t Length() const
    {
        return m_Length;
    }

    size_t BufferSize() const
    {
        return m_Capacity;
    }

    size_t MaxLength() const
    {
        return m_Capacity > 0 ? m_Capacity - 1 : 0;
    }

    bool Empty() const
    {
        return m_Length == 0;
    }

    const char* CStr() const
    {
        return m_Buffer != NULL ? m_Buffer : "";
    }

    char* Data()
    {
        EnsureCapacity(1);
        return m_Buffer;
    }

    const char* Data() const
    {
        return CStr();
    }

    operator const char*() const
    {
        return CStr();
    }

    char& operator[](size_t index)
    {
        EnsureCapacity(1);
        return m_Buffer[index];
    }

    const char& operator[](size_t index) const
    {
        return CStr()[index];
    }

    char& At(size_t index)
    {
        EnsureCapacity(1);
        return m_Buffer[index < m_Length ? index : m_Length];
    }

    const char& At(size_t index) const
    {
        return CStr()[index < m_Length ? index : m_Length];
    }

    // Assignment and modification

    String& Assign(const char* text)
    {
        if (text == NULL)
            text = "";

        if (IsInternal(text))
        {
            String temporary(text);
            return Assign(temporary.CStr());
        }

        size_t length = strlen(text);

        EnsureCapacity(length + 1);

        if (length > 0)
            memmove(m_Buffer, text, length);

        m_Buffer[length] = '\0';
        m_Length = length;

        return *this;
    }

    String& Clear()
    {
        m_Length = 0;

        if (m_Buffer != NULL)
            m_Buffer[0] = '\0';

        return *this;
    }

    String& Append(const char* text)
    {
        if (text == NULL)
            return *this;

        if (IsInternal(text))
        {
            String temporary(text);
            return Append(temporary.CStr());
        }

        size_t length = strlen(text);

        if (length == 0)
            return *this;

        EnsureCapacity(m_Length + length + 1);

        memmove(m_Buffer + m_Length, text, length);

        m_Length += length;
        m_Buffer[m_Length] = '\0';

        return *this;
    }

    String& Append(const String& other)
    {
        return Append(other.CStr());
    }

    template <size_t OtherCapacity>
    String& Append(const FixedString<OtherCapacity>& other)
    {
        return Append(other.CStr());
    }

    String& Append(char character)
    {
        EnsureCapacity(m_Length + 2);

        m_Buffer[m_Length++] = character;
        m_Buffer[m_Length] = '\0';

        return *this;
    }

    String& operator+=(const char* text)
    {
        return Append(text);
    }

    String& operator+=(const String& other)
    {
        return Append(other);
    }

    template <size_t OtherCapacity>
    String& operator+=(const FixedString<OtherCapacity>& other)
    {
        return Append(other);
    }

    String& operator+=(char character)
    {
        return Append(character);
    }

    String& Insert(size_t index, const char* text)
    {
        if (text == NULL || index > m_Length)
            return *this;

        if (IsInternal(text))
        {
            String temporary(text);
            return Insert(index, temporary.CStr());
        }

        size_t length = strlen(text);

        if (length == 0)
            return *this;

        EnsureCapacity(m_Length + length + 1);

        memmove(m_Buffer + index + length,
                m_Buffer + index,
                m_Length - index + 1);

        memmove(m_Buffer + index, text, length);

        m_Length += length;

        return *this;
    }

    String& Erase(size_t index, size_t count = 1)
    {
        if (index >= m_Length)
            return *this;

        if (count > m_Length - index)
            count = m_Length - index;

        memmove(m_Buffer + index,
                m_Buffer + index + count,
                m_Length - index - count + 1);

        m_Length -= count;

        return *this;
    }

    String& Replace(char oldCharacter, char newCharacter)
    {
        for (size_t i = 0; i < m_Length; ++i)
        {
            if (m_Buffer[i] == oldCharacter)
                m_Buffer[i] = newCharacter;
        }

        return *this;
    }

    String& Trim()
    {
        size_t start = 0;

        while (start < m_Length &&
               isspace(static_cast<unsigned char>(m_Buffer[start])))
        {
            ++start;
        }

        size_t end = m_Length;

        while (end > start &&
               isspace(static_cast<unsigned char>(m_Buffer[end - 1])))
        {
            --end;
        }

        m_Length = end - start;

        if (start > 0)
            memmove(m_Buffer, m_Buffer + start, m_Length);

        m_Buffer[m_Length] = '\0';

        return *this;
    }

    String& ToLower()
    {
        for (size_t i = 0; i < m_Length; ++i)
        {
            m_Buffer[i] = static_cast<char>(
                tolower(static_cast<unsigned char>(m_Buffer[i])));
        }

        return *this;
    }

    String& ToUpper()
    {
        for (size_t i = 0; i < m_Length; ++i)
        {
            m_Buffer[i] = static_cast<char>(
                toupper(static_cast<unsigned char>(m_Buffer[i])));
        }

        return *this;
    }

    // Searching

    size_t Find(char character, size_t start = 0) const
    {
        if (start >= m_Length)
            return NotFound;

        for (size_t i = start; i < m_Length; ++i)
        {
            if (m_Buffer[i] == character)
                return i;
        }

        return NotFound;
    }

    size_t Find(const char* text, size_t start = 0) const
    {
        if (text == NULL || start > m_Length)
            return NotFound;

        const char* result = strstr(CStr() + start, text);

        return result != NULL
            ? static_cast<size_t>(result - CStr())
            : NotFound;
    }

    bool StartsWith(const char* text) const
    {
        if (text == NULL)
            return false;

        size_t length = strlen(text);

        return length <= m_Length &&
               memcmp(CStr(), text, length) == 0;
    }

    bool EndsWith(const char* text) const
    {
        if (text == NULL)
            return false;

        size_t length = strlen(text);

        return length <= m_Length &&
               memcmp(CStr() + m_Length - length, text, length) == 0;
    }

    String Substring(size_t start, size_t count = NotFound) const
    {
        String result;

        if (start >= m_Length)
            return result;

        if (count > m_Length - start)
            count = m_Length - start;

        result.Assign(CStr() + start);

        if (result.m_Length > count)
        {
            result.m_Length = count;
            result.m_Buffer[count] = '\0';
        }

        return result;
    }

    static const size_t NotFound = static_cast<size_t>(-1);

    // Comparisons

    int Compare(const char* text) const
    {
        if (text == NULL)
            text = "";

        return strcmp(CStr(), text);
    }

    bool operator==(const char* text) const
    {
        return Compare(text) == 0;
    }

    bool operator!=(const char* text) const
    {
        return Compare(text) != 0;
    }

    bool operator<(const char* text) const
    {
        return Compare(text) < 0;
    }

    bool operator>(const char* text) const
    {
        return Compare(text) > 0;
    }

    bool operator<=(const char* text) const
    {
        return Compare(text) <= 0;
    }

    bool operator>=(const char* text) const
    {
        return Compare(text) >= 0;
    }

    bool operator==(const String& other) const
    {
        return Compare(other.CStr()) == 0;
    }

    bool operator!=(const String& other) const
    {
        return !(*this == other);
    }

    bool operator<(const String& other) const
    {
        return Compare(other.CStr()) < 0;
    }

    bool operator>(const String& other) const
    {
        return Compare(other.CStr()) > 0;
    }

    bool operator<=(const String& other) const
    {
        return Compare(other.CStr()) <= 0;
    }

    bool operator>=(const String& other) const
    {
        return Compare(other.CStr()) >= 0;
    }

    // Concatenation

    String operator+(const char* text) const
    {
        String result(*this);
        result.Append(text);
        return result;
    }

    String operator+(char character) const
    {
        String result(*this);
        result.Append(character);
        return result;
    }

    String operator+(const String& other) const
    {
        String result(*this);
        result.Append(other);
        return result;
    }

    // Conversion

    template <typename T>
    bool ToNumber(T& value) const
    {
        if (m_Length == 0)
            return false;

        char* end = NULL;
        errno = 0;

        if (std::numeric_limits<T>::is_integer)
        {
            if (std::numeric_limits<T>::is_signed)
            {
                const long long parsed = strtoll(CStr(), &end, 10);

                if (errno == ERANGE ||
                    end == CStr() ||
                    *end != '\0' ||
                    parsed < static_cast<long long>(
                        std::numeric_limits<T>::min()) ||
                    parsed > static_cast<long long>(
                        std::numeric_limits<T>::max()))
                {
                    return false;
                }

                value = static_cast<T>(parsed);
            }
            else
            {
                if (m_Buffer[0] == '-')
                    return false;

                const unsigned long long parsed =
                    strtoull(CStr(), &end, 10);

                if (errno == ERANGE ||
                    end == CStr() ||
                    *end != '\0' ||
                    parsed > static_cast<unsigned long long>(
                        std::numeric_limits<T>::max()))
                {
                    return false;
                }

                value = static_cast<T>(parsed);
            }

            return true;
        }

        if (std::numeric_limits<T>::is_iec559)
        {
            const double parsed = strtod(CStr(), &end);

            if (errno == ERANGE || end == CStr() || *end != '\0')
                return false;

            if (parsed < -static_cast<double>(
                            std::numeric_limits<T>::max()) ||
                parsed > static_cast<double>(
                            std::numeric_limits<T>::max()))
            {
                return false;
            }

            value = static_cast<T>(parsed);
            return true;
        }

        return false;
    }

private:
    char* m_Buffer;
    size_t m_Length;
    size_t m_Capacity;

    void EnsureCapacity(size_t required)
    {
        if (required <= m_Capacity)
            return;

        size_t newCapacity = m_Capacity > 0 ? m_Capacity : 1;

        while (newCapacity < required)
        {
            if (newCapacity > static_cast<size_t>(-1) / 2)
            {
                newCapacity = required;
                break;
            }

            newCapacity *= 2;
        }

        char* newBuffer = new char[newCapacity];

        if (m_Buffer != NULL)
            memcpy(newBuffer, m_Buffer, m_Length + 1);
        else
            newBuffer[0] = '\0';

        delete[] m_Buffer;

        m_Buffer = newBuffer;
        m_Capacity = newCapacity;
    }

    bool IsInternal(const char* text) const
    {
        if (m_Buffer == NULL || text == NULL)
            return false;

        uintptr_t address = reinterpret_cast<uintptr_t>(text);
        uintptr_t start = reinterpret_cast<uintptr_t>(m_Buffer);

        return address >= start && address - start < m_Capacity;
    }
};

// Support concatenation with a C string on the left.

inline String operator+(const char* left, const String& right)
{
    String result(left);
    result.Append(right);
    return result;
}

}