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
        m_AxisY(0)
    {
        memset(m_Buttons, 0, sizeof(m_Buttons));
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

        memset(m_Buttons, 0, sizeof(m_Buttons));
        memset(m_DeviceName, 0, sizeof(m_DeviceName));
        memset(m_DevicePath, 0, sizeof(m_DevicePath));
    }

    void Joystick::Update()
    {
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
                        event.number < MaxButtons)
                {
                    m_Buttons[event.number] = event.value != 0;
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
            break;
        }
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

    bool Joystick::IsButtonDown(int button) const
    {
        return button >= 0 &&
            button < MaxButtons &&
            button < m_ButtonCount &&
            m_Buttons[button];
    }

    const char* Joystick::GetDeviceName() const
    {
        return m_DeviceName;
    }

    const char* Joystick::GetDevicePath() const
    {
        return m_DevicePath;
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
                m_DeviceName, m_DevicePath,
                static_cast<unsigned int>(m_AxisCount),
                static_cast<unsigned int>(m_ButtonCount));

        return true;
    }

}
