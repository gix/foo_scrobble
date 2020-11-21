#include "Bindings.h"

#include <algorithm>

namespace foo_scrobble
{

class Binding::IBinding
{
public:
    virtual ~IBinding() = default;
    virtual bool HasChanged() const = 0;
    virtual void FlowToControl() = 0;
    virtual void FlowToVar() = 0;
};

namespace
{

class StringBinding : public Binding::IBinding
{
public:
    StringBinding(pfc::string_base& var, HWND hwnd, int controlId)
        : var_(var)
        , hwnd_(hwnd)
        , controlId_(controlId)
    {}

    bool HasChanged() const override
    {
        pfc::string8_fast text;
        uGetDlgItemText(hwnd_, controlId_, text);
        return uStringCompare(var_.c_str(), text.c_str()) != 0;
    }

    void FlowToControl() override { uSetDlgItemText(hwnd_, controlId_, var_); }

    void FlowToVar() override
    {
        pfc::string8_fast text;
        uGetDlgItemText(hwnd_, controlId_, text);
        var_ = text.c_str();
    }

private:
    pfc::string_base& var_;
    HWND hwnd_;
    int controlId_;
};

class BoolBinding : public Binding::IBinding
{
public:
    BoolBinding(bool& var, HWND hwnd, int controlId)
        : var_(var)
        , hwnd_(hwnd)
        , controlId_(controlId)
    {}

    bool HasChanged() const override
    {
        bool const checked = IsDlgButtonChecked(hwnd_, controlId_) == BST_CHECKED;
        return checked != var_;
    }

    void FlowToControl() override
    {
        CheckDlgButton(hwnd_, controlId_, var_ ? BST_CHECKED : BST_UNCHECKED);
    }

    void FlowToVar() override
    {
        var_ = IsDlgButtonChecked(hwnd_, controlId_) == BST_CHECKED;
    }

private:
    bool& var_;
    HWND hwnd_;
    int controlId_;
};

} // namespace

Binding::Binding(pfc::string_base& var, HWND hwnd, int controlId)
    : binding_(std::make_unique<StringBinding>(var, hwnd, controlId))
{}

Binding::Binding(bool& var, HWND hwnd, int controlId)
    : binding_(std::make_unique<BoolBinding>(var, hwnd, controlId))
{}

Binding::Binding(Binding&&) noexcept = default;
Binding& Binding::operator=(Binding&&) noexcept = default;
Binding::~Binding() = default;

bool Binding::HasChanged() const
{
    return binding_->HasChanged();
}

void Binding::FlowToControl()
{
    binding_->FlowToControl();
}

void Binding::FlowToVar()
{
    binding_->FlowToVar();
}

bool BindingCollection::HasChanged() const
{
    return std::any_of(std::begin(bindings_), std::end(bindings_),
                       [](auto const& binding) { return binding.HasChanged(); });
}

void BindingCollection::FlowToControl()
{
    for (auto&& binding : bindings_)
        binding.FlowToControl();
}

void BindingCollection::FlowToVar()
{
    for (auto&& binding : bindings_)
        binding.FlowToVar();
}

} // namespace foo_scrobble
