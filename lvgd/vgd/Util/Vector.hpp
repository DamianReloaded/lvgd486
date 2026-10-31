#ifndef VGD_VECTOR_HPP
#define VGD_VECTOR_HPP

//#include <new>

namespace vgd
{
    template <class T>
    class Vector
    {
    public:
        Vector() : m_Data(0), m_Size(0), m_Capacity(0) {}
        ~Vector() { delete[] m_Data; }

        Vector(const Vector& other) : m_Data(0), m_Size(0), m_Capacity(0)
        {
            if (other.m_Size > 0 && Reserve(other.m_Size))
            {
                for (int i = 0; i < other.m_Size; ++i)
                    m_Data[i] = other.m_Data[i];

                m_Size = other.m_Size;
            }
        }

        Vector& operator=(const Vector& other)
        {
            if (this == &other)
                return *this;

            if (!Reserve(other.m_Size))
                return *this;

            for (int i = 0; i < other.m_Size; ++i)
                m_Data[i] = other.m_Data[i];

            m_Size = other.m_Size;
            return *this;
        }

        bool Assign(const T* begin, const T* end)
        {          
            int size = end-begin;

            if (!Reserve(size))
                return false;

            for (int i = 0; i < size; ++i)
                m_Data[i] = *(begin+i);

            m_Size = size;
            return true;
        }

        bool Reserve(int wanted)
        {
            if (wanted <= m_Capacity)
                return true;

            int next = m_Capacity ? m_Capacity : 8;

            while (next < wanted)
            {
                if (next > 1073741823)
                    return false;

                next *= 2;
            }

            T* replacement = new () T[next];

            if (!replacement)
                return false;

            for (int i = 0; i < m_Size; ++i)
                replacement[i] = m_Data[i];

            delete[] m_Data;
            m_Data = replacement;
            m_Capacity = next;

            return true;
        }

        bool Resize(int wanted)
        {
            if (wanted < 0)
                return false;

            if (!Reserve(wanted))
                return false;

            for (int i = m_Size; i < wanted; ++i)
                m_Data[i] = T();

            m_Size = wanted;
            return true;
        }

        bool PushBack(const T& value)
        {
            if (!Reserve(m_Size + 1))
                return false;

            m_Data[m_Size++] = value;
            return true;
        }

        void Clear() { m_Size = 0; }
        int Size() const { return m_Size; }
        bool Empty() const { return m_Size == 0; }

        T* Get(int index)
        {
            return index < 0 || index >= m_Size ? 0 : &m_Data[index];
        }

        const T* Get(int index) const
        {
            return index < 0 || index >= m_Size ? 0 : &m_Data[index];
        }

        T& operator[](int index) { return m_Data[index]; }
        const T& operator[](int index) const { return m_Data[index]; }

    private:
        T* m_Data;
        int m_Size;
        int m_Capacity;
    };
}

#endif