#pragma once

#include "../Util/Vector.hpp"
#include "../Font/BmpFont.hpp"

namespace vgd
{
    class IDisplay;

    namespace UI
    {
        class FontManager
        {
        public:
            static bool Initialize();
            static void Shutdown();

            static int AddFont(const BMPFont& font);
            static const BMPFont* GetFont(int index);

            static void DrawText(IDisplay* display, int font, const char* text,
                                 int x, int y, int color);

        private:
            static Vector<BMPFont> m_Fonts;
        };
    }
}