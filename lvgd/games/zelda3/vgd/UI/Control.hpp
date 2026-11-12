#ifndef VGD_CONTROL_HPP
#define VGD_CONTROL_HPP

#include "../Display/IDisplay.hpp"
#include "../Util/UniquePtr.hpp"
#include "../Util/Vector.hpp"

#include <utility>

namespace vgd
{
namespace UI
{

class Control
{
public:
    Control(IDisplay* display, int x, int y, int width, int height);
    virtual ~Control();

    template <typename T>
    bool AddChild(vgd::UniquePtr<T>&& control)
    {
        Control* child = control.Get();

        if (!child || child == this || child->m_Parent)
            return false;

        if (child->Contains(this))
            return false;

        // Reserve before transferring ownership.
        if (!m_Children.Reserve(m_Children.Size() + 1))
            return false;

        vgd::UniquePtr<Control> owned(std::move(control));

        if (!m_Children.PushBack(std::move(owned)))
            return false;

        child->m_Parent = this;
        return true;
    }

    bool RemoveChild(Control* control);

    virtual void Update();
    void Draw();

    int GetX() const;
    int GetY() const;
    int GetAbsoluteX() const;
    int GetAbsoluteY() const;

    int GetWidth() const;
    int GetHeight() const;

    Control* GetParent() const;

    void SetPosition(int x, int y);
    void SetSize(int width, int height);

    void SetVisible(bool visible);
    bool IsVisible() const;

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    int GetChildCount() const;
    Control* GetChild(int index) const;

protected:
    virtual void OnUpdate();
    virtual void OnDraw(int x, int y);

    IDisplay* m_Display;

    int m_X;
    int m_Y;
    int m_Width;
    int m_Height;

    bool m_Visible;
    bool m_Enabled;

    Control* m_Parent;

private:
    void UpdateTree();
    void DrawTree(int parentX, int parentY);
    bool Contains(const Control* control) const;

    vgd::Vector<vgd::UniquePtr<Control> > m_Children;
};

}
}

#endif