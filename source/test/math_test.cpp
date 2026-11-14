import std;
import star.math_common;
import star.interpolation;
import star.vector;
import star.matrix3;
import star.format;

#include "gtest/gtest.h"

using namespace Star;

TEST(Math, FiniteBoundaryClassificationSurvivesFastMath) {
  for (auto value : {0.0, -0.0, 1.0, -1.0, std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()})
    EXPECT_TRUE(isFinite(value));
  for (auto bits : {0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull, 0xfff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    EXPECT_FALSE(isFinite(std::bit_cast<double>(static_cast<std::uint64_t>(input))));
  }
  for (auto value : {0.0f, -0.0f, 1.0f, -1.0f, std::numeric_limits<float>::max(), std::numeric_limits<float>::denorm_min()})
    EXPECT_TRUE(isFinite(value));
  for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u, 0xffc00001u}) {
    volatile std::uint32_t input = bits;
    EXPECT_FALSE(isFinite(std::bit_cast<float>(static_cast<std::uint32_t>(input))));
  }
}

TEST(Math, CycleIncrement) {
  int a = 0;
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 10);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 11);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 12);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 13);
  a = cycleIncrement(a, 10, 13);
  ASSERT_EQ(a, 10);
  int b = 14;
  b = cycleIncrement(b, 10, 13);
  ASSERT_EQ(b, 10);
}

TEST(Math, GenericLerpPreservesArithmetic) {
  EXPECT_EQ(lerp(0.25f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(4.0f, -2.0f));
  EXPECT_EQ(lerp(2.0f, Vec2F(2.0f, -4.0f), Vec2F(10.0f, 4.0f)), Vec2F(18.0f, 12.0f));
  EXPECT_EQ(lerp(0.25f, -3, 3), -1);
}

TEST(Math, ClampedFloatToBytePreservesSaturationAndTruncation) {
  EXPECT_EQ(floatToByte(-1.0f, true), 0);
  EXPECT_EQ(floatToByte(0.0f, true), 0);
  EXPECT_EQ(floatToByte(0.5f, true), 127);
  EXPECT_EQ(floatToByte(1.0f, true), 255);
  EXPECT_EQ(floatToByte(2.0f, true), 255);
}

TEST(Math, LegacyClampPreservesReversedBounds) {
  // Runtime-configured ranges can put the upper bound below the lower bound.
  // These calls intentionally retain the project policy rather than std::clamp.
  EXPECT_EQ(clamp(-5, 0, -2), -2);
  EXPECT_EQ(clamp(1, 0, -2), -2);
  EXPECT_EQ(clamp(-5.0f, 0.0f, -2.0f), -2.0f);
  EXPECT_EQ(clamp(1.0f, 0.0f, -2.0f), -2.0f);
}

TEST(Matrix3, RangeCopyPreservesRowMajorValuesAndDestinationBounds) {
  std::array<int, 9> input{1, 2, 3, 4, 5, 6, 7, 8, 9};
  Matrix3<int> matrix(input);
  input[3] = 99;
  EXPECT_EQ(2, matrix[0][1]);
  EXPECT_EQ(4, matrix[1][0]);

  std::array<int, 11> output;
  output.fill(-1);
  matrix.copy_to(std::span(output).subspan<1, 9>());
  EXPECT_EQ(output, (std::array<int, 11>{-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1}));
}

namespace {

template <typename M>
concept has_matrix_inverse = requires(M const& matrix) { matrix.inverse(); };

template <typename M>
concept has_matrix_sum = requires(M const& matrix) { matrix + matrix; };

template <typename M>
concept has_matrix_product = requires(M const& matrix) { matrix * matrix; };

struct StoredOnly {
  int value = 0;
};

struct AddOnly {
  int value = 0;
  AddOnly() = default;
  explicit constexpr AddOnly(int value) : value(value) {}
  AddOnly(AddOnly const&) = delete;
  AddOnly& operator=(AddOnly const&) = delete;
  AddOnly(AddOnly&&) = default;
  AddOnly& operator=(AddOnly&&) = default;
  friend constexpr AddOnly operator+(AddOnly const& left, AddOnly const& right) {
    return AddOnly(left.value + right.value);
  }
};

struct CopyOnly {
  int value = 0;
  CopyOnly() = default;
  explicit constexpr CopyOnly(int value) : value(value) {}
  CopyOnly(CopyOnly const&) = default;
  CopyOnly& operator=(CopyOnly const&) = default;
  CopyOnly& operator=(CopyOnly&&) = delete;
  friend constexpr CopyOnly operator+(CopyOnly const& left, CopyOnly const& right) {
    return CopyOnly(left.value + right.value);
  }
  friend constexpr CopyOnly operator*(CopyOnly const& left, CopyOnly const& right) {
    return CopyOnly(left.value * right.value);
  }
};

struct ColumnSource {
  int value = 0;
  ColumnSource() = default;
  ColumnSource(ColumnSource const&) = delete;
  ColumnSource& operator=(ColumnSource const&) = delete;
  ColumnSource(ColumnSource&&) = default;
  ColumnSource& operator=(ColumnSource&&) = default;
};

struct ColumnDestination {
  int value = 0;
  ColumnDestination() = default;
  explicit ColumnDestination(ColumnSource const& source) : value(source.value) {}
  ColumnDestination(ColumnDestination const&) = delete;
  ColumnDestination& operator=(ColumnDestination const&) = delete;
  ColumnDestination(ColumnDestination&&) = default;
  ColumnDestination& operator=(ColumnDestination&&) = default;
};

static_assert(std::is_default_constructible_v<Matrix3<StoredOnly>>);
static_assert(!has_matrix_sum<Matrix3<StoredOnly>>);
static_assert(!has_matrix_product<Matrix3<StoredOnly>>);
static_assert(has_matrix_sum<Matrix3<AddOnly>>);
static_assert(!has_matrix_product<Matrix3<AddOnly>>);
static_assert(!has_matrix_product<Matrix3<CopyOnly>>);
static_assert(matrix_product<CopyOnly, CopyOnly>);
static_assert(matrix_point_transform<CopyOnly, CopyOnly, CopyOnly>);
static_assert(matrix_direction_transform<CopyOnly, CopyOnly, CopyOnly>);
static_assert(has_matrix_inverse<Mat3F> && has_matrix_inverse<Mat3D>);
static_assert(!has_matrix_inverse<Matrix3<int>>);
static_assert(std::same_as<decltype(std::declval<Mat3D&>()[0]), Vec3D&>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>()[0]), Vec3D const&>);
static_assert(std::same_as<decltype(std::declval<Mat3D&&>()[0]), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&&>()[0]), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>().column(0)), Vec3D>);
static_assert(std::same_as<decltype(std::declval<Mat3D const&>().trace()), double>);
static_assert(sizeof(Mat3F) == 9 * sizeof(float));
static_assert(sizeof(Mat3D) == 9 * sizeof(double));
static_assert(Matrix3<int>::identity().trace() == 3);
static_assert(Matrix3<int>().trace() == 0);
static_assert(noexcept(std::declval<Mat3F&>()[0]));
static_assert(noexcept(std::declval<Mat3F const&>().copy_to(std::declval<std::span<float, 9>>())));

void expect_matrix_near(Mat3D const& actual, Mat3D const& expected, double tolerance = 1e-12) {
  for (std::size_t r = 0; r != 3; ++r)
    for (std::size_t c = 0; c != 3; ++c)
      EXPECT_NEAR(actual[r][c], expected[r][c], tolerance) << r << ',' << c;
}

}

TEST(Matrix3, ScalarCapabilitiesAreOperationSpecific) {
  Matrix3<StoredOnly> stored;
  stored[1][2].value = 17;
  EXPECT_EQ(stored[1][2].value, 17);

  Matrix3<AddOnly> first, second;
  first[0][2].value = 4;
  second[0][2].value = 7;
  auto sum = first + second;
  EXPECT_EQ(sum[0][2].value, 11);

  Vector<ColumnSource, 3> source;
  source[0].value = 3;
  source[1].value = 5;
  source[2].value = 7;
  Matrix3<ColumnDestination> destination;
  destination.set_column(1, source);
  EXPECT_EQ(destination[0][1].value, 3);
  EXPECT_EQ(destination[1][1].value, 5);
  EXPECT_EQ(destination[2][1].value, 7);
}

TEST(Matrix3, ValueTransformsSupportCopyOnlyCoordinatesWithoutMoveAssignment) {
  Matrix3<CopyOnly> matrix;
  matrix[0][0].value = 2;
  matrix[0][1].value = 3;
  matrix[0][2].value = 5;
  matrix[1][0].value = -1;
  matrix[1][1].value = 4;
  matrix[1][2].value = 7;
  matrix[2][2].value = 1;
  Vector<CopyOnly, 2> input;
  input[0].value = 2;
  input[1].value = 3;
  auto point = matrix.transform_point(input);
  auto direction = matrix.transform_direction(input);
  auto multiplied = matrix * input;
  EXPECT_EQ(point[0].value, 18);
  EXPECT_EQ(point[1].value, 17);
  EXPECT_EQ(direction[0].value, 13);
  EXPECT_EQ(direction[1].value, 10);
  EXPECT_EQ(multiplied[0].value, 18);
  EXPECT_EQ(multiplied[1].value, 17);
  Vector<CopyOnly, 3> homogeneous;
  homogeneous[0].value = 2;
  homogeneous[1].value = 3;
  homogeneous[2].value = 1;
  auto product = matrix * homogeneous;
  EXPECT_EQ(product[0].value, 18);
  EXPECT_EQ(product[1].value, 17);
  EXPECT_EQ(product[2].value, 1);
}

TEST(Matrix3, ValueAccessAndCrossTypeConversionPreserveCoordinates) {
  Matrix3<int> original(1, 2, 3, 4, 5, 6, 7, 8, 9);
  Mat3D converted(original);
  EXPECT_EQ(converted.row(1), Vec3D(4, 5, 6));
  EXPECT_EQ(std::as_const(converted).column(2), Vec3D(3, 6, 9));
  EXPECT_EQ(converted.trace(), 15);
  EXPECT_EQ(converted.diagonal(), Vec3D(1, 5, 9));
  auto row = converted.row(0);
  row[0] = -1;
  EXPECT_EQ(converted[0][0], 1);
  Mat3F assigned;
  assigned = converted;
  EXPECT_EQ(assigned[2], Vec3F(7, 8, 9));
  assigned.set_row(2, Vec3D(10.25, 11.5, 12.75));
  EXPECT_EQ(assigned[2], Vec3F(10.25f, 11.5f, 12.75f));
  assigned.set_column(0, Vec3D(13, 14, 15));
  EXPECT_EQ(assigned.column(0), Vec3F(13, 14, 15));
  auto saved_temporary_row = Mat3D::identity()[1];
  EXPECT_EQ(saved_temporary_row, Vec3D(0, 1, 0));
  EXPECT_EQ(transpose(original), (Matrix3<int>(1, 4, 7, 2, 5, 8, 3, 6, 9)));
  EXPECT_EQ(original[0], (Vector<int, 3>(1, 2, 3)));
}

TEST(Matrix3, GeneralProductsAndSelfMultiplicationUseAllSourceValues) {
  Matrix3<int> a(2, 1, 3, 0, 3, 1, 1, 0, 4);
  Matrix3<int> b(1, 2, 0, 3, 1, 2, 2, 0, 1);
  EXPECT_EQ(a * b, (Matrix3<int>(11, 5, 5, 11, 3, 7, 9, 2, 4)));
  EXPECT_EQ((a * Vector<int, 3>(2, 3, 4)), (Vector<int, 3>(19, 13, 18)));
  a *= a;
  EXPECT_EQ(a, (Matrix3<int>(7, 5, 19, 1, 9, 7, 6, 1, 19)));
}

TEST(Matrix3, AliasedScalarAndColumnInputsAreSnapshottedBeforeWrites) {
  Matrix3<int> original(2, 4, 6, 8, 10, 12, 14, 16, 18);
  auto multiplied = original;
  multiplied *= multiplied[0][0];
  EXPECT_EQ(multiplied, (Matrix3<int>(4, 8, 12, 16, 20, 24, 28, 32, 36)));
  auto divided = original;
  divided /= divided[0][0];
  EXPECT_EQ(divided, (Matrix3<int>(1, 2, 3, 4, 5, 6, 7, 8, 9)));
  original.set_column(1, original[0]);
  EXPECT_EQ(original, (Matrix3<int>(2, 2, 6, 8, 4, 12, 14, 6, 18)));
}

TEST(Matrix3, CheckedInverseMatchesIndependentCofactorsAndPreservesSource) {
  Mat3D matrix(2, 1, 3, 0, 3, 1, 1, 0, 4);
  auto original = matrix;
  Mat3D expected(12.0/16, -4.0/16, -8.0/16, 1.0/16, 5.0/16, -2.0/16, -3.0/16, 1.0/16, 6.0/16);
  auto inverted = inverse(matrix);
  ASSERT_TRUE(inverted);
  expect_matrix_near(*inverted, expected);
  EXPECT_EQ(matrix, original);
  expect_matrix_near(matrix * *inverted, Mat3D::identity());
  expect_matrix_near(*inverted * matrix, Mat3D::identity());
  ASSERT_TRUE(matrix.invert());
  expect_matrix_near(matrix, expected);

  Mat3D pivoting(0, 1, 0, 1, 0, 0, 0, 0, -2);
  auto pivoted = pivoting.inverse();
  ASSERT_TRUE(pivoted);
  expect_matrix_near(*pivoted, Mat3D(0, 1, 0, 1, 0, 0, 0, 0, -0.5));
}

TEST(Matrix3, CheckedInverseHandlesRepresentableExtremeScalesAndTranslation) {
  for (double scale : {1e-150, -1e-150, 1e150, -1e150}) {
    Mat3D matrix(scale, 0, 0, 0, scale, 0, 0, 0, scale);
    auto result = matrix.inverse();
    ASSERT_TRUE(result);
    for (std::size_t i = 0; i != 3; ++i)
      EXPECT_NEAR((*result)[i][i] / (1 / scale), 1, 1e-14);
    expect_matrix_near(matrix * *result, Mat3D::identity());
  }
  Mat3D disparate(1e-200, 0, 0, 0, 1e200, 0, 0, 0, 1);
  auto result = disparate.inverse();
  ASSERT_TRUE(result);
  EXPECT_NEAR((*result)[0][0] / 1e200, 1, 1e-14);
  EXPECT_NEAR((*result)[1][1] / 1e-200, 1, 1e-14);
  expect_matrix_near(disparate * *result, Mat3D::identity());
  auto translation = Mat3D::translation(Vec2D(1e200, -1e200)).inverse();
  ASSERT_TRUE(translation);
  EXPECT_EQ((*translation)[0][2], -1e200);
  EXPECT_EQ((*translation)[1][2], 1e200);
}

TEST(Matrix3, SingularAndNonfiniteCheckedFailuresPreserveTarget) {
  for (Mat3D matrix : {Mat3D(), Mat3D(1, 2, 3, 2, 4, 6, 0, 0, 1)}) {
    auto saved = matrix;
    auto result = matrix.invert();
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<MathException>());
    EXPECT_EQ(matrix, saved);
  }
  for (auto bits : {0x7ff0000000000000ull, 0x7ff8000000000001ull}) {
    volatile std::uint64_t input = bits;
    Mat3D matrix = Mat3D::identity();
    matrix[1][2] = std::bit_cast<double>(static_cast<std::uint64_t>(input));
    std::array<double, 9> before, after;
    matrix.copy_to(before);
    EXPECT_FALSE(matrix.invert());
    EXPECT_FALSE(matrix.orthonormalize());
    EXPECT_FALSE(matrix.is_orthogonal(1e-10));
    matrix.copy_to(after);
    for (std::size_t i = 0; i != 9; ++i)
      EXPECT_EQ(std::bit_cast<std::uint64_t>(before[i]), std::bit_cast<std::uint64_t>(after[i]));
  }
  Mat3D overflowing(1e-310, 0, 0, 0, 1, 0, 0, 0, 1);
  auto saved = overflowing;
  EXPECT_FALSE(overflowing.invert());
  EXPECT_EQ(overflowing, saved);
}

TEST(Matrix3, OrthogonalityChecksFullGramMatrixAndAcceptsReflections) {
  EXPECT_FALSE(Mat3D(2, 0, 0, 0, 0.5, 0, 0, 0, 1).is_orthogonal(1e-12));
  EXPECT_FALSE(Mat3D(1, 1, 0, 0, 1, 0, 0, 0, 1).is_orthogonal(1e-12));
  Mat3D reflection(-1, 0, 0, 0, 1, 0, 0, 0, 1);
  EXPECT_TRUE(reflection.is_orthogonal(0));
  EXPECT_TRUE(Mat3D::rotation(0.7).is_orthogonal(1e-12));
  EXPECT_FALSE(reflection.is_orthogonal(-1));
  auto result = reflection.orthonormalized();
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, reflection);
  EXPECT_EQ(result->determinant(), -1);
}

TEST(Matrix3, OrthonormalizationProjectsThirdRowAgainstBothEarlierRows) {
  Mat3D matrix(1, 0, 0, 1, 1, 0, 1, 1, 1);
  auto original = matrix;
  auto result = orthonormalized(matrix);
  ASSERT_TRUE(result);
  expect_matrix_near(*result, Mat3D::identity());
  EXPECT_EQ(matrix, original);
  ASSERT_TRUE(matrix.orthonormalize());
  expect_matrix_near(matrix, Mat3D::identity());
  for (Mat3D invalid : {Mat3D(), Mat3D(1, 0, 0, 2, 0, 0, 0, 0, 1),
      Mat3D(1, 1, 1, 1, 1, 1, 0, 0, 1),
      Mat3D(1, 2, 3, 2, 4, 6, 3, 6, 9),
      Mat3D(1, 2, 3, 2, 0, 1, 3, 2, 4)}) {
    auto saved = invalid;
    EXPECT_FALSE(invalid.orthonormalize());
    EXPECT_EQ(invalid, saved);
  }
  Mat3D nearly_dependent(1, 0, 0, 1, 1e-8, 0, 0, 0, 1);
  EXPECT_TRUE(nearly_dependent.orthonormalized());
  EXPECT_TRUE(nearly_dependent.orthonormalized(0));
  EXPECT_TRUE(nearly_dependent.orthonormalized(1e-9));
  EXPECT_TRUE(orthonormalized(nearly_dependent, 1e-9));
  EXPECT_FALSE(orthonormalized(nearly_dependent, 1e-6));
  auto saved = nearly_dependent;
  EXPECT_FALSE(nearly_dependent.orthonormalize(1e-6));
  EXPECT_EQ(nearly_dependent, saved);
  EXPECT_FALSE(nearly_dependent.orthonormalize(-1));
  EXPECT_EQ(nearly_dependent, saved);
  Mat3D large(1e200, 0, 0, 1e200, 1e200, 0, 1e200, 1e200, -1e200);
  auto large_result = large.orthonormalized();
  ASSERT_TRUE(large_result);
  expect_matrix_near(*large_result, Mat3D(1, 0, 0, 0, 1, 0, 0, 0, -1));
}

TEST(Matrix3, AffineKernelsPreserveGeneralThirdRowAndLeftMultiplication) {
  Mat3D matrix(2, 1, 3, 0, 3, 1, 1, 0, 4);
  auto translated = matrix;
  translated.translate(Vec2D(5, -2));
  EXPECT_EQ(translated, Mat3D(7, 1, 23, -2, 3, -7, 1, 0, 4));
  auto scaled = matrix;
  scaled.scale(Vec2D(2, 3), Vec2D(1, 2));
  EXPECT_EQ(scaled, Mat3D(3, 2, 2, -4, 9, -13, 1, 0, 4));
  auto rotated = matrix;
  rotated.rotate(std::numbers::pi / 2, Vec2D(1, 2));
  expect_matrix_near(rotated, Mat3D(3, -3, 11, 3, 1, 7, 1, 0, 4));
}

TEST(Matrix3, MixedTransformsConvertOnlyFinalCoordinatesAndIgnoreDirectionTranslation) {
  Matrix3<int> integral(2, 3, 5, -1, 4, 7, 9, 8, 6);
  EXPECT_EQ(integral.transform_point(Vec2D(0.25, 0.5)), Vec2D(7, 8.75));
  EXPECT_EQ(integral.transform_direction(Vec2D(0.25, 0.5)), Vec2D(2, 1.75));
  EXPECT_EQ(integral * Vec2D(0.25, 0.5), (Vector<int, 2>(7, 8)));
  Mat3D fractional(0.75, 0.75, 0.75, -0.75, 0.75, -0.75, 0, 0, 1);
  EXPECT_EQ(fractional.transform_point(Vector<int, 2>(1, 1)), (Vector<int, 2>(2, 0)));
  EXPECT_EQ(fractional.transform_direction(Vector<int, 2>(1, 1)), (Vector<int, 2>(1, 0)));
  Mat3D rotation = Mat3D::rotation(0.4);
  rotation.translate(Vec2D(1e200, -1e200));
  EXPECT_NEAR(rotation.transform_angle(0.3), 0.7, 1e-14);
  EXPECT_FLOAT_EQ(rotation.transform_angle(0.3f), static_cast<float>(0.4 + static_cast<double>(0.3f)));
  Mat3D reflection(-1, 0, 100, 0, 1, 200, 0, 0, 1);
  EXPECT_NEAR(reflection.transform_angle(0.0), std::numbers::pi, 1e-14);
}

TEST(Matrix3, UncheckedGeometryRetainsLongDoublePrecision) {
  using Matrix = Matrix3<long double>;
  static_assert(!has_matrix_inverse<Matrix>);
  long double epsilon = std::numeric_limits<long double>::epsilon();
  long double angle = 1.0L + 16 * epsilon;
  auto rotation = Matrix::rotation(angle);
  EXPECT_LE(std::fabs(rotation[1][0] - std::sin(angle)), 2 * epsilon);
  EXPECT_LE(std::fabs(rotation[0][0] - std::cos(angle)), 2 * epsilon);
  Matrix rotated = Matrix::identity();
  rotated.rotate(angle);
  EXPECT_LE(std::fabs(rotated[1][0] - std::sin(angle)), 2 * epsilon);
  EXPECT_LE(std::fabs(rotated[0][0] - std::cos(angle)), 2 * epsilon);
  auto identity = Matrix::identity();
  static_assert(std::same_as<decltype(identity.transform_angle(angle)), long double>);
  EXPECT_LE(std::fabs(identity.transform_angle(angle) - angle), 4 * epsilon);
  auto offset = Matrix::rotation(0.125L);
  EXPECT_LE(std::fabs(offset.transform_angle(angle) - (angle + 0.125L)), 4 * epsilon);
  if constexpr (std::numeric_limits<long double>::digits > std::numeric_limits<double>::digits) {
    long double rounded_angle = static_cast<long double>(static_cast<double>(angle));
    EXPECT_GT(std::fabs(rotation[1][0] - std::sin(rounded_angle)), 2 * epsilon);
    EXPECT_GT(std::fabs(identity.transform_angle(angle) - rounded_angle), 4 * epsilon);
  }
}
