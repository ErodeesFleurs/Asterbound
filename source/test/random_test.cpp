import star.format;
import star.random;
import star.static_random;

import std;

#include "gtest/gtest.h"

using namespace Star;

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

TEST(RandTest, All) {
  RandomSource rand(31415926);
  EXPECT_EQ(rand.randu32(), 2950892229u);
  EXPECT_EQ(rand.randu32(), 1418047276u);
  EXPECT_EQ(rand.randu32(), 3790079132u);
  EXPECT_EQ(rand.randu32(), 445970691u);
  EXPECT_EQ(rand.randu32(), 2728181679u);

  rand.addEntropy(27182818);
  EXPECT_EQ(rand.randu32(), 3255590103u);
  EXPECT_EQ(rand.randu32(), 678168874u);
  EXPECT_EQ(rand.randu32(), 3814633989u);
  EXPECT_EQ(rand.randu32(), 4070190392u);
  EXPECT_EQ(rand.randu32(), 265077625u);

  for (auto i = 0; i < 1e5; ++i)
    rand.randu32();

  EXPECT_EQ(rand.randu32(), 724230938u);

  EXPECT_EQ(rand.randf(), 0.6708741188049316f);
  EXPECT_EQ(rand.randf(), 0.3297619521617889f);
  EXPECT_EQ(rand.randf(), 0.2407863438129425f);
  EXPECT_EQ(rand.randf(), 0.2388365715742111f);
  EXPECT_EQ(rand.randf(), 0.8430468440055847f);
  EXPECT_EQ(rand.randf(), 0.5036200881004333f);
  EXPECT_EQ(rand.randf(), 0.2279680222272873f);

  EXPECT_EQ(rand.randd(), 0.0993789769271370693193290435374);
  EXPECT_EQ(rand.randd(), 0.489891395527775608265130813379);
  EXPECT_EQ(rand.randd(), 0.609412270506578757078841590555);
  EXPECT_EQ(rand.randd(), 0.838596715529411951273175418464);
  EXPECT_EQ(rand.randd(), 0.556277078882413622551439402741);
  EXPECT_EQ(rand.randd(), 0.575903901290708120086492272094);
  EXPECT_EQ(rand.randd(), 0.721894899474715856513284961693);

  EXPECT_EQ(rand.randu32(), 2870093081u);
  EXPECT_EQ(rand.randu64(), 16492986915519838998u);
  EXPECT_EQ(rand.randu32(), 1029635267u);
  EXPECT_EQ(rand.randu32(), 1469630330u);
  EXPECT_EQ(rand.randu32(), 2017291831u);
  EXPECT_EQ(rand.randu32(), 2167938696u);
  EXPECT_EQ(rand.randu64(), 7889337349893562706u);
  EXPECT_EQ(rand.randu64(), 11595813817497350001u);
  EXPECT_EQ(rand.randu64(), 14292979134113073402u);
  EXPECT_EQ(rand.randu32(), 119058573u);

  EXPECT_EQ(rand.randi32(), -1995152573);
  EXPECT_EQ(rand.randi32(), 1717688829);
  EXPECT_EQ(rand.randi64(), -4500435351487619671);
  EXPECT_EQ(rand.randi32(), 644788487);
  EXPECT_EQ(rand.randi64(), 2370131680533925071);
  EXPECT_EQ(rand.randi64(), -7391462988205297660);
  EXPECT_EQ(rand.randi32(), 817418170);
  EXPECT_EQ(rand.randi64(), -3754584120231434991);
  EXPECT_EQ(rand.randi64(), -2585223751692222899);

  EXPECT_EQ(rand.randInt(34), 20);
  EXPECT_EQ(rand.randInt(483), 49);
  EXPECT_EQ(rand.randInt(2), 1);
  EXPECT_EQ(rand.randInt(49382), 12751);
  EXPECT_EQ(rand.randInt(1291), 872);
  EXPECT_EQ(rand.randInt(rand.randu32()), 306693728);
  EXPECT_EQ(rand.randInt(rand.randu32()), 332940738);
  EXPECT_EQ(rand.randInt(rand.randu32()), 94215324);
  EXPECT_EQ(rand.randInt(rand.randu32()), 43770718);
  EXPECT_EQ(rand.randInt(2939), 2938u);
  EXPECT_EQ(rand.randUInt(rand.randu32()), 179327438u);
  EXPECT_EQ(rand.randUInt(rand.randu32()), 1761816964u);
  EXPECT_EQ(rand.randUInt(rand.randu32()), 68031400u);
  EXPECT_EQ(rand.randUInt(3972097), 2100462u);
  EXPECT_EQ(rand.randUInt(878), 839u);
  EXPECT_EQ(rand.randUInt(rand.randu32()), 1499799820u);
  EXPECT_EQ(rand.randUInt(rand.randu32()), 1807471845u);

  {
    auto result = rand.randInt(83, 198207);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 90862);
  }
  {
    auto result = rand.randInt(-98982, -989);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, -23203);
  }
  {
    auto result = rand.randInt(0, 1);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 1);
  }
  {
    auto result = rand.randInt(-8279, rand.randu32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 20616743);
  }
  {
    auto result = rand.randInt(87297, 298398);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 142455);
  }
  {
    auto result = rand.randInt(-93792, rand.randu32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 734418822);
  }
  {
    auto result = rand.randInt(2, 5);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 3);
  }
  {
    auto result = rand.randInt(2938, 2940);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 2939);
  }
  {
    auto result = rand.randUInt(9802, 87297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 47048u);
  }
  {
    auto result = rand.randUInt(9809802, 372987297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 150191254u);
  }
  {
    auto result = rand.randUInt(9809809, 272987297);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 263742306u);
  }
  {
    auto result = rand.randUInt(4, (std::uint64_t)(-1));
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 7288528389985641665u);
  }
  {
    auto result = rand.randUInt(rand.randu32(), (std::uint64_t)(-1));
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 1748024317879856867u);
  }
  {
    auto result = rand.randUInt(2, rand.randu32());
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 558624029u);
  }
  {
    auto result = rand.randUInt(9382, 888888);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, 212491u);
  }

  {
    auto result = rand.randf(4.3f, 4.4f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4.395795345306396f);
  }
  {
    auto result = rand.randf(rand.randf(), 5.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4.580977439880371f);
  }
  {
    auto result = rand.randf(387.f, 3920.f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 3740.644775390625f);
  }
  {
    auto result = rand.randf(rand.randf(), 1.0f);
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 0.9794777631759644f);
  }
  {
    auto result = rand.randf(-392.f, rand.randf());
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -276.0828552246094f);
  }
  {
    auto result = rand.randf(-2.f, rand.randf());
    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 0.1681497097015381f);
  }
  {
    auto result = rand.randd(rand.randd(), 1.0);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.942969795571236168996165361023);
  }
  {
    auto result = rand.randd(rand.randd(), 1.1);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.751293963391068353452340033982);
  }
  {
    auto result = rand.randd(rand.randd(), 83);
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 9.31872432254218274749746342422);
  }
  {
    auto result = rand.randd(-2, rand.randd());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.361844402875284743004158372059);
  }
  {
    auto result = rand.randd(-2.3, rand.randd());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, 0.580774591935332651360113231931);
  }
  {
    auto result = rand.randd(-303, rand.randd());
    ASSERT_TRUE(result);
    EXPECT_DOUBLE_EQ(*result, -110.181037142766882652722415514);
  }

  EXPECT_EQ(rand.randb(), false);
  EXPECT_EQ(rand.randb(), true);
  EXPECT_EQ(rand.randb(), false);
  EXPECT_EQ(rand.randb(), true);
  EXPECT_EQ(rand.randb(), false);
  EXPECT_EQ(rand.randb(), false);

  EXPECT_FLOAT_EQ(rand.nrandf(1.0f, 0.0f), -0.46687412f);
  EXPECT_FLOAT_EQ(rand.nrandf(1.5f, 4.0f), 4.5038204f);
  EXPECT_FLOAT_EQ(rand.nrandf(0.1f, 3.0f), 2.8866563f);
  EXPECT_FLOAT_EQ(rand.nrandf(5.0f, -10.0f), -7.4856615f);
  EXPECT_FLOAT_EQ(rand.nrandf(rand.randf(), 0.0f), 0.21202649f);
  EXPECT_FLOAT_EQ(rand.nrandf(rand.randf(), 0.0f), -0.18832046f);
  EXPECT_FLOAT_EQ(rand.nrandf(rand.randf(), 0.0f), -0.8733508f);
  EXPECT_DOUBLE_EQ(rand.nrandd(1.0, 0.0), -1.6134212525108711);
  EXPECT_DOUBLE_EQ(rand.nrandd(1.5, 4.0), 4.1692477323762258);
  EXPECT_DOUBLE_EQ(rand.nrandd(0.1, 3.0), 2.8561578555964706);
  EXPECT_DOUBLE_EQ(rand.nrandd(5.0, -10.0), -15.805748549670087);
  EXPECT_DOUBLE_EQ(rand.nrandd(rand.randd(), 0.0), -0.3154774175319317);
  EXPECT_DOUBLE_EQ(rand.nrandd(rand.randd(), 0.0), 0.074425803794012854);
  EXPECT_DOUBLE_EQ(rand.nrandd(rand.randd(), 0.0), 0.45895995279014684);

  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 0);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.7), 0);
  EXPECT_EQ(rand.stochasticRound(0.7), 1);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 1);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
  EXPECT_EQ(rand.stochasticRound(0.1), 0);
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
