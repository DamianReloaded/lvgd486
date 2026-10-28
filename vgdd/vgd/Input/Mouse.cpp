#include "Mouse.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace vgd
{
    namespace
    {
        /*
         * Open every available candidate instead of assuming that
         * the first successfully opened device is the correct one.
         *
         * Linux input interfaces are tried alongside legacy PS/2
         * interfaces so that both Debian Sarge and the older devfs
         * setup can be supported.
         *
         * All candidates must supply standard three-byte PS/2
         * compatible packets. Other protocols require their own
         * decoders.
         */
        const char* const MouseDevicePaths[] =
        {
            "/dev/input/mice",
            "/dev/input/mouse0",
            "/dev/input/mouse1",
            "/dev/input/mouse2",
            "/dev/input/mouse3",
            "/dev/input/mouse4",
            "/dev/input/mouse5",
            "/dev/input/mouse6",
            "/dev/input/mouse7",
            "/dev/misc/psaux",
            "/dev/psaux",
            "/dev/mouse"
        };

        const int MouseDevicePathCount =
            sizeof(MouseDevicePaths) / sizeof(MouseDevicePaths[0]);

        /*
         * Validate the standard PS/2 packet status and sign bits.
         *
         * Status bit 3 must be set.
         * Status bit 4 contains the X sign.
         * Status bit 5 contains the Y sign.
         * Status bits 6 and 7 indicate X and Y overflow.
         */
        bool IsPlausiblePacket(const unsigned char* packet)
        {
            const unsigned char flags = packet[0];

            if ((flags & 0x08) == 0)
                return false;

            if ((flags & 0x40) == 0)
            {
                const bool negativeX =
                    (packet[1] & 0x80) != 0;

                const bool signX =
                    (flags & 0x10) != 0;

                if (negativeX != signX)
                    return false;
            }

            if ((flags & 0x80) == 0)
            {
                const bool negativeY =
                    (packet[2] & 0x80) != 0;

                const bool signY =
                    (flags & 0x20) != 0;

                if (negativeY != signY)
                    return false;
            }

            return true;
        }

        /*
         * Convert an unsigned movement byte to a signed displacement
         * without relying on implementation-defined conversion to
         * int8_t.
         */
        int DecodeMovement(unsigned char value)
        {
            if (value & 0x80)
                return static_cast<int>(value) - 256;

            return static_cast<int>(value);
        }
    }

    Mouse::Mouse()
        : m_Fd(-1),
          m_DevicePath(NULL),
          m_Initialized(false),
          m_DeltaX(0),
          m_DeltaY(0),
          m_PacketCount(0),
          m_EventCount(0),
          m_EventRead(0),
          m_CandidateCount(0)
    {
        int i;

        for (i = 0; i < MaxCandidates; ++i)
        {
            m_CandidateFds[i] = -1;
            m_CandidatePaths[i] = NULL;
            m_CandidatePacketCounts[i] = 0;

            m_CandidatePackets[i][0] = 0;
            m_CandidatePackets[i][1] = 0;
            m_CandidatePackets[i][2] = 0;
        }

        ResetState();
    }

    Mouse::~Mouse()
    {
        Shutdown();
    }

    bool Mouse::Initialize()
    {
        if (m_Initialized)
            return true;

        Shutdown();
        ResetState();

        /*
         * Open every available candidate nonblocking.
         *
         * No input is required during initialization. A device
         * remains a candidate until Update() receives and validates
         * a packet from it.
         */
        for (int i = 0; i < MouseDevicePathCount; ++i)
        {
            if (m_CandidateCount >= MaxCandidates)
                break;

            const char* path = MouseDevicePaths[i];

            /*
             * Avoid opening the same pathname twice.
             */
            bool alreadyTried = false;

            for (int j = 0; j < m_CandidateCount; ++j)
            {
                if (strcmp(m_CandidatePaths[j], path) == 0)
                {
                    alreadyTried = true;
                    break;
                }
            }

            if (alreadyTried)
                continue;

            const int fd = open(path, O_RDONLY | O_NONBLOCK);

            if (fd < 0)
                continue;

            const int index = m_CandidateCount++;

            m_CandidateFds[index] = fd;
            m_CandidatePaths[index] = path;
            m_CandidatePacketCounts[index] = 0;

            m_CandidatePackets[index][0] = 0;
            m_CandidatePackets[index][1] = 0;
            m_CandidatePackets[index][2] = 0;

            fprintf(stderr,
                    "Mouse: candidate %s\n",
                    path);
        }

        if (m_CandidateCount == 0)
        {
            fprintf(stderr,
                    "Mouse: no supported mouse device could be opened\n");

            return false;
        }

        m_Initialized = true;

        fprintf(stderr,
                "Mouse: monitoring %d candidate device(s)\n",
                m_CandidateCount);

        return true;
    }

    void Mouse::Shutdown()
    {
        if (m_Fd >= 0)
        {
            close(m_Fd);
            m_Fd = -1;
        }

        for (int i = 0; i < m_CandidateCount; ++i)
        {
            if (m_CandidateFds[i] >= 0)
            {
                close(m_CandidateFds[i]);
                m_CandidateFds[i] = -1;
            }

            m_CandidatePaths[i] = NULL;
            m_CandidatePacketCounts[i] = 0;
        }

        m_CandidateCount = 0;
        m_DevicePath = NULL;
        m_Initialized = false;

        ResetState();
    }

    void Mouse::Update()
    {
        /*
         * Pressed and released flags describe this update only.
         * Down state persists across updates until a packet changes it.
         */
        for (int i = 0; i < ButtonCount; ++i)
        {
            m_Pressed[i] = false;
            m_Released[i] = false;
        }

        m_DeltaX = 0;
        m_DeltaY = 0;

        CompactQueues();

        if (!m_Initialized)
            return;

        unsigned char buffer[64];

        /*
         * Once a candidate has been selected, use only its descriptor.
         */
        if (m_Fd >= 0)
        {
            for (;;)
            {
                const ssize_t count =
                    read(m_Fd, buffer, sizeof(buffer));

                if (count < 0)
                {
                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    perror("Mouse: read");
                    Shutdown();
                    return;
                }

                if (count == 0)
                    break;

                for (ssize_t i = 0; i < count; ++i)
                    ProcessByte(buffer[i]);
            }

            return;
        }

        /*
         * No device has been selected yet.
         *
         * Poll every candidate on each Update(). A silent candidate
         * is retained while other candidates are checked.
         */
        int candidateIndex = 0;

        while (candidateIndex < m_CandidateCount)
        {
            const int fd = m_CandidateFds[candidateIndex];

            if (fd < 0)
            {
                ++candidateIndex;
                continue;
            }

            bool removeCandidate = false;

            for (;;)
            {
                const ssize_t count =
                    read(fd, buffer, sizeof(buffer));

                if (count < 0)
                {
                    if (errno == EINTR)
                        continue;

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;

                    fprintf(stderr,
                            "Mouse: device read failed: %s\n",
                            m_CandidatePaths[candidateIndex]);

                    removeCandidate = true;
                    break;
                }

                if (count == 0)
                    break;

                for (ssize_t i = 0; i < count; ++i)
                {
                    ProcessCandidateByte(candidateIndex, buffer[i]);

                    /*
                     * Selection closes all competing descriptors.
                     * The first validated packet has already been
                     * processed. Feed any remaining bytes from this
                     * read directly into the active decoder.
                     */
                    if (m_Fd >= 0)
                    {
                        for (ssize_t j = i + 1; j < count; ++j)
                            ProcessByte(buffer[j]);

                        return;
                    }
                }
            }

            /*
             * Remove a candidate only if it has failed with a real
             * read error. EAGAIN means there is simply no data yet.
             */
            if (removeCandidate)
            {
                close(m_CandidateFds[candidateIndex]);

                for (int j = candidateIndex;
                     j < m_CandidateCount - 1;
                     ++j)
                {
                    m_CandidateFds[j] =
                        m_CandidateFds[j + 1];

                    m_CandidatePaths[j] =
                        m_CandidatePaths[j + 1];

                    m_CandidatePacketCounts[j] =
                        m_CandidatePacketCounts[j + 1];

                    m_CandidatePackets[j][0] =
                        m_CandidatePackets[j + 1][0];

                    m_CandidatePackets[j][1] =
                        m_CandidatePackets[j + 1][1];

                    m_CandidatePackets[j][2] =
                        m_CandidatePackets[j + 1][2];
                }

                --m_CandidateCount;

                if (m_CandidateCount < MaxCandidates)
                {
                    m_CandidateFds[m_CandidateCount] = -1;
                    m_CandidatePaths[m_CandidateCount] = NULL;
                    m_CandidatePacketCounts[m_CandidateCount] = 0;
                }

                /*
                 * Do not increment candidateIndex here: the next
                 * candidate has shifted into this position.
                 */
                continue;
            }

            ++candidateIndex;
        }

        /*
         * No candidate has yet produced a valid packet. Keep all
         * remaining candidates for subsequent Update() calls.
         */
        if (m_CandidateCount == 0)
        {
            fprintf(stderr,
                    "Mouse: all candidate devices failed\n");

            m_Initialized = false;
        }
    }

    int Mouse::GetDeltaX() const
    {
        return m_DeltaX;
    }

    int Mouse::GetDeltaY() const
    {
        return m_DeltaY;
    }

    bool Mouse::IsDown(Button button) const
    {
        return IsValidButton(button) && m_Down[button];
    }

    bool Mouse::WasPressed(Button button) const
    {
        return IsValidButton(button) && m_Pressed[button];
    }

    bool Mouse::WasReleased(Button button) const
    {
        return IsValidButton(button) && m_Released[button];
    }

    bool Mouse::HasEvent() const
    {
        return m_EventRead < m_EventCount;
    }

    Mouse::Event Mouse::ReadEvent()
    {
        Event event;

        event.type = EventMove;
        event.button = ButtonLeft;
        event.x = 0;
        event.y = 0;
        event.deltaX = 0;
        event.deltaY = 0;

        if (HasEvent())
            event = m_Events[m_EventRead++];

        return event;
    }

    void Mouse::ClearState()
    {
        ResetState();
    }

    const char* Mouse::GetDevicePath() const
    {
        /*
         * Returns NULL until an actual valid packet identifies the
         * active device.
         */
        return m_DevicePath;
    }

    bool Mouse::IsValidButton(Button button)
    {
        return button >= ButtonLeft && button < ButtonCount;
    }

    void Mouse::ResetState()
    {
        m_DeltaX = 0;
        m_DeltaY = 0;
        m_PacketCount = 0;

        m_EventCount = 0;
        m_EventRead = 0;

        for (int i = 0; i < ButtonCount; ++i)
        {
            m_Down[i] = false;
            m_Pressed[i] = false;
            m_Released[i] = false;
        }
    }

    void Mouse::CompactQueues()
    {
        if (m_EventRead == m_EventCount)
        {
            m_EventRead = 0;
            m_EventCount = 0;
        }
        else if (m_EventRead > 0)
        {
            const int remaining =
                m_EventCount - m_EventRead;

            memmove(m_Events,
                    &m_Events[m_EventRead],
                    remaining * sizeof(Event));

            m_EventCount = remaining;
            m_EventRead = 0;
        }
    }

    void Mouse::AddEvent(EventType type,
                         Button button,
                         int deltaX,
                         int deltaY)
    {
        if (m_EventCount >= MaxEvents)
            return;

        Event& event = m_Events[m_EventCount++];

        event.type = type;
        event.button = button;
        event.x = m_DeltaX;
        event.y = m_DeltaY;
        event.deltaX = deltaX;
        event.deltaY = deltaY;
    }

    /*
     * Process bytes from the selected device.
     */
    void Mouse::ProcessByte(unsigned char byte)
    {
        if (m_PacketCount == 0)
        {
            if ((byte & 0x08) == 0)
                return;

            m_Packet[0] = byte;
            m_PacketCount = 1;
            return;
        }

        m_Packet[m_PacketCount++] = byte;

        if (m_PacketCount < 3)
            return;

        if (IsPlausiblePacket(m_Packet))
        {
            ProcessPacket();
            m_PacketCount = 0;
            return;
        }

        /*
         * Recover packet alignment by sliding forward to the next
         * possible status byte.
         */
        const unsigned char second = m_Packet[1];
        const unsigned char third = m_Packet[2];

        m_PacketCount = 0;

        if (second & 0x08)
        {
            m_Packet[0] = second;
            m_Packet[1] = third;
            m_PacketCount = 2;
        }
        else if (third & 0x08)
        {
            m_Packet[0] = third;
            m_PacketCount = 1;
        }
    }

    /*
     * Process one byte from a candidate device.
     *
     * Returns true when the candidate produces a valid packet.
     * That packet is processed immediately after selecting the device.
     */
    bool Mouse::ProcessCandidateByte(int candidateIndex,
                                     unsigned char byte)
    {
        if (candidateIndex < 0 ||
            candidateIndex >= m_CandidateCount)
        {
            return false;
        }

        unsigned char* packet =
            m_CandidatePackets[candidateIndex];

        int& packetCount =
            m_CandidatePacketCounts[candidateIndex];

        if (packetCount == 0)
        {
            if ((byte & 0x08) == 0)
                return false;

            packet[0] = byte;
            packetCount = 1;
            return false;
        }

        packet[packetCount++] = byte;

        if (packetCount < 3)
            return false;

        if (IsPlausiblePacket(packet))
        {
            /*
             * Preserve and process the exact packet that established
             * the candidate as the active device.
             */
            m_Packet[0] = packet[0];
            m_Packet[1] = packet[1];
            m_Packet[2] = packet[2];
            m_PacketCount = 3;

            SelectCandidate(candidateIndex);

            ProcessPacket();

            m_PacketCount = 0;

            return true;
        }

        /*
         * Invalid packet: attempt to recover alignment.
         */
        const unsigned char second = packet[1];
        const unsigned char third = packet[2];

        packetCount = 0;

        if (second & 0x08)
        {
            packet[0] = second;
            packet[1] = third;
            packetCount = 2;
        }
        else if (third & 0x08)
        {
            packet[0] = third;
            packetCount = 1;
        }

        return false;
    }

    /*
     * Promote the candidate that produced a valid packet.
     * All competing descriptors are closed only after this evidence
     * exists.
     */
    void Mouse::SelectCandidate(int candidateIndex)
    {
        if (candidateIndex < 0 ||
            candidateIndex >= m_CandidateCount)
        {
            return;
        }

        const int selectedFd =
            m_CandidateFds[candidateIndex];

        const char* selectedPath =
            m_CandidatePaths[candidateIndex];

        for (int i = 0; i < m_CandidateCount; ++i)
        {
            if (i != candidateIndex &&
                m_CandidateFds[i] >= 0)
            {
                close(m_CandidateFds[i]);
            }

            m_CandidateFds[i] = -1;
            m_CandidatePaths[i] = NULL;
            m_CandidatePacketCounts[i] = 0;
        }

        m_Fd = selectedFd;
        m_DevicePath = selectedPath;
        m_CandidateCount = 0;

        fprintf(stderr,
                "Mouse: selected active device %s\n",
                m_DevicePath);
    }

    void Mouse::ProcessPacket()
    {
        const unsigned char flags = m_Packet[0];

        int dx = (flags & 0x40) ? 0 :
            DecodeMovement(m_Packet[1]);

        int dy = (flags & 0x80) ? 0 :
            DecodeMovement(m_Packet[2]);

        /*
         * PS/2 Y increases upward; screen coordinates increase
         * downward.
         */
        dy = -dy;

        if (dx != 0 || dy != 0)
        {
            m_DeltaX += dx;
            m_DeltaY += dy;

            AddEvent(EventMove, ButtonLeft, dx, dy);
        }

        for (int i = 0; i < ButtonCount; ++i)
        {
            const bool down =
                (flags & (1 << i)) != 0;

            if (down == m_Down[i])
                continue;

            m_Down[i] = down;

            if (down)
            {
                m_Pressed[i] = true;

                AddEvent(EventButtonDown,
                         static_cast<Button>(i),
                         0,
                         0);
            }
            else
            {
                m_Released[i] = true;

                AddEvent(EventButtonUp,
                         static_cast<Button>(i),
                         0,
                         0);
            }
        }
    }
}
