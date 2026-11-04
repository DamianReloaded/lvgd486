#include "Panel.hpp"

namespace vgd
{
namespace UI
{

Panel::Panel(IDisplay* display, int x, int y,
           int width, int height)
    : Control(display, x, y, width, height)
{
}

}
}