#include "Control.hpp"

namespace vgd
{
namespace UI
{

Control::Control(IDisplay* display, int x, int y, int width, int height)
    : m_Display(display),
      m_X(x),
      m_Y(y),
      m_Width(width),
      m_Height(height),
      m_Visible(true),
      m_Enabled(true),
      m_Parent(0)
{
}

Control::~Control()
{
    for (int i = 0; i < m_Children.Size(); ++i)
    {
        Control* child = m_Children[i].Get();

        if (child)
            child->m_Parent = 0;
    }

    m_Children.Clear();
}

bool Control::RemoveChild(Control* control)
{
    if (!control)
        return false;

    for (int i = 0; i < m_Children.Size(); ++i)
    {
        if (m_Children[i].Get() == control)
        {
            control->m_Parent = 0;
            return m_Children.RemoveAt(i);
        }
    }

    return false;
}

void Control::Update()
{
    UpdateTree();
}

void Control::UpdateTree()
{
    if (!m_Visible || !m_Enabled)
        return;

    OnUpdate();

    for (int i = 0; i < m_Children.Size(); ++i)
    {
        Control* child = m_Children[i].Get();

        if (child)
            child->UpdateTree();
    }
}

void Control::Draw()
{
    DrawTree(0, 0);
}

void Control::DrawTree(int parentX, int parentY)
{
    if (!m_Visible)
        return;

    const int x = parentX + m_X;
    const int y = parentY + m_Y;

    OnDraw(x, y);

    for (int i = 0; i < m_Children.Size(); ++i)
    {
        Control* child = m_Children[i].Get();

        if (child)
            child->DrawTree(x, y);
    }
}

void Control::OnUpdate()
{
}

void Control::OnDraw(int x, int y)
{
    (void)x;
    (void)y;
}

bool Control::Contains(const Control* control) const
{
    if (this == control)
        return true;

    for (int i = 0; i < m_Children.Size(); ++i)
    {
        const Control* child = m_Children[i].Get();

        if (child && child->Contains(control))
            return true;
    }

    return false;
}

int Control::GetX() const
{
    return m_X;
}

int Control::GetY() const
{
    return m_Y;
}

int Control::GetAbsoluteX() const
{
    return m_X + (m_Parent ? m_Parent->GetAbsoluteX() : 0);
}

int Control::GetAbsoluteY() const
{
    return m_Y + (m_Parent ? m_Parent->GetAbsoluteY() : 0);
}

int Control::GetWidth() const
{
    return m_Width;
}

int Control::GetHeight() const
{
    return m_Height;
}

Control* Control::GetParent() const
{
    return m_Parent;
}

int Control::GetChildCount() const
{
    return m_Children.Size();
}

Control* Control::GetChild(int index) const
{
    const vgd::UniquePtr<Control>* child = m_Children.Get(index);
    return child ? child->Get() : 0;
}

void Control::SetPosition(int x, int y)
{
    m_X = x;
    m_Y = y;
}

void Control::SetSize(int width, int height)
{
    m_Width = width;
    m_Height = height;
}

void Control::SetVisible(bool visible)
{
    m_Visible = visible;
}

bool Control::IsVisible() const
{
    return m_Visible;
}

void Control::SetEnabled(bool enabled)
{
    m_Enabled = enabled;
}

bool Control::IsEnabled() const
{
    return m_Enabled;
}

}
}