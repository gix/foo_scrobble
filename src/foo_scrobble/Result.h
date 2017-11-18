#pragma once
#include <cassert>
#include <system_error>

namespace foo_scrobble
{

template<typename T, typename E = std::error_code>
class Result
{
    using ValueType =
        std::conditional_t<std::is_reference_v<T>, std::reference_wrapper<T>, T>;
    using ErrorType = E;

    using reference = std::remove_reference_t<T>&;
    using const_reference = std::remove_reference_t<T> const&;
    using pointer = std::remove_reference_t<T>*;
    using const_pointer = std::remove_reference_t<T> const*;

public:
    template<typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
    Result(U&& value)
        : hasError_(false)
    {
        new (&GetValue()) ValueType(std::forward<U>(value));
    }

    Result(ErrorType ec) noexcept
        : hasError_(true)
    {
        new (&GetError()) ErrorType(ec);
    }

    template<typename EC,
             typename = std::enable_if_t<std::is_error_code_enum<EC>::value ||
                                         std::is_error_condition_enum<EC>::value>>
    Result(EC ec) noexcept
        : hasError_(true)
    {
        new (&GetError()) ErrorType(std::make_error_code(ec));
    }

    Result(Result&& source) noexcept
        : hasError_(source.hasError_)
    {
        MoveConstruct(std::move(source));
    }

    Result& operator=(Result&& source) noexcept
    {
        MoveConstruct(std::move(source));
        return *this;
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
    Result(Result<U, E>&& source) noexcept
        : hasError_(source.hasError_)
    {
        MoveConstruct(std::move(source));
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible<U, T>::value>>
    Result& operator=(Result<U, E>&& source) noexcept
    {
        MoveConstruct(std::move(source));
        return *this;
    }

    template<bool B = std::is_default_constructible_v<T>, std::enable_if_t<B>* = nullptr>
    Result()
        : hasError_(false)
    {
        new (&GetValue()) ValueType();
    }

    Result(std::enable_if_t<std::is_copy_constructible_v<T>, Result const&> source)
        : hasError_(source.hasError_)
    {
        CopyConstruct(source);
    }

    Result&
        operator=(std::enable_if_t<std::is_copy_assignable_v<T>, Result const&> source)
    {
        if (this != &source) {
            hasError_ = source.hasError_;
            CopyConstruct(source);
        }
        return *this;
    }

    ~Result() { DestroyValue(); }

    constexpr explicit operator bool() const noexcept { return !hasError_; }
    constexpr bool HasValue() const noexcept { return !hasError_; }

    const_reference Value() const noexcept { return GetValue(); }
    reference Value() noexcept { return GetValue(); }

    ErrorType Error() const noexcept { return hasError_ ? GetError() : ErrorType(); }

    const_reference operator*() const { return GetValue(); }
    reference operator*() { return GetValue(); }

    const_pointer operator->() const { return &GetValue(); }
    pointer operator->() { return &GetValue(); }

private:
    template<typename U>
    void MoveConstruct(Result<U, E>&& source)
    {
        if (!source.hasError_)
            new (&GetValue()) ValueType(std::move(source.GetValue()));
        else
            new (&GetError()) ErrorType(std::move(source.GetError()));
    }

    template<typename U>
    void CopyConstruct(Result<U, E> const& source)
    {
        if (!source.hasError_)
            new (&GetValue()) ValueType(source.GetValue());
        else
            new (&GetError()) ErrorType(source.GetError());
    }

    void DestroyValue()
    {
        if (!hasError_)
            GetValue().~ValueType();
        else
            GetError().~ErrorType();
    }

    ValueType const& GetValue() const
    {
        assert(!hasError_);
        return *reinterpret_cast<ValueType const*>(&valueStorage_);
    }

    ValueType& GetValue()
    {
        assert(!hasError_);
        return *reinterpret_cast<ValueType*>(&valueStorage_);
    }

    ErrorType const& GetError() const
    {
        assert(hasError_);
        return *reinterpret_cast<ErrorType const*>(&errorStorage_);
    }

    ErrorType& GetError()
    {
        assert(hasError_);
        return *reinterpret_cast<ErrorType*>(&errorStorage_);
    }

    template<typename U>
    using AlignedStorage = std::aligned_storage_t<sizeof(U), alignof(U)>;

    union
    {
        AlignedStorage<ValueType> valueStorage_;
        AlignedStorage<ErrorType> errorStorage_;
    };
    bool hasError_ : 1;

    template<typename U, typename F>
    friend class Result;
};

template<typename T, typename E>
std::enable_if_t<
    std::is_error_code_enum<E>::value || std::is_error_condition_enum<E>::value, bool>
operator==(Result<T, E> const& result, E error)
{
    return result.Error() == error;
}

} // namespace ffmf
