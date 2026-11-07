
#pragma once

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "Array.hpp"
#include "FixedString.hpp"

namespace vgd
{

template <int MaxProperties, size_t PropertyCapacity>
class Entry
{
public:
    Array<FixedString<PropertyCapacity>, MaxProperties> m_Entries;

    Entry()
    {
        m_Entries.Resize(MaxProperties);
    }

    Entry(const Entry&) = default;
    Entry& operator=(const Entry&) = default;

    const FixedString<PropertyCapacity> &GetProperty(int index) const
    {
        return m_Entries[index];
    }

    FixedString<PropertyCapacity> &GetProperty(int index)
    {
        return m_Entries[index];
    }
};

template <int MaxEntries, int MaxProperties, size_t PropertyCapacity>
class PropertyFile
{
    static_assert(MaxEntries > 0, "MaxEntries must be greater than zero");
    static_assert(MaxProperties > 0, "MaxProperties must be greater than zero");
    static_assert(PropertyCapacity > 1, "PropertyCapacity must be greater than one");

public:
    typedef Entry<MaxProperties, PropertyCapacity> EntryType;

    Array<EntryType, MaxEntries> m_Entries;

    int m_SeekPos;
    int m_EntryCount;

    PropertyFile()
        : m_File(NULL),
          m_SeekPos(0),
          m_EntryCount(0),
          m_GroupStartPos(-1),
          m_FileSize(0)
    {
    }

    ~PropertyFile()
    {
        Close();
    }

    PropertyFile(const PropertyFile &) = delete;
    PropertyFile &operator=(const PropertyFile &) = delete;

    bool Open(const char *path)
    {
        Close();

        if (path == NULL)
            return false;

        m_File = fopen(path, "rb");

        if (m_File == NULL)
            return false;

        if (fseek(m_File, 0, SEEK_END) != 0)
        {
            Close();
            return false;
        }

        long fileSize = ftell(m_File);

        if (fileSize < 0 || fileSize > INT_MAX)
        {
            Close();
            return false;
        }

        m_FileSize = static_cast<int>(fileSize);
        m_SeekPos = 0;
        m_EntryCount = 0;
        m_GroupStartPos = -1;

        m_Entries.Clear();

        return true;
    }

    void Close()
    {
        if (m_File != NULL)
        {
            fclose(m_File);
            m_File = NULL;
        }

        m_SeekPos = 0;
        m_EntryCount = 0;
        m_GroupStartPos = -1;
        m_FileSize = 0;

        m_Entries.Clear();
    }

    bool IsOpen() const
    {
        return m_File != NULL;
    }

    bool LoadNextGroup()
    {
        if (m_File == NULL || m_SeekPos < 0 || m_SeekPos > m_FileSize)
            return false;

        if (fseek(m_File, m_SeekPos, SEEK_SET) != 0)
            return false;

        m_Entries.Clear();
        m_Entries.Resize(MaxEntries);
        m_EntryCount = 0;

        const int groupStart = m_SeekPos;

        while (m_EntryCount < MaxEntries)
        {
            EntryType &entry = m_Entries[m_EntryCount];
            int result = ReadEntry(entry);

            if (result == 0)
            {
                if (m_EntryCount == 0)
                {
                    m_GroupStartPos = -1;
                    return false;
                }

                break;
            }

            if (result < 0)
                return Fail();

            ++m_EntryCount;

            if (!GetPosition(m_SeekPos))
                return Fail();
        }

        m_Entries.Resize(m_EntryCount);
        m_GroupStartPos = groupStart;

        return m_EntryCount > 0;
    }

    bool LoadPreviousGroup()
    {
        if (m_File == NULL || m_GroupStartPos <= 0)
            return false;

        int previousGroup = -1;

        if (!FindPreviousGroup(m_GroupStartPos, previousGroup))
            return false;

        m_SeekPos = previousGroup;

        return LoadNextGroup();
    }

    int GetEntryCount() const
    {
        return m_EntryCount;
    }

    int GetSeekPosition() const
    {
        return m_SeekPos;
    }

    const EntryType &GetEntry(int index) const
    {
        return m_Entries[index];
    }

    EntryType &GetEntry(int index)
    {
        return m_Entries[index];
    }

private:
    FILE *m_File;

    int m_GroupStartPos;
    int m_FileSize;

    // Returns:
    //  1 = line read successfully
    //  0 = EOF
    // -1 = line too long or read error

    int ReadLine(char *buffer, size_t capacity)
    {
        if (fgets(buffer, static_cast<int>(capacity), m_File) == NULL)
            return feof(m_File) ? 0 : -1;

        size_t length = strlen(buffer);

        bool terminated = length > 0 && buffer[length - 1] == '\n';

        if (!terminated && !feof(m_File))
        {
            int c = fgetc(m_File);

            if (c != '\n' && c != EOF)
            {
                while (c != '\n' && c != EOF)
                    c = fgetc(m_File);

                return -1;
            }
        }

        while (length > 0 &&
               (buffer[length - 1] == '\n' ||
                buffer[length - 1] == '\r'))
        {
            buffer[--length] = '\0';
        }

        return 1;
    }

    // Reads a property value, ignoring the property name.
    // Example: Title: Doom -> Doom
    //
    // Blank lines are skipped.
    //
    // Returns:
    //  1 = success
    //  0 = EOF
    // -1 = invalid line or oversized value

    int ReadProperty(FixedString<PropertyCapacity> &value)
    {
        char buffer[PropertyCapacity + 1];

        while (true)
        {
            int result = ReadLine(buffer, sizeof(buffer));

            if (result != 1)
                return result;

            char *line = buffer;

            while (*line != '\0' &&
                   isspace(static_cast<unsigned char>(*line)))
            {
                ++line;
            }

            if (*line == '\0')
                continue;

            char *separator = strchr(line, ':');

            if (separator == NULL)
                return -1;

            char *text = separator + 1;

            while (*text != '\0' &&
                   isspace(static_cast<unsigned char>(*text)))
            {
                ++text;
            }

            size_t length = strlen(text);

            while (length > 0 &&
                   isspace(static_cast<unsigned char>(text[length - 1])))
            {
                --length;
            }

            if (length >= PropertyCapacity)
                return -1;

            value.Clear();

            memcpy(value.Data(), text, length);
            value.Data()[length] = '\0';

            return 1;
        }
    }

    // Returns:
    //  1 = complete entry
    //  0 = clean EOF before an entry starts
    // -1 = incomplete or malformed entry
    int ReadEntry(EntryType &entry)
    {
        for (int i = 0; i < MaxProperties; ++i)
        {
            int result = ReadProperty(entry.GetProperty(i));

            if (result < 0)
                return -1;

            if (result == 0)
                return i == 0 ? 0 : -1;
        }

        return 1;
    }

    bool GetPosition(int &position)
    {
        long result = ftell(m_File);

        if (result < 0 || result > INT_MAX)
            return false;

        position = static_cast<int>(result);

        return true;
    }

    bool Fail()
    {
        m_Entries.Clear();
        m_EntryCount = 0;
        m_GroupStartPos = -1;

        return false;
    }

    // Finds the beginning of the group immediately preceding target.

    bool FindPreviousGroup(int target, int &previous)
    {
        if (fseek(m_File, 0, SEEK_SET) != 0)
            return false;

        int groupStart = 0;
        previous = -1;

        while (groupStart < target)
        {
            previous = groupStart;

            int entriesRead = 0;

            while (entriesRead < MaxEntries)
            {
                int entryStart = 0;

                if (!GetPosition(entryStart))
                    return false;

                EntryType temporary;

                if (!ReadEntry(temporary))
                {
                    if (feof(m_File) && entriesRead == 0)
                        return false;

                    return false;
                }

                ++entriesRead;
            }

            if (!GetPosition(groupStart))
                return false;
        }

        return previous >= 0 && previous < target;
    }
};

}
