
#include "Joystick.hpp"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>

namespace vgd
{

Joystick::Joystick()
    : m_FileDescriptor(-1),
      m_AxisCount(0),
      m_ButtonCount(0),
      m_AxisX(0),
      m_AxisY(0),
      m_HatX(0),
      m_HatY(0),
      m_HatAxisX(-1),
      m_HatAxisY(-1),
      m_DeadZone(6000)
{
    memset(m_Buttons, 0, sizeof(m_Buttons));
    memset(m_ButtonPressed, 0, sizeof(m_ButtonPressed));
    memset(m_ButtonReleased, 0, sizeof(m_ButtonReleased));

    memset(m_Directions, 0, sizeof(m_Directions));
    memset(m_DirectionPressed, 0, sizeof(m_DirectionPressed));
    memset(m_DirectionReleased, 0, sizeof(m_DirectionReleased));

    for (int i = 0; i < DirectionCount; ++i)
        m_DirectionButtons[i] = -1;

    memset(m_DeviceName, 0, sizeof(m_DeviceName));
    memset(m_DevicePath, 0, sizeof(m_DevicePath));
}

bool Joystick::Initialize()
{
    Shutdown();

    char path[32];

    for (int i = 0; i < MaxDevices; ++i)
    {
        snprintf(path, sizeof(path), "/dev/input/js%d", i);

        if (TryOpenDevice(path))
            return true;

        snprintf(path, sizeof(path), "/dev/js%d", i);

        if (TryOpenDevice(path))
            return true;
    }

    fprintf(stderr, "Joystick: no usable joystick found.\n");
    return false;
}

void Joystick::Shutdown()
{
    if (m_FileDescriptor >= 0)
    {
        close(m_FileDescriptor);
        m_FileDescriptor = -1;
    }

    m_AxisCount = 0;
    m_ButtonCount = 0;

    m_AxisX = 0;
    m_AxisY = 0;
    m_HatX = 0;
    m_HatY = 0;

    m_HatAxisX = -1;
    m_HatAxisY = -1;

    memset(m_Buttons, 0, sizeof(m_Buttons));
    memset(m_ButtonPressed, 0, sizeof(m_ButtonPressed));
    memset(m_ButtonReleased, 0, sizeof(m_ButtonReleased));

    memset(m_Directions, 0, sizeof(m_Directions));
    memset(m_DirectionPressed, 0, sizeof(m_DirectionPressed));
    memset(m_DirectionReleased, 0, sizeof(m_DirectionReleased));

    memset(m_DeviceName, 0, sizeof(m_DeviceName));
    memset(m_DevicePath, 0, sizeof(m_DevicePath));
}

void Joystick::Update()
{
    memset(m_ButtonPressed, 0, sizeof(m_ButtonPressed));
    memset(m_ButtonReleased, 0, sizeof(m_ButtonReleased));

    memset(m_DirectionPressed, 0, sizeof(m_DirectionPressed));
    memset(m_DirectionReleased, 0, sizeof(m_DirectionReleased));

    if (m_FileDescriptor < 0)
        return;

    struct js_event event;

    while (true)
    {
        const ssize_t bytesRead =
            read(m_FileDescriptor, &event, sizeof(event));

        if (bytesRead == static_cast<ssize_t>(sizeof(event)))
        {
            const unsigned char type =
                event.type & ~JS_EVENT_INIT;

            if (type == JS_EVENT_AXIS)
            {
                if (event.number == 0)
                    m_AxisX = event.value;
                else if (event.number == 1)
                    m_AxisY = event.value;
            }
            else if (type == JS_EVENT_BUTTON &&
                     event.number < MaxButtons &&
                     event.number < m_ButtonCount)
            {
                const int button = event.number;
                const bool down = event.value != 0;
                const bool initial =
                    (event.type & JS_EVENT_INIT) != 0;

                if (!initial && down != m_Buttons[button])
                {
                    if (down)
                        m_ButtonPressed[button] = true;
                    else
                        m_ButtonReleased[button] = true;
                }

                m_Buttons[button] = down;
            }

            continue;
        }

        if (bytesRead < 0 && errno == EINTR)
            continue;

        if (bytesRead < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }

        if (bytesRead == 0)
            fprintf(stderr, "Joystick: device disconnected.\n");
        else if (bytesRead < 0)
            perror("Joystick: read");

        Shutdown();
        return;
    }

    UpdateDirections(false);
}

bool Joystick::IsInitialized() const
{
    return m_FileDescriptor >= 0;
}

int Joystick::GetAxisX() const
{
    return m_AxisX;
}

int Joystick::GetAxisY() const
{
    return m_AxisY;
}

int Joystick::GetButtonCount() const
{
    return m_ButtonCount;
}

bool Joystick::IsButtonPressed(int button) const
{
    return button >= 0 &&
           button < MaxButtons &&
           button < m_ButtonCount &&
           m_Buttons[button];
}

bool Joystick::WasButtonPressed(int button) const
{
    return button >= 0 &&
           button < MaxButtons &&
           button < m_ButtonCount &&
           m_ButtonPressed[button];
}

bool Joystick::WasButtonReleased(int button) const
{
    return button >= 0 &&
           button < MaxButtons &&
           button < m_ButtonCount &&
           m_ButtonReleased[button];
}

bool Joystick::IsDirectionPressed(Direction direction) const
{
    return direction >= 0 &&
           direction < DirectionCount &&
           m_Directions[direction];
}

bool Joystick::WasDirectionPressed(Direction direction) const
{
    return direction >= 0 &&
           direction < DirectionCount &&
           m_DirectionPressed[direction];
}

bool Joystick::WasDirectionReleased(Direction direction) const
{
    return direction >= 0 &&
           direction < DirectionCount &&
           m_DirectionReleased[direction];
}

void Joystick::SetDeadZone(int deadZone)
{
    if (deadZone < 0)
        deadZone = 0;
    else if (deadZone > 32767)
        deadZone = 32767;

    m_DeadZone = deadZone;
}

int Joystick::GetDeadZone() const
{
    return m_DeadZone;
}

void Joystick::SetDirectionButtonMapping(
    Direction direction,
    int button)
{
    if (direction < 0 || direction >= DirectionCount)
        return;

    if (button < -1 || button >= MaxButtons)
        return;

    m_DirectionButtons[direction] = button;
}

const char* Joystick::GetDeviceName() const
{
    return m_DeviceName;
}

const char* Joystick::GetDevicePath() const
{
    return m_DevicePath;
}

void Joystick::UpdateDirections(bool suppressEvents)
{
    bool newDirections[DirectionCount] = { false };

    // Analog stick.
    newDirections[DirectionUp] = m_AxisY < -m_DeadZone;
    newDirections[DirectionDown] = m_AxisY > m_DeadZone;

    newDirections[DirectionLeft] = m_AxisX < -m_DeadZone;
    newDirections[DirectionRight] = m_AxisX > m_DeadZone;

    // Optional direction button mappings.
    for (int i = 0; i < DirectionCount; ++i)
    {
        const int button = m_DirectionButtons[i];

        if (button >= 0 &&
            button < MaxButtons &&
            button < m_ButtonCount &&
            m_Buttons[button])
        {
            newDirections[i] = true;
        }
    }

    // Generate directional edge events.
    for (int i = 0; i < DirectionCount; ++i)
    {
        if (!suppressEvents)
        {
            if (newDirections[i] && !m_Directions[i])
                m_DirectionPressed[i] = true;
            else if (!newDirections[i] && m_Directions[i])
                m_DirectionReleased[i] = true;
        }

        m_Directions[i] = newDirections[i];
    }
}

bool Joystick::TryOpenDevice(const char* path)
{
    const int fd = open(path, O_RDONLY | O_NONBLOCK);

    if (fd < 0)
        return false;

    unsigned char axes = 0;
    unsigned char buttons = 0;

    if (ioctl(fd, JSIOCGAXES, &axes) < 0 ||
        ioctl(fd, JSIOCGBUTTONS, &buttons) < 0 ||
        axes < 2)
    {
        close(fd);
        return false;
    }

    m_FileDescriptor = fd;
    m_AxisCount = axes;
    m_ButtonCount = buttons;

    strncpy(m_DevicePath, path, sizeof(m_DevicePath) - 1);
    m_DevicePath[sizeof(m_DevicePath) - 1] = '\0';

    if (ioctl(fd, JSIOCGNAME(sizeof(m_DeviceName)),
              m_DeviceName) < 0)
    {
        strncpy(m_DeviceName, "Unknown joystick",
                sizeof(m_DeviceName) - 1);
        m_DeviceName[sizeof(m_DeviceName) - 1] = '\0';
    }

    fprintf(stderr,
            "Joystick: found %s at %s (%u axes, %u buttons).\n",
            m_DeviceName,
            m_DevicePath,
            static_cast<unsigned int>(m_AxisCount),
            static_cast<unsigned int>(m_ButtonCount));

    return true;
}

}
