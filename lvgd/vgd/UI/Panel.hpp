#pragma once
#include "Control.hpp"

namespace vgd
{
namespace UI
{

class Panel : public Control
{
public:
    Panel(IDisplay* display, int x, int y,
         int width, int height);
};

}
}

