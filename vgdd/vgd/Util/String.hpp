#pragma once
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <limits>

template <size_t Capacity>
class String
{
    static_assert(Capacity > 0, "String capacity must be greater than zero");

public:
    String()
        : m_Length(0)
    {
        m_Buffer[0] = '\0';
    }

    String(const char* text)
        : m_Length(0)
    {
        m_Buffer[0] = '\0';
        Assign(text);
    }

    String(const String& other)
        : m_Length(other.m_Length)
    {
        memcpy(m_Buffer, other.m_Buffer, m_Length + 1);
    }

    template <size_t OtherCapacity>
    String(const String<OtherCapacity>& other)
        : m_Length(0)
    {
        m_Buffer[0] = '\0';
        Assign(other.CStr());
    }

    String& operator=(const String& other)
    {
        if (this != &other)
        {
            m_Length = other.m_Length;
            memcpy(m_Buffer, other.m_Buffer, m_Length + 1);
        }

        return *this;
    }

    template <size_t OtherCapacity>
    String& operator=(const String<OtherCapacity>& other)
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

    static constexpr size_t BufferSize()
    {
        return Capacity;
    }

    static constexpr size_t MaxLength()
    {
        return Capacity - 1;
    }

    bool Empty() const
    {
        return m_Length == 0;
    }

    const char* CStr() const
    {
        return m_Buffer;
    }

    char* Data()
    {
        return m_Buffer;
    }

    const char* Data() const
    {
        return m_Buffer;
    }

    operator const char*() const
    {
        return m_Buffer;
    }

    char& operator[](size_t index)
    {
        return m_Buffer[index];
    }

    const char& operator[](size_t index) const
    {
        return m_Buffer[index];
    }

    char& At(size_t index)
    {
        return m_Buffer[index < m_Length ? index : m_Length];
    }

    const char& At(size_t index) const
    {
        return m_Buffer[index < m_Length ? index : m_Length];
    }

    // Assignment and modification

    String& Assign(const char* text)
    {
        if (text == NULL)
            text = "";

        size_t length = strlen(text);

        if (length >= Capacity)
            length = Capacity - 1;

        memmove(m_Buffer, text, length);
        m_Buffer[length] = '\0';
        m_Length = length;

        return *this;
    }

    String& Clear()
    {
        m_Length = 0;
        m_Buffer[0] = '\0';
        return *this;
    }

    String& Append(const char* text)
    {
        if (text == NULL)
            return *this;

        size_t length = strlen(text);
        size_t available = Capacity - 1 - m_Length;

        if (length > available)
            length = available;

        memmove(m_Buffer + m_Length, text, length);
        m_Length += length;
        m_Buffer[m_Length] = '\0';

        return *this;
    }

    template <size_t OtherCapacity>
    String& Append(const String<OtherCapacity>& other)
    {
        return Append(other.CStr());
    }

    String& Append(char character)
    {
        if (m_Length < Capacity - 1)
        {
            m_Buffer[m_Length++] = character;
            m_Buffer[m_Length] = '\0';
        }

        return *this;
    }

    String& operator+=(const char* text)
    {
        return Append(text);
    }

    template <size_t OtherCapacity>
    String& operator+=(const String<OtherCapacity>& other)
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

        char temporary[Capacity];
        size_t length = strlen(text);

        if (length > Capacity - 1 - m_Length)
            length = Capacity - 1 - m_Length;

        // Preserve the source if it points into this String.
        if (text >= m_Buffer && text < m_Buffer + Capacity)
        {
            size_t sourceLength = strlen(text);

            if (sourceLength >= Capacity)
                sourceLength = Capacity - 1;

            memcpy(temporary, text, sourceLength);
            temporary[sourceLength] = '\0';
            text = temporary;

            if (length > sourceLength)
                length = sourceLength;
        }

        memmove(m_Buffer + index + length,
                m_Buffer + index, m_Length - index + 1);

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

        const char* result = strstr(m_Buffer + start, text);

        return result != NULL
            ? static_cast<size_t>(result - m_Buffer)
            : NotFound;
    }

    bool StartsWith(const char* text) const
    {
        if (text == NULL)
            return false;

        size_t length = strlen(text);

        return length <= m_Length &&
               memcmp(m_Buffer, text, length) == 0;
    }

    bool EndsWith(const char* text) const
    {
        if (text == NULL)
            return false;

        size_t length = strlen(text);

        return length <= m_Length &&
               memcmp(m_Buffer + m_Length - length,
                      text, length) == 0;
    }

    String Substring(size_t start, size_t count = Capacity - 1) const
    {
        String result;

        if (start >= m_Length)
            return result;

        if (count > m_Length - start)
            count = m_Length - start;

        result.Assign(m_Buffer + start);

        if (result.m_Length > count)
        {
            result.m_Length = count;
            result.m_Buffer[count] = '\0';
        }

        return result;
    }

    static constexpr size_t NotFound = static_cast<size_t>(-1);

    // Comparisons

    int Compare(const char* text) const
    {
        if (text == NULL)
            text = "";

        return strcmp(m_Buffer, text);
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

    template <size_t OtherCapacity>
    bool operator==(const String<OtherCapacity>& other) const
    {
        return Compare(other.CStr()) == 0;
    }

    template <size_t OtherCapacity>
    bool operator!=(const String<OtherCapacity>& other) const
    {
        return !(*this == other);
    }

    template <size_t OtherCapacity>
    bool operator<(const String<OtherCapacity>& other) const
    {
        return Compare(other.CStr()) < 0;
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

    template <size_t OtherCapacity>
    String operator+(const String<OtherCapacity>& other) const
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
        {
            return false;
        }

        char* end = NULL;
        errno = 0;

        if (std::numeric_limits<T>::is_integer)
        {
            if (std::numeric_limits<T>::is_signed)
            {
                const long long parsed = strtoll(m_Buffer, &end, 10);

                if (errno == ERANGE ||
                    end == m_Buffer ||
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
                {
                    return false;
                }

                const unsigned long long parsed =
                    strtoull(m_Buffer, &end, 10);

                if (errno == ERANGE ||
                    end == m_Buffer ||
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
            const double parsed = strtod(m_Buffer, &end);

            if (errno == ERANGE || end == m_Buffer || *end != '\0')
            {
                return false;
            }

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
    char m_Buffer[Capacity];
    size_t m_Length;
};

// Support concatenation with a C string on the left.

template <size_t Capacity>
String<Capacity> operator+(const char* left, const String<Capacity>& right)
{
    String<Capacity> result(left);
    result.Append(right);
    return result;
}
