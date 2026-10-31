#pragma once

#include <stdint.h>

namespace vgd
{
    class Joystick
    {
    public:
        static const int MaxButtons = 32;
        static const int MaxDevices = 16;

        Joystick();

        bool Initialize();
        void Shutdown();
        void Update();

        bool IsInitialized() const;

        int GetAxisX() const;
        int GetAxisY() const;
        int GetButtonCount() const;

        bool IsButtonDown(int button) const;

        const char* GetDeviceName() const;
        const char* GetDevicePath() const;

    private:
        int m_FileDescriptor;
        unsigned char m_AxisCount;
        unsigned char m_ButtonCount;
        int16_t m_AxisX;
        int16_t m_AxisY;

        bool m_Buttons[MaxButtons];
        char m_DeviceName[128];
        char m_DevicePath[32];

        bool TryOpenDevice(const char* path);
    };
}
