#include "Thumbnail.hpp"

#include <stdint.h>

Thumbnail::Thumbnail(vgd::IDisplay* display, int x, int y, int width, int height)
    : Control(display, x, y, width, height),
      m_Image(0)
{
}

void Thumbnail::SetImage(const vgd::Image* image)
{
    m_Image = image;
}

const vgd::Image* Thumbnail::GetImage() const
{
    return m_Image;
}

void Thumbnail::OnDraw(int x, int y)
{
    if (!m_Display || !m_Image || !m_Image->IsValid())
        return;

    uint8_t* buffer = m_Display->GetBackBuffer();
    const int pitch = m_Display->GetBackBufferPitch();
    const int displayWidth = m_Display->GetWidth();
    const int displayHeight = m_Display->GetHeight();

    if (!buffer || pitch < displayWidth)
        return;

    m_Image->Blit(
        buffer,
        displayWidth,
        displayHeight,
        pitch,
        x,
        y
    );
}