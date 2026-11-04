#include "Label.hpp"
#include "FontManager.hpp"
#include <string.h>

namespace vgd
{
namespace UI
{

Label::Label(IDisplay* display, int x, int y,
             int width, int height, const char* text, int font)
    : Control(display, x, y, width, height),
      m_Color(15),
      m_Font(font),
      m_Alignment(AlignLeft)
{
    SetText(text);
}

void Label::SetText(const char* text)
{
    if (!text)
        text = "";

    strncpy(m_Text, text, sizeof(m_Text) - 1);
    m_Text[sizeof(m_Text) - 1] = '\0';
}

const char* Label::GetText() const
{
    return m_Text;
}

void Label::SetColor(int color)
{
    m_Color = color;
}

int Label::GetColor() const
{
    return m_Color;
}

void Label::SetAlignment(Alignment alignment)
{
    m_Alignment = alignment;
}

Label::Alignment Label::GetAlignment() const
{
    return m_Alignment;
}

void Label::SetFont(int font)
{
    m_Font = font;
}

int Label::GetFont() const
{
    return m_Font;
}

void Label::OnDraw(int x, int y)
{
    if (!m_Display || !m_Text[0])
        return;

    const BMPFont* font = FontManager::GetFont(m_Font);

    if (!font)
        return;

    int textWidth = (int)strlen(m_Text) * font->Width;
    int textX = x;

    if (m_Alignment == AlignCenter)
        textX += (m_Width - textWidth) / 2;
    else if (m_Alignment == AlignRight)
        textX += m_Width - textWidth;

    FontManager::DrawText(
        m_Display, m_Font, m_Text,
        textX, y, m_Color
    );
}

}
}