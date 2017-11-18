#pragma once
#include <memory>
#include <vector>

namespace foo_scrobble
{

class Binding
{
public:
    Binding(pfc::string_base& var, HWND hwnd, int controlId);
    Binding(bool& var, HWND hwnd, int controlId);

    Binding(Binding&& source) noexcept;
    Binding& operator =(Binding&& source) noexcept;
    ~Binding();

    bool HasChanged() const;
    void FlowToControl();
    void FlowToVar();

    class IBinding;

private:
    std::unique_ptr<IBinding> binding_;
};


class BindingCollection
{
public:
    bool HasChanged() const;
    void FlowToControl();
    void FlowToVar();

    template<typename... T>
    void Bind(T&&... args)
    {
        bindings_.emplace_back(std::forward<T>(args)...);
    }

private:
    std::vector<Binding> bindings_;
};

} // namespace foo_scrobble
