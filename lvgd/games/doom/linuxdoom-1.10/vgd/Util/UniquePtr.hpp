#pragma once

#include <utility>

namespace vgd
{

template <typename T>
class UniquePtr
{
public:
    UniquePtr()
        : m_Pointer(nullptr)
    {
    }

    ~UniquePtr()
    {
        Reset();
    }

    UniquePtr(const UniquePtr &) = delete;

    UniquePtr &operator=(const UniquePtr &) = delete;

    UniquePtr(UniquePtr &&other) noexcept
        : m_Pointer(other.m_Pointer)
    {
        other.m_Pointer =
            nullptr;
    }

    UniquePtr &operator=(UniquePtr &&other) noexcept
    {
        if (this != &other)
        {
            Reset();

            m_Pointer =
                other.m_Pointer;

            other.m_Pointer =
                nullptr;
        }

        return *this;
    }

    template <typename U>
    UniquePtr(UniquePtr<U> &&other) noexcept
        : m_Pointer(other.Release())
    {
    }

    template <typename U>
    UniquePtr &operator=(UniquePtr<U> &&other) noexcept
    {
        Reset();

        m_Pointer =
            other.Release();

        return *this;
    }

    T *operator->() const
    {
        return m_Pointer;
    }

    T &operator*() const
    {
        return *m_Pointer;
    }

    T *Get() const
    {
        return m_Pointer;
    }

    bool IsValid() const
    {
        return m_Pointer != nullptr;
    }

    template <typename... Args>
    UniquePtr<T>&& MakeNewInstance(Args&&... args)
    {
        Reset();
        m_Pointer = new T(std::forward<Args>(args)...);
        return std::move(*this);
    }

    T *Release()
    {
        T *pointer =
            m_Pointer;

        m_Pointer =
            nullptr;

        return pointer;
    }

    void Reset()
    {
        if (m_Pointer != nullptr)
        {
            delete m_Pointer;
            m_Pointer =
                nullptr;
        }
    }

private:
    T *m_Pointer;
};

}