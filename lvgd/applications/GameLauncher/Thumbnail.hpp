#pragma once

#include "../../vgd/UI/Control.hpp"
#include "../../vgd/Image/Image.hpp"

class Thumbnail : public vgd::UI::Control
{
public:
    Thumbnail(vgd::IDisplay* display, int x, int y, int width, int height);

    void SetImage(const vgd::Image* image);
    const vgd::Image* GetImage() const;

protected:
    virtual void OnDraw(int x, int y);

private:
    const vgd::Image* m_Image;
};