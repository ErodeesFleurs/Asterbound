import std;
import star.ref_ptr;
import star.casting;
import star.type_identity;

#include "gtest/gtest.h"

#ifdef STAR_COMPILER_CLANG
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif 

using namespace Star;

int g_test1Count = 0;
int g_test2Count = 0;

struct Base : RefCounter {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Base, RefCounter>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return type_name<Base>(); }
};

struct Test1 : Base {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Test1, Base>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return type_name<Test1>(); }
  Test1() {
    ++g_test1Count;
  }

  ~Test1() {
    --g_test1Count;
  }
};

struct Test2 : Base {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Test2, Base>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return type_name<Test2>(); }
  Test2() {
    ++g_test2Count;
  }

  ~Test2() {
    --g_test2Count;
  }
};

TEST(IntrusivePtr, All) {
  {
    RefPtr<Base> p1 = make_ref<Test1>();
    RefPtr<Base> p2 = make_ref<Test2>();

    EXPECT_TRUE(is<Test1>(p1));
    EXPECT_FALSE(is<Test2>(p1));
    EXPECT_TRUE(is<Test2>(p2));
    EXPECT_FALSE(is<Test1>(p2));

    RefPtr<Base> p3 = p1;
    RefPtr<Base> p4 = p2;

    p3 = p3;
    std::swap(p3, p4);

    EXPECT_TRUE(is<Test1>(p4));
    EXPECT_FALSE(is<Test2>(p4));
    EXPECT_TRUE(is<Test2>(p3));
    EXPECT_FALSE(is<Test1>(p3));

    EXPECT_EQ(p3, p2);
    EXPECT_EQ(p4, p1);

    std::swap(p3, p4);

    EXPECT_EQ(p3, p1);
    EXPECT_EQ(p4, p2);

    RefPtr<Base> p5;
    EXPECT_FALSE(p5);

    EXPECT_NE(p4, p1);
    EXPECT_NE(p3, p2);
    EXPECT_NE(p3, p5);

    p5 = p2;
    p2 = std::move(p5);
    EXPECT_TRUE(is<Test2>(p2));

    RefPtr<Test1> p6 = as<Test1>(p1);
    RefPtr<Test2> p7 = as<Test2>(p2);
    RefPtr<Test2> p8 = as<Test2>(p1);
    EXPECT_TRUE((bool)p6);
    EXPECT_TRUE((bool)p7);
    EXPECT_FALSE((bool)p8);
  }

  EXPECT_EQ(0, g_test1Count);
  EXPECT_EQ(0, g_test2Count);
}

namespace {
struct QueryCounter : RefCounter {
  mutable int retains = 0;
  mutable int drops = 0;
  mutable int queries = 0;

  friend void ref_ptr_inc_ref(QueryCounter const& value) noexcept {
    ++value.retains;
    Star::ref_ptr_inc_ref(static_cast<RefCounter const&>(value));
  }
  friend void ref_ptr_dec_ref(QueryCounter const& value) noexcept {
    ++value.drops;
    Star::ref_ptr_dec_ref(static_cast<RefCounter const&>(value));
  }
};
struct QueryLeft : virtual QueryCounter {
  int left = 17;
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<QueryLeft, RefCounter>(*this, query);
  }
};

struct QueryRight : virtual QueryCounter {
  int right = 29;
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<QueryRight, RefCounter>(*this, query);
  }
};

struct QueryBoth : QueryLeft, QueryRight {
  explicit QueryBoth(bool& destroyed) : destroyed(destroyed) {}
  ~QueryBoth() { destroyed = true; }
  void query_type(TypeQuery& query) const noexcept override {
    ++queries;
    query_type_of<QueryBoth, QueryLeft, QueryRight>(*this, query);
  }
  bool& destroyed;
};
}

TEST(IntrusivePtr, VirtualCrossCastSharesObject) {
  bool destroyed = false;
  {
    RefPtr<QueryLeft> left = make_ref<QueryBoth>(destroyed);
    auto right = as<QueryRight>(left);
    ASSERT_TRUE(right);
    EXPECT_EQ(29, right->right);
    EXPECT_EQ(as<RefCounter>(left), as<RefCounter>(right));
    EXPECT_FALSE(as<Test2>(left));
    left.reset();
    EXPECT_FALSE(destroyed);
    EXPECT_EQ(29, right->right);
  }
  EXPECT_TRUE(destroyed);
}

TEST(IntrusivePtr, AdjustedRvalueQueriesOnceAndTransfersWithoutCountChurn) {
  bool destroyed = false;
  auto object = make_ref<QueryBoth>(destroyed);
  QueryRight const& expected = *object;
  RefPtr<QueryLeft const> left = std::move(object);
  ASSERT_EQ(1, left->retains);
  EXPECT_FALSE(as<Test2>(std::move(left)));
  ASSERT_TRUE(left);
  EXPECT_EQ(1, left->queries);
  auto right = as<QueryRight>(std::move(left));
  ASSERT_TRUE(right);
  EXPECT_FALSE(left);
  EXPECT_EQ(std::addressof(expected), std::addressof(*right));
  EXPECT_EQ(2, right->queries);
  EXPECT_EQ(1, right->retains);
  EXPECT_EQ(0, right->drops);
  right.reset();
  EXPECT_TRUE(destroyed);
}

TEST(Casting, SharedCrossCastAliasesOwnerAndPreservesConst) {
  static_assert(std::same_as<
      decltype(as<QueryRight>(std::declval<std::shared_ptr<QueryLeft const>>())),
      std::shared_ptr<QueryRight const>>);
  bool destroyed = false;
  auto object = std::make_shared<QueryBoth>(destroyed);
  std::weak_ptr<QueryBoth> observer = object;
  std::shared_ptr<QueryLeft> left = object;
  auto right = as<QueryRight>(left);
  ASSERT_TRUE(right);
  EXPECT_EQ(static_cast<QueryRight*>(object.get()), right.get());
  EXPECT_EQ(29, right->right);
  EXPECT_FALSE(left.owner_before(right));
  EXPECT_FALSE(right.owner_before(left));
  {
    std::shared_ptr<QueryLeft const> readOnly = left;
    auto constRight = as<QueryRight>(readOnly);
    ASSERT_TRUE(constRight);
    EXPECT_EQ(right.get(), constRight.get());
    EXPECT_EQ(29, constRight->right);
    EXPECT_FALSE(readOnly.owner_before(constRight));
    EXPECT_FALSE(constRight.owner_before(readOnly));
  }
  auto failed = as<Test2>(left);
  EXPECT_EQ(nullptr, failed.get());
  EXPECT_EQ(0, failed.use_count());
  auto rootFromLeft = as<RefCounter>(left);
  auto rootFromRight = as<RefCounter>(right);
  EXPECT_EQ(rootFromLeft.get(), rootFromRight.get());
  rootFromLeft.reset();
  rootFromRight.reset();
  object.reset();
  left.reset();
  EXPECT_FALSE(observer.expired());
  right->right = 41;
  EXPECT_EQ(41, as<QueryRight>(observer.lock())->right);
  right.reset();
  EXPECT_TRUE(observer.expired());
  EXPECT_TRUE(destroyed);
}

TEST(Casting, BorrowedCrossCastAdjustsWithoutExtendingLifetime) {
  bool destroyed = false;
  auto owner = std::make_shared<QueryBoth>(destroyed);
  QueryLeft& left = *owner;

  auto right = as_ref<QueryRight>(left);
  ASSERT_TRUE(right);
  right->get().right = 43;
  EXPECT_EQ(43, owner->right);

  auto readOnly = convert_ref<QueryRight>(std::as_const(left));
  ASSERT_TRUE(readOnly);
  EXPECT_EQ(43, readOnly->get().right);

  EXPECT_FALSE(as_ref<Test2>(left));
  auto missing = convert_ref<Test2>(left);
  ASSERT_FALSE(missing);
  EXPECT_TRUE(missing.error().is<PointerConvertException>());

  owner.reset();
  EXPECT_TRUE(destroyed);
  // Borrow values may remain in scope, but must not be dereferenced again.
}

namespace {
namespace CastingRegression {
struct Plain {};
struct PlainBase {
  int value = 13;
};
struct PlainDerived : PlainBase {};

struct Root {
  virtual ~Root() = default;
  virtual void query_type(TypeQuery& query) const noexcept { query_type_of<Root>(*this, query); }
  virtual std::string_view dynamic_type_name() const noexcept { return type_name<Root>(); }
  int value = 11;
};
struct Left : Root {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Left, Root>(*this, query); }
};
struct Right : Root {
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Right, Root>(*this, query); }
  int right = 29;
};
struct Repeated : Left, Right {
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<Repeated, Left, Right>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return type_name<Repeated>(); }
};
static_assert(!std::is_convertible_v<Repeated*, Root*>);
struct Missing : Root {};

struct Tracked : RefCounter {
  explicit Tracked(int& destructions) : destructions(destructions) {}
  ~Tracked() override { ++destructions; }
  void query_type(TypeQuery& query) const noexcept override { query_type_of<Tracked, RefCounter>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return type_name<Tracked>(); }
  int value = 37;
  int& destructions;
};
struct OtherTracked : RefCounter {};
struct WithPlainBase : PlainBase, RefCounter {};
struct NonVoidHook {
  friend int ref_ptr_inc_ref(NonVoidHook const&) noexcept { return 0; }
  friend void ref_ptr_dec_ref(NonVoidHook const&) noexcept {}
};
struct ThrowingHook {
  friend void ref_ptr_inc_ref(ThrowingHook const&) noexcept {}
  friend void ref_ptr_dec_ref(ThrowingHook const&) {}
};

struct ListNode;
static_assert(std::copy_constructible<RefPtr<ListNode>>);
struct ListNode : RefCounter {
  explicit ListNode(int& destroyed) : destroyed(destroyed) {}
  ~ListNode() override { ++destroyed; }
  int& destroyed;
  RefPtr<ListNode> next;
};

static_assert(!std::is_copy_constructible_v<OtherTracked> && !std::is_move_constructible_v<OtherTracked>);
static_assert(!std::is_copy_assignable_v<OtherTracked> && !std::is_move_assignable_v<OtherTracked>);
static_assert(intrusive_counted<ListNode> && intrusive_counted<Tracked const>);
static_assert(!intrusive_counted<NonVoidHook> && !intrusive_counted<ThrowingHook>);
static_assert(!std::constructible_from<RefPtr<PlainBase>, RefPtr<WithPlainBase>&&>);
static_assert(!cast_input<PlainBase, RefPtr<WithPlainBase>&>);
static_assert(!cast_input<PlainBase, RefPtr<WithPlainBase const>&&>);
static_assert(std::movable<DetachedRef<Tracked>> && !std::copy_constructible<DetachedRef<Tracked>>);
static_assert(!std::constructible_from<RefPtr<Tracked const>, Tracked&&>);
static_assert(!std::constructible_from<RefPtr<RefCounter const>, Tracked const&&>);
static_assert(!std::constructible_from<RefPtr<Tracked>, Tracked*>);

template <typename Target, typename Source>
concept nullable_cast_available = requires(std::optional<std::reference_wrapper<Source>> source) { as_ref<Target>(source); };
template <typename Target, typename Source>
concept reference_cast_available = requires(Source& source) { as_ref<Target>(source); };
template <typename Target, typename Source>
concept nullable_convert_available = requires(std::optional<std::reference_wrapper<Source>> source) { convert_ref<Target>(source); };
template <typename Target, typename Source>
concept reference_convert_available = requires(Source& source) { convert_ref<Target>(source); };

template <typename Target, typename Source>
concept temporary_borrow_available = requires(Source&& value) { as_ref<Target>(std::move(value)); };
template <typename Target, typename Source>
concept temporary_checked_available = requires(Source&& value) { convert_ref<Target>(std::move(value)); };

static_assert(!temporary_borrow_available<PlainBase, PlainDerived const>);
static_assert(!temporary_checked_available<PlainBase, PlainDerived const>);
static_assert(!cast_input<PlainBase, PlainDerived*>);
static_assert(capability_queryable<Left>);
static_assert(!capability_queryable<Plain>);
static_assert(!capability_castable<Root, Plain>);
static_assert(capability_castable<PlainBase, PlainDerived>);
static_assert(capability_castable<PlainBase const, PlainDerived const>);
static_assert(!capability_castable<PlainBase, PlainDerived const>);
static_assert(capability_castable<Right const, Left const>);
static_assert(!capability_castable<Right, Left const>);
static_assert(!nullable_cast_available<Root, Plain>);
static_assert(!reference_cast_available<Root, Plain>);
static_assert(!nullable_convert_available<Root, Plain>);
static_assert(!reference_convert_available<Root, Plain>);
static_assert(nullable_cast_available<PlainBase, PlainDerived>);
static_assert(reference_cast_available<PlainBase, PlainDerived>);
static_assert(nullable_convert_available<PlainBase, PlainDerived>);
static_assert(reference_convert_available<PlainBase, PlainDerived>);
static_assert(std::same_as<decltype(as_ref<PlainBase>(std::declval<std::optional<std::reference_wrapper<PlainDerived const>>>())),
                           std::optional<std::reference_wrapper<PlainBase const>>>);
static_assert(std::same_as<decltype(as_ref<Right>(std::declval<Left const&>())),
                           std::optional<std::reference_wrapper<Right const>>>);
static_assert(
  std::same_as<decltype(as<Right>(std::declval<std::shared_ptr<Left const>&&>())), std::shared_ptr<Right const>>);
static_assert(std::same_as<decltype(as<Tracked>(std::declval<RefPtr<RefCounter const>&&>())), RefPtr<Tracked const>>);
static_assert(std::constructible_from<RefPtr<RefCounter const>, RefPtr<Tracked> const&>);
static_assert(std::constructible_from<RefPtr<RefCounter const>, RefPtr<Tracked>&&>);
static_assert(!std::constructible_from<RefPtr<Tracked>, RefPtr<Tracked const> const&>);
static_assert(!std::constructible_from<RefPtr<Tracked>, RefPtr<Tracked const>&&>);
static_assert(!std::assignable_from<RefPtr<Tracked>&, RefPtr<Tracked const> const&>);
}// namespace CastingRegression
}// namespace

TEST(Casting, RepeatedBaseIsAmbiguousButStaticBranchUpcastWorks) {
  using namespace CastingRegression;
  Repeated object;
  EXPECT_NE(static_cast<Root*>(static_cast<Left*>(&object)), static_cast<Root*>(static_cast<Right*>(&object)));
  EXPECT_FALSE(query_type<Root>(object));
  EXPECT_FALSE(as_ref<Root>(std::optional{std::ref(object)}));
  auto left = std::optional{std::ref(static_cast<Left&>(object))};
  auto root = as_ref<Root>(left);
  auto right = as_ref<Right>(left);
  ASSERT_TRUE(root);
  ASSERT_TRUE(right);
  EXPECT_EQ(std::addressof(static_cast<Root&>(left->get())), std::addressof(root->get()));
  EXPECT_EQ(std::addressof(static_cast<Right&>(object)), std::addressof(right->get()));
  auto failure = convert_ref<Root>(std::optional{std::ref(object)});
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<PointerConvertException>());
}

TEST(Casting, AmbiguousChildDoesNotHideUniqueSibling) {
  using namespace CastingRegression;
  struct Single : Root {};
  struct Composite : Repeated, Single {
    void query_type(TypeQuery& query) const noexcept override {
      query_type_of<Composite, Single, Repeated>(*this, query);
    }
  } composite;
  auto found = query_type<Root>(composite);
  ASSERT_TRUE(found);
  EXPECT_EQ(std::addressof(static_cast<Root&>(static_cast<Single&>(composite))), std::addressof(found->get()));
}

TEST(Casting, PlainStaticConversionsNeedNoQueryProtocol) {
  using namespace CastingRegression;
  PlainDerived object;
  auto source = std::optional{std::ref(object)};
  auto base = as_ref<PlainBase>(source);
  ASSERT_TRUE(base);
  EXPECT_EQ(std::addressof(static_cast<PlainBase&>(object)), std::addressof(base->get()));
  auto reference = convert_ref<PlainBase>(object);
  ASSERT_TRUE(reference);
  reference->get().value = 41;
  EXPECT_EQ(41, object.value);
  auto checked = convert_ref<PlainBase>(source);
  ASSERT_TRUE(checked);
  EXPECT_EQ(std::addressof(static_cast<PlainBase&>(object)), std::addressof(checked->get()));
  source.reset();
  EXPECT_FALSE(as_ref<PlainBase>(source));
  auto failure = convert_ref<PlainBase>(source);
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<PointerConvertException>());
}

TEST(Casting, SharedConstRvalueAdjustsAndNullAliasPreservesSourceOnFailure) {
  using namespace CastingRegression;
  auto object = std::make_shared<Repeated>();
  std::shared_ptr<Left const> source = object;
  auto right = as<Right>(std::move(source));
  ASSERT_TRUE(right);
  EXPECT_FALSE(source);
  EXPECT_EQ(static_cast<Right const*>(object.get()), right.get());
  EXPECT_EQ(29, right->right);
  EXPECT_FALSE(right.owner_before(object));
  EXPECT_FALSE(object.owner_before(right));

  std::shared_ptr<Left const> nullAlias(object, nullptr);
  auto count = object.use_count();
  auto failedCopy = as<Right>(nullAlias);
  EXPECT_EQ(nullptr, failedCopy.get());
  EXPECT_EQ(0, failedCopy.use_count());
  auto failedStaticUpcast = as<Root>(std::move(nullAlias));
  EXPECT_EQ(nullptr, failedStaticUpcast.get());
  EXPECT_EQ(0, failedStaticUpcast.use_count());
  EXPECT_EQ(count, object.use_count());
  auto failed = as<Right>(std::move(nullAlias));
  EXPECT_EQ(nullptr, failed.get());
  EXPECT_EQ(0, failed.use_count());
  EXPECT_EQ(nullptr, nullAlias.get());
  EXPECT_EQ(count, object.use_count());
  EXPECT_FALSE(nullAlias.owner_before(object));
  EXPECT_FALSE(object.owner_before(nullAlias));
  auto converted = convert<Right>(std::move(nullAlias));
  ASSERT_FALSE(converted);
  EXPECT_TRUE(converted.error().is<PointerConvertException>());
  EXPECT_EQ(count, object.use_count());
  EXPECT_FALSE(nullAlias.owner_before(object));
  EXPECT_FALSE(object.owner_before(nullAlias));
}

TEST(Casting, SharedRvalueFailurePreservesNonNullSource) {
  using namespace CastingRegression;
  std::shared_ptr<Left> source = std::make_shared<Repeated>();
  auto count = source.use_count();
  EXPECT_FALSE(as<Missing>(std::move(source)));
  ASSERT_TRUE(source);
  EXPECT_EQ(count, source.use_count());
  EXPECT_EQ(11, source->value);
  auto failure = convert<Missing>(std::move(source));
  ASSERT_FALSE(failure);
  EXPECT_TRUE(failure.error().is<PointerConvertException>());
  ASSERT_TRUE(source);
  EXPECT_EQ(count, source.use_count());
}

TEST(Casting, SharedConvertCopiesLvaluesAndTransfersRvalues) {
  using namespace CastingRegression;
  std::shared_ptr<Left> source = std::make_shared<Repeated>();
  auto count = source.use_count();
  {
    auto copied = convert<Right>(std::as_const(source));
    ASSERT_TRUE(copied);
    ASSERT_TRUE(source);
    EXPECT_EQ(count + 1, source.use_count());
    EXPECT_FALSE(source.owner_before(*copied));
    EXPECT_FALSE(copied->owner_before(source));
    (*copied)->right = 43;
  }
  EXPECT_EQ(count, source.use_count());
  std::weak_ptr<Left> observer = source;
  auto moved = convert<Right>(std::move(source));
  ASSERT_TRUE(moved);
  EXPECT_FALSE(source);
  EXPECT_EQ(count, moved->use_count());
  EXPECT_EQ(43, (*moved)->right);
  EXPECT_FALSE(observer.expired());
  moved->reset();
  EXPECT_TRUE(observer.expired());
}

TEST(IntrusivePtr, ConstConversionsTransferAndKeepObjectAlive) {
  using namespace CastingRegression;
  int destructions = 0;
  RefPtr<RefCounter const> source = make_ref<Tracked>(destructions);
  {
    auto copied = convert<Tracked>(std::as_const(source));
    ASSERT_TRUE(copied);
    EXPECT_EQ(37, (*copied)->value);
    EXPECT_EQ(source, *copied);
    EXPECT_FALSE(as<OtherTracked>(std::move(source)));
    ASSERT_TRUE(source);
    auto failure = convert<OtherTracked>(std::move(source));
    ASSERT_FALSE(failure);
    EXPECT_TRUE(failure.error().is<PointerConvertException>());
    ASSERT_TRUE(source);
    auto transferred = as<Tracked>(std::move(source));
    ASSERT_TRUE(transferred);
    EXPECT_FALSE(source);
    EXPECT_EQ(*copied, transferred);
    transferred.reset();
    EXPECT_EQ(0, destructions);
  }
  EXPECT_EQ(1, destructions);
}

TEST(IntrusivePtr, ConvertRvalueTransfersAndConstAssignmentsRetainLifetime) {
  using namespace CastingRegression;
  int destructions = 0;
  RefPtr<Tracked> mutableOwner = make_ref<Tracked>(destructions);
  RefPtr<RefCounter const> copied;
  copied = mutableOwner;
  RefPtr<RefCounter const> moved;
  moved = std::move(mutableOwner);
  EXPECT_FALSE(mutableOwner);
  auto converted = convert<Tracked>(std::move(moved));
  ASSERT_TRUE(converted);
  EXPECT_FALSE(moved);
  EXPECT_EQ(37, (*converted)->value);
  copied.reset();
  EXPECT_EQ(0, destructions);
  converted->reset();
  EXPECT_EQ(1, destructions);
}

TEST(IntrusivePtr, SelfMoveAndSameAddressAdoptionConsumeExactlyOneCount) {
  using namespace CastingRegression;
  int destructions = 0;
  auto owner = make_ref<Tracked>(destructions);
  owner.adopt(owner.detach());
  auto& alias = owner;
  owner = std::move(alias);
  ASSERT_TRUE(owner);
  owner.reset(*owner);
  RefPtr<Tracked> second(*owner);
  auto token = owner.detach();
  EXPECT_FALSE(owner);
  ASSERT_TRUE(token);
  ASSERT_EQ(std::addressof(*second), std::addressof(*token));
  second.adopt(std::move(token));
  EXPECT_FALSE(token);
  ASSERT_TRUE(second);
  EXPECT_EQ(37, second->value);
  EXPECT_EQ(0, destructions);
  second.reset();
  EXPECT_EQ(1, destructions);
}

TEST(IntrusivePtr, DroppingDetachedOwnerAndAdoptingDifferentAddress) {
  using namespace CastingRegression;
  int old_destructions = 0;
  int new_destructions = 0;
  RefPtr<Tracked const> old_owner = make_ref<Tracked>(old_destructions);
  auto new_owner = make_ref<Tracked>(new_destructions);
  old_owner.adopt(new_owner.detach());
  EXPECT_FALSE(new_owner);
  EXPECT_EQ(1, old_destructions);
  {
    auto token = old_owner.detach();
    EXPECT_TRUE(token);
    EXPECT_FALSE(old_owner);
    EXPECT_EQ(0, new_destructions);
  }
  EXPECT_EQ(1, new_destructions);
}

TEST(IntrusivePtr, MovingFromOwnedMemberReleasesParentButNotChild) {
  using namespace CastingRegression;
  int parent_destructions = 0;
  int child_destructions = 0;
  auto owner = make_ref<ListNode>(parent_destructions);
  owner->next = make_ref<ListNode>(child_destructions);
  owner = std::move(owner->next);
  ASSERT_TRUE(owner);
  EXPECT_EQ(1, parent_destructions);
  EXPECT_EQ(0, child_destructions);
  EXPECT_EQ(std::addressof(child_destructions), std::addressof(owner->destroyed));
  owner.reset();
  EXPECT_EQ(1, child_destructions);
}

TEST(IntrusivePtr, UncountedBaseCanBeBorrowed) {
  using namespace CastingRegression;
  auto owner = make_ref<WithPlainBase>();
  EXPECT_TRUE(is<PlainBase>(owner));
  auto borrowed = as_ref<PlainBase>(*owner);
  ASSERT_TRUE(borrowed);
  EXPECT_EQ(std::addressof(static_cast<PlainBase&>(*owner)), std::addressof(borrowed->get()));
}

namespace {
class ProtectedIntrusive final : public RefCounter {
public:
  explicit ProtectedIntrusive(int& destroyed) : m_destroyed(destroyed) {}

protected:
  ~ProtectedIntrusive() override { ++m_destroyed.get(); }

private:
  std::reference_wrapper<int> m_destroyed;
};
}// namespace

TEST(IntrusivePtr, FactoryDoesNotRequirePublicDestruction) {
  int destroyed = 0;
  auto owner = make_ref<ProtectedIntrusive>(destroyed);
  RefPtr<ProtectedIntrusive const> copied = owner;
  owner.reset();
  EXPECT_EQ(destroyed, 0);
  copied.reset();
  EXPECT_EQ(destroyed, 1);
}

TEST(Casting, ArrayStaticBorrowAndElementOwnerPreserveStorage) {
  int values[3]{13, 17, 19};
  auto borrowed = as_ref<int[3]>(values);
  ASSERT_TRUE(borrowed);
  borrowed->get()[1] = 23;
  EXPECT_EQ(values[1], 23);
  auto owner = std::make_shared<int[]>(3);
  owner[0] = 29;
  auto element = as<int>(owner);
  ASSERT_TRUE(element);
  EXPECT_EQ(element.get(), owner.get());
  EXPECT_FALSE(element.owner_before(owner));
  EXPECT_FALSE(owner.owner_before(element));
  auto erased = as<void>(owner);
  ASSERT_TRUE(erased);
  EXPECT_EQ(erased.get(), owner.get());
  EXPECT_FALSE(erased.owner_before(owner));
  EXPECT_FALSE(owner.owner_before(erased));
  auto checked = convert<int>(std::move(owner));
  ASSERT_TRUE(checked);
  EXPECT_FALSE(owner);
  EXPECT_EQ(**checked, 29);
}
