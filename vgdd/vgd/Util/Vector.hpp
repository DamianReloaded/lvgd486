#ifndef VGD_VECTOR_HPP
#define VGD_VECTOR_HPP

namespace vgd
{
    template <class T>
    class Vector
    {
    public:
        Vector() : Data(0), Count(0), Capacity(0) {}
        ~Vector() { delete[] Data; }

        Vector(const Vector& other) : Data(0), Count(0), Capacity(0)
        {
            if (other.Count > 0 && Reserve(other.Count))
            {
                for (int i = 0; i < other.Count; ++i) Data[i] = other.Data[i];
                Count = other.Count;
            }
        }

        Vector& operator=(const Vector& other)
        {
            if (this == &other) return *this;
            if (!Reserve(other.Count)) return *this;
            for (int i = 0; i < other.Count; ++i) Data[i] = other.Data[i];
            Count = other.Count;
            return *this;
        }

        bool Reserve(int wanted)
        {
            if (wanted <= Capacity) return true;
            int next = Capacity ? Capacity : 8;
            while (next < wanted)
            {
                if (next > 1073741823) return false;
                next *= 2;
            }
            T* replacement = new T[next];
            if (!replacement) return false;
            for (int i = 0; i < Count; ++i) replacement[i] = Data[i];
            delete[] Data;
            Data = replacement;
            Capacity = next;
            return true;
        }

        bool PushBack(const T& value)
        {
            if (!Reserve(Count + 1)) return false;
            Data[Count++] = value;
            return true;
        }

        void Clear() { Count = 0; }
        int Size() const { return Count; }
        bool Empty() const { return Count == 0; }
        T* Get(int index) { return index < 0 || index >= Count ? 0 : &Data[index]; }
        const T* Get(int index) const { return index < 0 || index >= Count ? 0 : &Data[index]; }
        T& operator[](int index) { return Data[index]; }
        const T& operator[](int index) const { return Data[index]; }

    private:
        T* Data;
        int Count;
        int Capacity;
    };
}

#endif
