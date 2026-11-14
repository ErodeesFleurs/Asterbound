import star.format;
import star.random;
import star.static_random;
import star.set;
import star.math_common;

import std;

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename Random, typename Range>
concept can_sample_reference = requires(Random& random, Range&& range) {
  random.sample_ref(std::forward<Range>(range));
};

template <typename Range>
concept can_static_sample_reference = requires(Range&& range) {
  static_random_from(std::forward<Range>(range), 7);
};

struct ExplicitCopy {
  int value;
  explicit ExplicitCopy(int value) : value(value) {}
  explicit ExplicitCopy(ExplicitCopy const& other) : value(other.value) {}
  ExplicitCopy(ExplicitCopy&&) = delete;
};

struct OversizedSelection {
  using value_type = int;
  std::array<int, 1> values{7};
  mutable int begin_calls = 0;
  bool empty() const { return false; }
  std::size_t size() const { return std::numeric_limits<std::size_t>::max(); }
  auto begin() { ++begin_calls; return values.begin(); }
  auto begin() const { ++begin_calls; return values.begin(); }
  auto end() { return values.end(); }
};

template <typename Random, typename Range>
concept can_sample_value = requires(Random& random, Range const& range) {
  random.sample_value(range);
};

template <typename Random, typename Range>
concept can_sample_default_value = requires(Random& random, Range const& range, typename Range::value_type const& value) {
  random.sample_value(range, value);
};

static_assert(std::uniform_random_bit_generator<RandomSource>);
static_assert(!std::is_convertible_v<std::uint64_t, RandomSource>);
static_assert(can_sample_reference<RandomSource, std::vector<int>&>);
static_assert(can_sample_reference<RandomService, std::vector<int> const&>);
static_assert(can_sample_reference<RandomSource, std::span<int>>);
static_assert(can_sample_reference<RandomService, std::span<int>>);
static_assert(!can_sample_reference<RandomSource, std::vector<int>>);
static_assert(!can_sample_reference<RandomService, std::vector<int> const>);
static_assert(!can_static_sample_reference<std::vector<int>>);
static_assert(!can_static_sample_reference<std::vector<int> const>);
static_assert(can_static_sample_reference<std::span<int>>);
static_assert(!can_sample_reference<RandomSource, std::vector<bool>&>);
static_assert(!can_sample_value<RandomSource, std::vector<std::unique_ptr<int>>>);
static_assert(!can_sample_value<RandomSource, std::array<ExplicitCopy, 1>>);
static_assert(can_sample_default_value<RandomSource, std::array<ExplicitCopy, 1>>);
static_assert(can_sample_default_value<RandomService, std::array<ExplicitCopy, 1>>);
}

TEST(StaticRandomTest, FromEmptyContainerReturnsError) {
  // size() - 1 on an empty container used to wrap around to SIZE_MAX, which
  // became -1 and then a division by zero inside static_random_i32_range.
  std::vector<int> const empty;
  auto rejected = static_random_from(empty, 1);
  ASSERT_FALSE(rejected);
  EXPECT_TRUE(rejected.error().is<StarException>());

  std::vector<int> const one{7};
  auto selected = static_random_from(one, 1);
  ASSERT_TRUE(selected);
  EXPECT_EQ(selected->get(), 7);
}

TEST(RandTest, BorrowingPreservesReadOnlyKeysAndMutableReferenceSlots) {
  auto check = []<typename Random>(Random& random)
    requires (std::same_as<Random, RandomSource> || std::same_as<Random, RandomService>)
  {
    HashSet<int> keys{7};
    auto selected = random.sample_ref(keys);
    static_assert(std::same_as<decltype(selected), std::expected<std::reference_wrapper<int const>, StarException>>);
    ASSERT_TRUE(selected);
    EXPECT_EQ(std::addressof(selected->get()), std::addressof(*keys.begin()));
    EXPECT_EQ(selected->get(), 7);
    int first = 1, second = 2;
    std::vector<std::reference_wrapper<int>> slots{std::ref(first)};
    auto slot = random.sample_ref(slots);
    ASSERT_TRUE(slot);
    slot->get() = std::ref(second);
    EXPECT_EQ(std::addressof(slots.front().get()), std::addressof(second));
    std::vector<int> mutable_values{3};
    auto mutable_value = random.sample_ref(mutable_values);
    ASSERT_TRUE(mutable_value);
    mutable_value->get() = 4;
    EXPECT_EQ(mutable_values.front(), 4);
    HashSet<int> empty;
    auto rejected = random.sample_ref(empty);
    ASSERT_FALSE(rejected);
    EXPECT_TRUE(rejected.error().template is<RandomException>());
  };
  RandomSource source(17);
  RandomService service(17);
  check(source);
  check(service);
}

TEST(RandTest, All) {
  RandomSource rand(31415926);
  EXPECT_EQ(rand.next_u32(), 2950892229u);
  EXPECT_EQ(rand.next_u32(), 1418047276u);
  EXPECT_EQ(rand.next_u32(), 3790079132u);
  EXPECT_EQ(rand.next_u32(), 445970691u);
  EXPECT_EQ(rand.next_u32(), 2728181679u);

  rand.mix_seed(27182818);
  EXPECT_EQ(rand.next_u32(), 3255590103u);
  EXPECT_EQ(rand.next_u32(), 678168874u);
  EXPECT_EQ(rand.next_u32(), 3814633989u);
  EXPECT_EQ(rand.next_u32(), 4070190392u);
  EXPECT_EQ(rand.next_u32(), 265077625u);

  for (auto i = 0; i < 1e5; ++i)
    rand.next_u32();

  EXPECT_EQ(rand.next_u32(), 724230938u);

  EXPECT_EQ(rand.uniform_float(), 0.6708741188049316f);
  EXPECT_EQ(rand.uniform_float(), 0.3297619521617889f);
  EXPECT_EQ(rand.uniform_float(), 0.2407863438129425f);
  EXPECT_EQ(rand.uniform_float(), 0.2388365715742111f);
  EXPECT_EQ(rand.uniform_float(), 0.8430468440055847f);
  EXPECT_EQ(rand.uniform_float(), 0.5036200881004333f);
  EXPECT_EQ(rand.uniform_float(), 0.2279680222272873f);

  EXPECT_EQ(rand.uniform_double(), 0.0993789769271370693193290435374);
  EXPECT_EQ(rand.uniform_double(), 0.489891395527775608265130813379);
  EXPECT_EQ(rand.uniform_double(), 0.609412270506578757078841590555);
  EXPECT_EQ(rand.uniform_double(), 0.838596715529411951273175418464);
  EXPECT_EQ(rand.uniform_double(), 0.556277078882413622551439402741);
  EXPECT_EQ(rand.uniform_double(), 0.575903901290708120086492272094);
  EXPECT_EQ(rand.uniform_double(), 0.721894899474715856513284961693);

  EXPECT_EQ(rand.next_u32(), 2870093081u);
  EXPECT_EQ(rand.next_u64(), 16492986915519838998u);
  EXPECT_EQ(rand.next_u32(), 1029635267u);
  EXPECT_EQ(rand.next_u32(), 1469630330u);
  EXPECT_EQ(rand.next_u32(), 2017291831u);
  EXPECT_EQ(rand.next_u32(), 2167938696u);
  EXPECT_EQ(rand.next_u64(), 7889337349893562706u);
  EXPECT_EQ(rand.next_u64(), 11595813817497350001u);
  EXPECT_EQ(rand.next_u64(), 14292979134113073402u);
  EXPECT_EQ(rand.next_u32(), 119058573u);

  EXPECT_EQ(rand.next_i32(), -1995152573);
  EXPECT_EQ(rand.next_i32(), 1717688829);
  EXPECT_EQ(rand.next_i64(), -4500435351487619671);
  EXPECT_EQ(rand.next_i32(), 644788487);
  EXPECT_EQ(rand.next_i64(), 2370131680533925071);
  EXPECT_EQ(rand.next_i64(), -7391462988205297660);
  EXPECT_EQ(rand.next_i32(), 817418170);
  EXPECT_EQ(rand.next_i64(), -3754584120231434991);
  EXPECT_EQ(rand.next_i64(), -2585223751692222899);

  EXPECT_EQ(rand.uniform_int(34), 20);
  EXPECT_EQ(rand.uniform_int(483), 49);
  EXPECT_EQ(rand.uniform_int(2), 1);
  EXPECT_EQ(rand.uniform_int(49382), 12751);
  EXPECT_EQ(rand.uniform_int(1291), 872);
  EXPECT_EQ(rand.uniform_int(rand.next_u32()), 306693728);
  EXPECT_EQ(rand.uniform_int(rand.next_u32()), 332940738);
  EXPECT_EQ(rand.uniform_int(rand.next_u32()), 94215324);
  EXPECT_EQ(rand.uniform_int(rand.next_u32()), 43770718);
  EXPECT_EQ(rand.uniform_int(2939), 2938u);
  EXPECT_EQ(rand.uniform_uint(rand.next_u32()), 179327438u);
  EXPECT_EQ(rand.uniform_uint(rand.next_u32()), 1761816964u);
  EXPECT_EQ(rand.uniform_uint(rand.next_u32()), 68031400u);
  EXPECT_EQ(rand.uniform_uint(3972097), 2100462u);
  EXPECT_EQ(rand.uniform_uint(878), 839u);
  EXPECT_EQ(rand.uniform_uint(rand.next_u32()), 1499799820u);
  EXPECT_EQ(rand.uniform_uint(rand.next_u32()), 1807471845u);

  {
    auto result = rand.uniform_int(83, 198207);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 90862);
  }
  {
    auto result = rand.uniform_int(-98982, -989);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, -23203);
  }
  {
    auto result = rand.uniform_int(0, 1);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 1);
  }
  {
    auto result = rand.uniform_int(-8279, rand.next_u32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 20616743);
  }
  {
    auto result = rand.uniform_int(87297, 298398);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 142455);
  }
  {
    auto result = rand.uniform_int(-93792, rand.next_u32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 734418822);
  }
  {
    auto result = rand.uniform_int(2, 5);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 3);
  }
  {
    auto result = rand.uniform_int(2938, 2940);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 2939);
  }
  {
    auto result = rand.uniform_uint(9802, 87297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 47048u);
  }
  {
    auto result = rand.uniform_uint(9809802, 372987297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 150191254u);
  }
  {
    auto result = rand.uniform_uint(9809809, 272987297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 263742306u);
  }
  {
    auto result = rand.uniform_uint(4, (std::uint64_t)(-1));
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 7288528389985641665u);
  }
  {
    auto result = rand.uniform_uint(rand.next_u32(), (std::uint64_t)(-1));
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 1748024317879856867u);
  }
  {
    auto result = rand.uniform_uint(2, rand.next_u32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 558624029u);
  }
  {
    auto result = rand.uniform_uint(9382, 888888);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 212491u);
  }

  {
    auto result = rand.uniform_float(4.3f, 4.4f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4.395795345306396f);
  }
  {
    auto result = rand.uniform_float(rand.uniform_float(), 5.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4.580977439880371f);
  }
  {
    auto result = rand.uniform_float(387.f, 3920.f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 3740.644775390625f);
  }
  {
    auto result = rand.uniform_float(rand.uniform_float(), 1.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 0.9794777631759644f);
  }
  {
    auto result = rand.uniform_float(-392.f, rand.uniform_float());
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -276.0828552246094f);
  }
  {
    auto result = rand.uniform_float(-2.f, rand.uniform_float());
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 0.1681497097015381f);
  }
  {
    auto result = rand.uniform_double(rand.uniform_double(), 1.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.942969795571236168996165361023);
  }
  {
    auto result = rand.uniform_double(rand.uniform_double(), 1.1);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.751293963391068353452340033982);
  }
  {
    auto result = rand.uniform_double(rand.uniform_double(), 83);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 9.31872432254218274749746342422);
  }
  {
    auto result = rand.uniform_double(-2, rand.uniform_double());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.361844402875284743004158372059);
  }
  {
    auto result = rand.uniform_double(-2.3, rand.uniform_double());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.580774591935332651360113231931);
  }
  {
    auto result = rand.uniform_double(-303, rand.uniform_double());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, -110.181037142766882652722415514);
  }

  EXPECT_EQ(rand.next_bool(), false);
  EXPECT_EQ(rand.next_bool(), true);
  EXPECT_EQ(rand.next_bool(), false);
  EXPECT_EQ(rand.next_bool(), true);
  EXPECT_EQ(rand.next_bool(), false);
  EXPECT_EQ(rand.next_bool(), false);

  {
    auto result = rand.normal_float(1.0f, 0.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -0.46687412f);
  }
  {
    auto result = rand.normal_float(1.5f, 4.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4.5038204f);
  }
  {
    auto result = rand.normal_float(0.1f, 3.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 2.8866563f);
  }
  {
    auto result = rand.normal_float(5.0f, -10.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -7.4856615f);
  }
  {
    auto result = rand.normal_float(rand.uniform_float(), 0.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 0.21202649f);
  }
  {
    auto result = rand.normal_float(rand.uniform_float(), 0.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -0.18832046f);
  }
  {
    auto result = rand.normal_float(rand.uniform_float(), 0.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -0.8733508f);
  }
  {
    auto result = rand.normal_double(1.0, 0.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, -1.6134212525108711);
  }
  {
    auto result = rand.normal_double(1.5, 4.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 4.1692477323762258);
  }
  {
    auto result = rand.normal_double(0.1, 3.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 2.8561578555964706);
  }
  {
    auto result = rand.normal_double(5.0, -10.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, -15.805748549670087);
  }
  {
    auto result = rand.normal_double(rand.uniform_double(), 0.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, -0.3154774175319317);
  }
  {
    auto result = rand.normal_double(rand.uniform_double(), 0.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.074425803794012854);
  }
  {
    auto result = rand.normal_double(rand.uniform_double(), 0.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.45895995279014684);
  }

  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 0);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.7), 0);
  EXPECT_EQ(rand.stochastic_round(0.7), 1);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 1);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
  EXPECT_EQ(rand.stochastic_round(0.1), 0);
}

TEST(StaticRandomTest, All) {
  EXPECT_EQ(static_random_u64("test1", 999, "test2"), 17057684957748924255u);
  EXPECT_EQ(static_random_u64("test1", 1000, "test2"), 17136762056491983648u);
  EXPECT_EQ(static_random_u64("test1", 1001, "test2"), 10826209999926048792u);
  EXPECT_EQ(static_random_u64("test1", 1002, "test2"), 10190371075442159783u);
  EXPECT_EQ(static_random_u64("test1", 1003, "test2"), 16325723287291511625u);
  EXPECT_EQ(static_random_u64("test1", 1004, "test2"), 6061201707788279217u);
  EXPECT_EQ(static_random_u64("test1", 1005, "test2"), 13034875300321135291u);

  EXPECT_EQ(static_random_u32("test1", 999, "test2"), 3893169212u);
  EXPECT_EQ(static_random_u32("test1", 1000, "test2"), 1330274955u);
  EXPECT_EQ(static_random_u32("test1", 1001, "test2"), 2268597334u);
  EXPECT_EQ(static_random_u32("test1", 1002, "test2"), 1221477368u);
  EXPECT_EQ(static_random_u32("test1", 1003, "test2"), 271894555u);
  EXPECT_EQ(static_random_u32("test1", 1004, "test2"), 2464836468u);
  EXPECT_EQ(static_random_u32("test1", 1005, "test2"), 3808877030u);
}

TEST(RandTest, InvalidParametersRejectWithoutDrawing) {
  auto check = []<typename Random>() {
    Random random(2);
    RandomSource oracle(2);
    auto nan = std::bit_cast<double>(0x7ff8000000000001ull);
    auto infinity = std::bit_cast<double>(0x7ff0000000000000ull);
    auto float_nan = std::bit_cast<float>(0x7fc00001u);
    EXPECT_FALSE(random.uniform_int(-1));
    EXPECT_FALSE(random.uniform_int(7, -7));
    EXPECT_FALSE(random.uniform_uint(7, 6));
    EXPECT_FALSE(random.uniform_float(1.0f, -1.0f));
    EXPECT_FALSE(random.uniform_float(float_nan, 1.0f));
    EXPECT_FALSE(random.uniform_double(-infinity, 1.0));
    EXPECT_FALSE(random.uniform_double(0.0, nan));
    EXPECT_FALSE(random.normal_float(-1.0f));
    EXPECT_FALSE(random.normal_float(1.0f, float_nan));
    EXPECT_FALSE(random.normal_double(infinity));
    EXPECT_FALSE(random.normal_double(1.0, nan));
    EXPECT_FALSE(random.stochastic_round(nan));
    EXPECT_FALSE(random.stochastic_round(infinity));
    EXPECT_FALSE(random.stochastic_round(0x1p63));
    EXPECT_FALSE(random.stochastic_round(-0x1p64));
    std::vector<int> empty;
    EXPECT_FALSE(random.sample_ref(empty));
    EXPECT_EQ(random.sample_value(empty), 0);
    EXPECT_EQ(random.sample_value(empty, 19), 19);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
}

TEST(RandTest, IntegerDomainsPreserveQuotientsAndState) {
  auto check = []<typename Random>() {
    constexpr auto unsigned_max = std::numeric_limits<std::uint64_t>::max();
    constexpr auto signed_min = std::numeric_limits<std::int64_t>::min();
    constexpr auto signed_max = std::numeric_limits<std::int64_t>::max();
    Random random(2);
    RandomSource oracle(2);
    EXPECT_EQ(random.uniform_uint(unsigned_max), oracle.next_u64());
    auto full_signed = random.uniform_int(signed_min, signed_max);
    ASSERT_TRUE(full_signed);
    EXPECT_EQ(*full_signed, std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(signed_min) + oracle.next_u64()));
    auto full_unsigned = random.uniform_uint(0, unsigned_max);
    ASSERT_TRUE(full_unsigned);
    EXPECT_EQ(*full_unsigned, oracle.next_u64());
    EXPECT_EQ(random.uniform_uint(0), 0u);
    oracle.next_u64();
    auto singleton = random.uniform_int(signed_min, signed_min);
    ASSERT_TRUE(singleton);
    EXPECT_EQ(*singleton, signed_min);
    oracle.next_u64();
    EXPECT_EQ(random.next_u64(), oracle.next_u64());

    random.seed(2);
    oracle.seed(2);
    // Seed 2's first candidate exceeds INT64_MAX; the old quotient returned a
    // negative signed value. Rejection must consume fresh complete draws.
    ASSERT_GT(oracle.next_u64(), static_cast<std::uint64_t>(signed_max));
    std::uint64_t accepted;
    do {
      accepted = oracle.next_u64();
    } while (accepted > static_cast<std::uint64_t>(signed_max));
    auto bounded = random.uniform_int(signed_max);
    ASSERT_TRUE(bounded);
    EXPECT_EQ(*bounded, static_cast<std::int64_t>(accepted));
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
    for (std::uint64_t bound : {1ull, 3ull, 4294967295ull, 9223372036854775808ull, 18446744073709551614ull}) {
      for (int i = 0; i < 32; ++i)
        EXPECT_LE(random.uniform_uint(bound), bound);
    }
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
}

TEST(RandTest, UniformBatchBytesAndReseedPreserveOrderedDraws) {
  auto check = []<typename Random>() {
    Random random(31415926);
    RandomSource oracle(31415926);
    std::array<float, 11> values;
    random.fill_uniform(values);
    for (float value : values)
      EXPECT_EQ(std::bit_cast<std::uint32_t>(value), std::bit_cast<std::uint32_t>(oracle.uniform_float()));
    random.fill_uniform({});
    random.fill_bytes({});
    std::array<char, 7> bytes;
    random.fill_bytes(bytes);
    for (std::size_t offset = 0; offset < bytes.size();) {
      auto bits = oracle.next_u32();
      for (std::size_t i = 0; i < 4 && offset < bytes.size(); ++i)
        EXPECT_EQ(static_cast<unsigned char>(bytes[offset++]), static_cast<unsigned char>(bits >> (8 * i)));
    }
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
    random.seed(9);
    oracle.seed(9);
    random.mix_seed(42);
    oracle.mix_seed(42);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
}

TEST(RandTest, FullWidthShuffleMatchesStandardGeneratorAndServiceSeedConsumption) {
  std::array<int, 8> values{0, 1, 2, 3, 4, 5, 6, 7};
  auto expected = values;
  RandomSource random(17), oracle(17);
  random.shuffle(values);
  std::shuffle(expected.begin(), expected.end(), oracle);
  EXPECT_EQ(values, expected);
  EXPECT_EQ(random.next_u64(), oracle.next_u64());
  RandomService service(17);
  oracle.seed(17);
  std::vector<int> empty;
  service.shuffle(empty);
  oracle.next_u64();
  EXPECT_EQ(service.next_u64(), oracle.next_u64());
  service.seed(17);
  oracle.seed(17);
  RandomSource local(oracle.next_u64());
  expected = {0, 1, 2, 3, 4, 5, 6, 7};
  values = expected;
  service.shuffle(values);
  local.shuffle(expected);
  EXPECT_EQ(values, expected);
  EXPECT_EQ(service.next_u64(), oracle.next_u64());
}

TEST(RandTest, ExplicitCopySamplingNeedsNoDefaultOrMoveConstruction) {
  std::array<ExplicitCopy, 1> values{ExplicitCopy(7)};
  ExplicitCopy fallback(19);
  RandomSource source(3);
  RandomService service(3);
  auto source_copy = source.sample_value(values, fallback);
  auto service_copy = service.sample_value(values, fallback);
  ASSERT_TRUE(source_copy);
  ASSERT_TRUE(service_copy);
  EXPECT_EQ(source_copy->value, 7);
  EXPECT_EQ(service_copy->value, 7);
  auto copied = static_random_value_from(values, 3);
  ASSERT_TRUE(copied);
  EXPECT_EQ(copied->value, 7);
  std::array<ExplicitCopy, 0> empty;
  auto source_default = source.sample_value(empty, fallback);
  auto service_default = service.sample_value(empty, fallback);
  ASSERT_TRUE(source_default);
  ASSERT_TRUE(service_default);
  EXPECT_EQ(source_default->value, 19);
  EXPECT_EQ(service_default->value, 19);
  EXPECT_FALSE(static_random_value_from(empty, 3));
  std::array<std::unique_ptr<int>, 1> owners{std::make_unique<int>(23)};
  auto selected = source.sample_ref(owners);
  ASSERT_TRUE(selected);
  auto moved = std::move(selected->get());
  EXPECT_EQ(*moved, 23);
  EXPECT_FALSE(owners.front());
}

TEST(RandTest, ExtremeRealBoundsStayFiniteAndRoundingIsChecked) {
  RandomSource random(19), oracle(19);
  constexpr auto largest = std::numeric_limits<double>::max();
  auto interpolated = random.uniform_double(-largest, largest);
  ASSERT_TRUE(interpolated);
  EXPECT_TRUE(is_finite(*interpolated));
  EXPECT_GE(*interpolated, -largest);
  EXPECT_LE(*interpolated, largest);
  oracle.uniform_double();
  EXPECT_EQ(random.next_u64(), oracle.next_u64());
  auto lower = random.stochastic_round(-0x1p63);
  ASSERT_TRUE(lower);
  EXPECT_EQ(*lower, std::numeric_limits<std::int64_t>::min());
  auto upper = random.stochastic_round(0x1.fffffffffffffp62);
  ASSERT_TRUE(upper);
  EXPECT_EQ(*upper, 9223372036854774784ll);
  auto negative = random.stochastic_round(-2.25);
  ASSERT_TRUE(negative);
  EXPECT_TRUE(*negative == -3 || *negative == -2);
}

TEST(RandTest, NormalZeroDeviationRetainsPolarDrawConsumptionAndChecksOutput) {
  auto check = []<typename Random>() {
    Random random(31);
    RandomSource oracle(31);
    auto draw = oracle.normal_float();
    ASSERT_TRUE(draw);
    auto zero = random.normal_float(0, 19);
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero, 19);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
    auto double_draw = oracle.normal_double();
    ASSERT_TRUE(double_draw);
    auto double_zero = random.normal_double(0, -23);
    ASSERT_TRUE(double_zero);
    EXPECT_EQ(*double_zero, -23);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());

    random.seed(31);
    oracle.seed(31);
    auto unit = oracle.normal_double();
    ASSERT_TRUE(unit);
    auto mean = *unit < 0 ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
    auto overflow = random.normal_double(std::numeric_limits<double>::max(), mean);
    ASSERT_FALSE(overflow);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());

    random.seed(31);
    oracle.seed(31);
    auto float_unit = oracle.normal_float();
    ASSERT_TRUE(float_unit);
    auto float_mean = *float_unit < 0 ? -std::numeric_limits<float>::max() : std::numeric_limits<float>::max();
    auto float_overflow = random.normal_float(std::numeric_limits<float>::max(), float_mean);
    ASSERT_FALSE(float_overflow);
    EXPECT_TRUE(float_overflow.error().template is<RandomException>());
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
}

TEST(StaticRandomTest, CheckedDomainsPreserveAcceptedHashMapping) {
  constexpr auto largest = std::numeric_limits<std::uint64_t>::max();
  auto hash = static_random_u64("bounds", 17);
  auto unsigned_range = static_random_u32_range(0, std::numeric_limits<std::uint32_t>::max(), "bounds", 17);
  auto signed_range = static_random_i32_range(std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max(), "bounds", 17);
  ASSERT_TRUE(unsigned_range);
  ASSERT_TRUE(signed_range);
  auto offset = hash / (largest / (std::uint64_t{1} << 32));
  ASSERT_LE(offset, std::numeric_limits<std::uint32_t>::max());
  EXPECT_EQ(*unsigned_range, offset);
  EXPECT_EQ(*signed_range, std::bit_cast<std::int32_t>(std::uint32_t{0x80000000} + static_cast<std::uint32_t>(offset)));
  EXPECT_EQ(static_random_i32_range(-7, -7, 17), -7);
  EXPECT_FALSE(static_random_i32_range(1, -1, 17));
  EXPECT_FALSE(static_random_u32_range(1, 0, 17));
  // An explicit tail digest deterministically exercises retry derivation.
  auto candidate = largest;
  std::uint64_t retry = 0;
  while (candidate > largest / 2)
    candidate = static_random_u64(largest, ++retry);
  EXPECT_GT(retry, 0u);
  EXPECT_EQ(static_random_bounded(largest, largest / 2), candidate);
  EXPECT_EQ(static_random_bounded(largest, largest), largest);
}

TEST(StaticRandomTest, EmptyShuffleBorrowedViewsAndReferenceSlotsAreSafe) {
  std::vector<int> empty;
  EXPECT_TRUE(static_random_shuffle(empty, 7));
  std::array<int, 1> one{19};
  EXPECT_TRUE(static_random_shuffle(one, 7));
  EXPECT_EQ(one.front(), 19);
  auto selected = static_random_from(std::span<int>(one), 7);
  ASSERT_TRUE(selected);
  EXPECT_EQ(std::addressof(selected->get()), std::addressof(one.front()));
  HashSet<int> keys{23};
  auto key = static_random_from(keys, 7);
  static_assert(std::same_as<decltype(key), std::expected<std::reference_wrapper<int const>, StarException>>);
  ASSERT_TRUE(key);
  EXPECT_EQ(key->get(), 23);
  int first = 1, second = 2;
  std::array<std::reference_wrapper<int>, 1> slots{std::ref(first)};
  auto slot = static_random_from(slots, 7);
  ASSERT_TRUE(slot);
  slot->get() = std::ref(second);
  EXPECT_EQ(std::addressof(slots.front().get()), std::addressof(second));
}

TEST(RandTest, OversizedSelectionRejectsBeforeDrawingOrIterating) {
  auto check = []<typename Random>() {
    Random random(7);
    RandomSource oracle(7);
    OversizedSelection values;
    EXPECT_FALSE(random.sample_ref(values));
    EXPECT_FALSE(random.sample_value(values));
    EXPECT_FALSE(random.sample_value(values, 19));
    EXPECT_EQ(values.begin_calls, 0);
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
  OversizedSelection values;
  EXPECT_FALSE(static_random_from(values, 7));
  EXPECT_FALSE(static_random_value_from(values, 7));
  EXPECT_FALSE(static_random_shuffle(values, 7));
  EXPECT_EQ(values.begin_calls, 0);
}

TEST(StaticRandomTest, ExtremeRealInterpolationPreservesFiniteBounds) {
  constexpr auto float_max = std::numeric_limits<float>::max();
  constexpr auto double_max = std::numeric_limits<double>::max();
  auto single = static_random_float_range(-float_max, float_max, 17);
  auto precise = static_random_double_range(-double_max, double_max, 17);
  ASSERT_TRUE(single);
  ASSERT_TRUE(precise);
  EXPECT_TRUE(is_finite(*single));
  EXPECT_TRUE(is_finite(*precise));
  EXPECT_GE(*single, -float_max);
  EXPECT_LE(*single, float_max);
  EXPECT_GE(*precise, -double_max);
  EXPECT_LE(*precise, double_max);
}

TEST(RandTest, ShuffleAcceptsStandardRandomAccessRangesWithoutMemberAccess) {
  int values[]{0, 1, 2, 3, 4};
  int expected[]{0, 1, 2, 3, 4};
  RandomSource source(29), oracle(29);
  source.shuffle(values);
  std::shuffle(std::begin(expected), std::end(expected), oracle);
  EXPECT_TRUE(std::equal(std::begin(values), std::end(values), std::begin(expected)));
  EXPECT_EQ(source.next_u64(), oracle.next_u64());

  RandomService service(29);
  oracle.seed(29);
  RandomSource local(oracle.next_u64());
  service.shuffle(values);
  local.shuffle(expected);
  EXPECT_TRUE(std::equal(std::begin(values), std::end(values), std::begin(expected)));
  EXPECT_EQ(service.next_u64(), oracle.next_u64());
}

TEST(RandTest, NormalRejectsAnActualZeroRadiusPairWithoutLosingStreamOrder) {
  auto check = []<typename Random>() {
    constexpr std::uint64_t seed = 52339836760827ull;
    Random random(seed);
    RandomSource oracle(seed);
    ASSERT_EQ(oracle.uniform_float(), 0.5f);
    ASSERT_EQ(oracle.uniform_float(), 0.5f);
    auto expected = oracle.normal_float(2, 3);
    auto actual = random.normal_float(2, 3);
    ASSERT_TRUE(expected);
    ASSERT_TRUE(actual);
    EXPECT_EQ(std::bit_cast<std::uint32_t>(*actual), std::bit_cast<std::uint32_t>(*expected));
    EXPECT_EQ(random.next_u64(), oracle.next_u64());
  };
  check.template operator()<RandomSource>();
  check.template operator()<RandomService>();
}

namespace {
struct SamplingCopyHandshake {
  std::mutex mutex;
  std::condition_variable condition;
  bool copy_started = false;
  bool draw_completed = false;
  bool draw_completed_during_copy = false;
  std::uint64_t drawn_value = 0;
};

struct SamplingCopyItem {
  int value = 0;
  std::optional<std::reference_wrapper<SamplingCopyHandshake>> handshake;

  SamplingCopyItem() = default;
  SamplingCopyItem(int value, SamplingCopyHandshake& handshake) : value(value), handshake(handshake) {}
  explicit SamplingCopyItem(SamplingCopyItem const& other) : value(other.value), handshake(other.handshake) {
    if (!handshake)
      return;
    auto& state = handshake->get();
    std::unique_lock lock(state.mutex);
    state.copy_started = true;
    state.condition.notify_all();
    state.draw_completed_during_copy = state.condition.wait_for(lock, std::chrono::seconds(2), [&] {
      return state.draw_completed;
    });
  }
  SamplingCopyItem(SamplingCopyItem&&) = delete;
};
}

TEST(RandTest, ServiceSamplingDoesNotHoldStreamLockDuringElementCopy) {
  for (bool with_default : {false, true}) {
    RandomService service(73);
    RandomSource oracle(73);
    SamplingCopyHandshake state;
    std::array<SamplingCopyItem, 3> items{
      SamplingCopyItem(11, state), SamplingCopyItem(22, state), SamplingCopyItem(33, state)};
    SamplingCopyItem fallback;
    auto selected_index = oracle.uniform_uint(items.size() - 1);
    auto expected_worker_draw = oracle.next_u64();
    std::latch worker_ready(1);
    std::thread worker([&] {
      {
        std::unique_lock lock(state.mutex);
        worker_ready.count_down();
        state.condition.wait(lock, [&] { return state.copy_started; });
      }
      auto drawn = service.next_u64();
      {
        std::lock_guard lock(state.mutex);
        state.drawn_value = drawn;
        state.draw_completed = true;
      }
      state.condition.notify_all();
    });
    worker_ready.wait();
    auto selected = with_default ? service.sample_value(items, fallback) : service.sample_value(items);
    // Even a failed selection must release the waiting worker before assertions.
    {
      std::lock_guard lock(state.mutex);
      state.copy_started = true;
    }
    state.condition.notify_all();
    worker.join();

    ASSERT_TRUE(selected);
    EXPECT_EQ(selected->value, items[selected_index].value);
    EXPECT_TRUE(state.draw_completed_during_copy);
    EXPECT_EQ(state.drawn_value, expected_worker_draw);
    EXPECT_EQ(service.next_u64(), oracle.next_u64());
  }
}
