#pragma once

#include "../../vgd/UI/Control.hpp"

class Background : public vgd::UI::Control
{
public:
    Background(vgd::IDisplay* display, int x, int y, int width, int height);

    void SetFrame(uint64_t frame, double interpolation);

protected:
    virtual void OnDraw(int x, int y);

private:
    uint64_t m_Frame;
    double m_Interpolation;
};
