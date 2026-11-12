#pragma once
#include "Control.hpp"

namespace vgd
{
namespace UI
{

class Label : public Control
{
public:
    enum Alignment
    {
        AlignLeft,
        AlignCenter,
        AlignRight
    };

    Label(IDisplay* display, int x, int y,
          int width, int height, const char* text, int font = 0);

    void SetText(const char* text);
    const char* GetText() const;

    void SetColor(int color);
    int GetColor() const;

    void SetAlignment(Alignment alignment);
    Alignment GetAlignment() const;

    void SetFont(int font);
    int GetFont() const;

protected:
    virtual void OnDraw(int x, int y);

private:
    char m_Text[128];
    int m_Color;
    int m_Font;
    Alignment m_Alignment;
};

}
}