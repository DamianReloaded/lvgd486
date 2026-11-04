#include "Background.hpp"
#include <stdint.h>

Background::Background(vgd::IDisplay* display, int x, int y, int width, int height)
    : Control(display, x, y, width, height),
      m_Frame(0),
      m_Interpolation(0.0)
{
}

void Background::SetFrame(uint64_t frame, double interpolation)
{
    m_Frame = frame;
    m_Interpolation = interpolation;
}

void Background::OnDraw(int x, int y)
{
    (void)x;
    (void)y;

    if (!m_Display)
        return;

    uint8_t* buffer = m_Display->GetBackBuffer();
    const int pitch = m_Display->GetBackBufferPitch();
    const int width = m_Display->GetWidth();
    const int height = m_Display->GetHeight();

    if (!buffer || pitch < width)
        return;

    const double frame = (double)m_Frame + m_Interpolation;
    const int offsetX = (int)(frame * 0.25);
    const int offsetY = (int)(frame * 0.125);

    for (int y = 0; y < height; ++y)
    {
        uint8_t* row = buffer + y * pitch;
        const int sourceY = (y + offsetY) & 31;
        const int tileY = sourceY >> 4;

        for (int x = 0; x < width; ++x)
        {
            const int sourceX = (x + offsetX) & 31;
            const int tileX = sourceX >> 4;

            row[x] = (uint8_t)(((tileX + tileY) & 1) ? 101 : 1000);
        }
    }
}
