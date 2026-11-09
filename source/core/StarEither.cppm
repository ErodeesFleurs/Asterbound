module;
import std;
import star.optional;
import star.format;

namespace Star {

struct EitherExceptionTag {
  static constexpr std::string_view name() { return "EitherException"; }
};
using EitherException = StarError<EitherExceptionTag, StarException>;

template <typename Value>
struct EitherLeftValue {
  Value value;
};

template <typename Value>
struct EitherRightValue {
  Value value;
};

template <typename Value>
EitherLeftValue<Value> makeLeft(Value value);

template <typename Value>
EitherRightValue<Value> makeRight(Value value);

// Container that contains exactly one of either Left or Right.
template <typename Left, typename Right>
class Either {
public:
  // Constructs Either that contains a default constructed Left value
  Either();

  Either(EitherLeftValue<Left> left);
  Either(EitherRightValue<Right> right);

  template <typename T>
  Either(EitherLeftValue<T> left);

  template <typename T>
  Either(EitherRightValue<T> right);

  Either(Either const& rhs);
  Either(Either&& rhs);

  Either& operator=(Either const& rhs);
  Either& operator=(Either&& rhs);

  template <typename T>
  Either& operator=(EitherLeftValue<T> left);

  template <typename T>
  Either& operator=(EitherRightValue<T> right);

  bool isLeft() const;
  bool isRight() const;

  void setLeft(Left left);
  void setRight(Right left);

  // Checked side access returns a diagnostic on the inactive alternative.

  [[nodiscard]]
  std::expected<std::reference_wrapper<Left const>, StarException> left() const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<Right const>, StarException> right() const;

  [[nodiscard]]
  std::expected<std::reference_wrapper<Left>, StarException> left();
  [[nodiscard]]
  std::expected<std::reference_wrapper<Right>, StarException> right();

  std::optional<Left> maybeLeft() const;
  std::optional<Right> maybeRight() const;

  Left const& leftRef() const;
  Right const& rightRef() const;
  Left& leftRef();
  Right& rightRef();

  std::optional<std::reference_wrapper<Left const>> maybeLeftRef() const;
  std::optional<std::reference_wrapper<Right const>> maybeRightRef() const;
  std::optional<std::reference_wrapper<Left>> maybeLeftRef();
  std::optional<std::reference_wrapper<Right>> maybeRightRef();

private:
  using LeftType = EitherLeftValue<Left>;
  using RightType = EitherRightValue<Right>;

  std::variant<LeftType, RightType> m_value;
};

template <typename Value>
EitherLeftValue<Value> makeLeft(Value value) {
  return {std::move(value)};
}

template <typename Value>
EitherRightValue<Value> makeRight(Value value) {
  return {std::move(value)};
}

template <typename Left, typename Right> Either<Left, Right>::Either() = default;

template <typename Left, typename Right>
Either<Left, Right>::Either(EitherLeftValue<Left> left)
  : m_value(std::move(left)) {}

template <typename Left, typename Right>
Either<Left, Right>::Either(EitherRightValue<Right> right)
  : m_value(std::move(right)) {}

template <typename Left, typename Right>
template <typename T>
Either<Left, Right>::Either(EitherLeftValue<T> left)
  : Either(LeftType{std::move(left.value)}) {}

template <typename Left, typename Right>
template <typename T>
Either<Left, Right>::Either(EitherRightValue<T> right)
  : Either(RightType{std::move(right.value)}) {}

template <typename Left, typename Right>
Either<Left, Right>::Either(Either const& rhs)
  : m_value(rhs.m_value) {}

template <typename Left, typename Right>
Either<Left, Right>::Either(Either&& rhs)
  : m_value(std::move(rhs.m_value)) {
  std::visit([](auto& side) {
    if constexpr (requires { optionalRef(side.value); })
      side.value.reset();
  }, rhs.m_value);
}

template <typename Left, typename Right>
Either<Left, Right>& Either<Left, Right>::operator=(Either const& rhs) = default;

template <typename Left, typename Right>
Either<Left, Right>& Either<Left, Right>::operator=(Either&& rhs) {
  if (this == &rhs)
    return *this;
  m_value = std::move(rhs.m_value);
  std::visit([](auto& side) {
    if constexpr (requires { optionalRef(side.value); })
      side.value.reset();
  }, rhs.m_value);
  return *this;
}

template <typename Left, typename Right>
template <typename T>
Either<Left, Right>& Either<Left, Right>::operator=(EitherLeftValue<T> left) {
  m_value = LeftType{std::move(left.value)};
  return *this;
}

template <typename Left, typename Right>
template <typename T>
Either<Left, Right>& Either<Left, Right>::operator=(EitherRightValue<T> right) {
  m_value = RightType{std::move(right.value)};
  return *this;
}

template <typename Left, typename Right>
bool Either<Left, Right>::isLeft() const {
  return std::holds_alternative<LeftType>(m_value);
}

template <typename Left, typename Right>
bool Either<Left, Right>::isRight() const {
  return std::holds_alternative<RightType>(m_value);
}

template <typename Left, typename Right>
void Either<Left, Right>::setLeft(Left left) {
  m_value = LeftType{std::move(left)};
}

template <typename Left, typename Right>
void Either<Left, Right>::setRight(Right right) {
  m_value = RightType{std::move(right)};
}

template <typename Left, typename Right>
std::expected<std::reference_wrapper<Left const>, StarException> Either<Left, Right>::left() const {
  if (auto l = maybeLeftRef())
    return *l;
  return std::unexpected(EitherException("Improper access of left side of Either"));
}

template <typename Left, typename Right>
std::expected<std::reference_wrapper<Right const>, StarException> Either<Left, Right>::right() const {
  if (auto r = maybeRightRef())
    return *r;
  return std::unexpected(EitherException("Improper access of right side of Either"));
}

template <typename Left, typename Right>
std::expected<std::reference_wrapper<Left>, StarException> Either<Left, Right>::left() {
  if (auto l = maybeLeftRef())
    return *l;
  return std::unexpected(EitherException("Improper access of left side of Either"));
}

template <typename Left, typename Right>
std::expected<std::reference_wrapper<Right>, StarException> Either<Left, Right>::right() {
  if (auto r = maybeRightRef())
    return *r;
  return std::unexpected(EitherException("Improper access of right side of Either"));
}

template <typename Left, typename Right>
std::optional<Left> Either<Left, Right>::maybeLeft() const {
  if (auto l = maybeLeftRef())
    return std::optional<Left>(std::in_place, l->get());
  return {};
}

template <typename Left, typename Right>
std::optional<Right> Either<Left, Right>::maybeRight() const {
  if (auto r = maybeRightRef())
    return std::optional<Right>(std::in_place, r->get());
  return {};
}

template <typename Left, typename Right>
Left const& Either<Left, Right>::leftRef() const {
  return std::get<LeftType>(m_value).value;
}

template <typename Left, typename Right>
Right const& Either<Left, Right>::rightRef() const {
  return std::get<RightType>(m_value).value;
}

template <typename Left, typename Right>
Left& Either<Left, Right>::leftRef() {
  return std::get<LeftType>(m_value).value;
}

template <typename Left, typename Right>
Right& Either<Left, Right>::rightRef() {
  return std::get<RightType>(m_value).value;
}

template <typename Left, typename Right>
std::optional<std::reference_wrapper<Left const>> Either<Left, Right>::maybeLeftRef() const {
  if (isLeft())
    return std::reference_wrapper<Left const>(leftRef());
  return std::nullopt;
}

template <typename Left, typename Right>
std::optional<std::reference_wrapper<Right const>> Either<Left, Right>::maybeRightRef() const {
  if (isRight())
    return std::reference_wrapper<Right const>(rightRef());
  return std::nullopt;
}

template <typename Left, typename Right>
std::optional<std::reference_wrapper<Left>> Either<Left, Right>::maybeLeftRef() {
  if (isLeft())
    return std::reference_wrapper<Left>(leftRef());
  return std::nullopt;
}

template <typename Left, typename Right>
std::optional<std::reference_wrapper<Right>> Either<Left, Right>::maybeRightRef() {
  if (isRight())
    return std::reference_wrapper<Right>(rightRef());
  return std::nullopt;
}

}

export module star.either;

export namespace Star {
  using ::Star::EitherExceptionTag;
  using ::Star::EitherException;
  using ::Star::EitherLeftValue;
  using ::Star::EitherRightValue;
  using ::Star::makeLeft;
  using ::Star::makeRight;
  using ::Star::Either;
}
