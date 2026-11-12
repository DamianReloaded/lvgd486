#include "FontManager.hpp"
#include "../Display/IDisplay.hpp"

namespace vgd
{
    namespace UI
    {
        Vector<BMPFont> FontManager::m_Fonts;

        bool FontManager::Initialize()
        {
            m_Fonts.Clear();
            return true;
        }

        void FontManager::Shutdown()
        {
            m_Fonts.Clear();
        }

        int FontManager::AddFont(const BMPFont& font)
        {
            if (font.Width <= 0 || font.Height <= 0 ||
                !font.Space || !font.Digits || !font.Upper)
                return -1;

            if (!m_Fonts.PushBack(font))
                return -1;

            return m_Fonts.Size() - 1;
        }

        const BMPFont* FontManager::GetFont(int index)
        {
            return m_Fonts.Get(index);
        }

        void FontManager::DrawText(IDisplay* display, int font,
                                   const char* text, int x, int y, int color)
        {
            if (!display || !text)
                return;

            const BMPFont* bmpFont = GetFont(font);

            if (!bmpFont)
                return;

            display->DrawText(*bmpFont, text, x, y, (uint8_t)color);
        }
    }
}