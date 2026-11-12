#pragma once

#include <stdint.h>

namespace vgd
{

class Joystick
{
public:
    static const int MaxButtons = 32;
    static const int MaxDevices = 16;

    enum Direction
    {
        DirectionUp = 0,
        DirectionDown,
        DirectionLeft,
        DirectionRight,
        DirectionCount
    };

    Joystick();

    bool Initialize();
    void Shutdown();
    void Update();

    bool IsInitialized() const;

    int GetAxisX() const;
    int GetAxisY() const;
    int GetButtonCount() const;

    bool IsButtonPressed(int button) const;
    bool WasButtonPressed(int button) const;
    bool WasButtonReleased(int button) const;

    bool IsDirectionPressed(Direction direction) const;
    bool WasDirectionPressed(Direction direction) const;
    bool WasDirectionReleased(Direction direction) const;

    void SetDeadZone(int deadZone);
    int GetDeadZone() const;

    void SetDirectionButtonMapping(Direction direction, int button);

    const char* GetDeviceName() const;
    const char* GetDevicePath() const;

private:
    int m_FileDescriptor;

    unsigned char m_AxisCount;
    unsigned char m_ButtonCount;

    int16_t m_AxisX;
    int16_t m_AxisY;
    int16_t m_HatX;
    int16_t m_HatY;

    int m_HatAxisX;
    int m_HatAxisY;
    int m_DeadZone;

    bool m_Buttons[MaxButtons];
    bool m_ButtonPressed[MaxButtons];
    bool m_ButtonReleased[MaxButtons];

    bool m_Directions[DirectionCount];
    bool m_DirectionPressed[DirectionCount];
    bool m_DirectionReleased[DirectionCount];

    int m_DirectionButtons[DirectionCount];

    char m_DeviceName[128];
    char m_DevicePath[32];

    bool TryOpenDevice(const char* path);
    void UpdateDirections(bool suppressEvents);
};

}