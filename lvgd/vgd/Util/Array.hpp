#pragma once

#include <new>

namespace vgd
{

template <typename T, int Size>
class Array
{
    static_assert(Size > 0, "Array size must be greater than zero");

public:
    Array()
        : m_Count(0)
    {
    }

    Array(const Array &) = delete;
    Array &operator=(const Array &) = delete;

    bool Resize(int count)
    {
        if (count < 0 || count > Size)
        {
            return false;
        }

        if (count > m_Count)
        {
            for (int i = m_Count; i < count; i++)
            {
                new (&m_Data[i]) T();
            }
        }

        m_Count = count;
        return true;
    }

    void Clear()
    {
        m_Count = 0;
    }

    T *GetData()
    {
        return m_Data;
    }

    const T *GetData() const
    {
        return m_Data;
    }

    int GetCount() const
    {
        return m_Count;
    }

    static constexpr int GetCapacity()
    {
        return Size;
    }

    T &operator[](int index)
    {
        return m_Data[index];
    }

    const T &operator[](int index) const
    {
        return m_Data[index];
    }

private:
    T m_Data[Size];
    int m_Count;
};

}
