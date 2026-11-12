#pragma once

#include <stdint.h>

namespace vgd
{
    class Mouse
    {
    public:
        enum Button
        {
            ButtonLeft = 0,
            ButtonRight,
            ButtonMiddle,
            ButtonCount
        };

        enum EventType
        {
            EventMove,
            EventButtonDown,
            EventButtonUp
        };

        struct Event
        {
            EventType type;
            Button button;
            int x;
            int y;
            int deltaX;
            int deltaY;
        };

        Mouse();
        ~Mouse();

        bool Initialize();
        void Shutdown();
        void Update();

        int GetDeltaX() const;
        int GetDeltaY() const;

        bool IsDown(Button button) const;
        bool WasPressed(Button button) const;
        bool WasReleased(Button button) const;

        bool HasEvent() const;
        Event ReadEvent();

        void ClearState();

        const char* GetDevicePath() const;

    private:
        Mouse(const Mouse&);
        Mouse& operator=(const Mouse&);

        static const int MaxEvents = 256;
        static const int MaxCandidates = 19;

        int m_Fd;
        const char* m_DevicePath;
        bool m_Initialized;

        int m_DeltaX;
        int m_DeltaY;

        bool m_Down[ButtonCount];
        bool m_Pressed[ButtonCount];
        bool m_Released[ButtonCount];

        unsigned char m_Packet[3];
        int m_PacketCount;

        Event m_Events[MaxEvents];
        int m_EventCount;
        int m_EventRead;

        int m_CandidateFds[MaxCandidates];
        const char* m_CandidatePaths[MaxCandidates];
        unsigned char m_CandidatePackets[MaxCandidates][3];
        int m_CandidatePacketCounts[MaxCandidates];
        int m_CandidateCount;

        static bool IsValidButton(Button button);

        void ResetState();
        void CompactQueues();

        void ProcessByte(unsigned char byte);
        void ProcessPacket();

        bool ProcessCandidateByte(int candidateIndex,
                                  unsigned char byte);

        void SelectCandidate(int candidateIndex);

        void AddEvent(EventType type,
                      Button button,
                      int deltaX,
                      int deltaY);
    };
}
