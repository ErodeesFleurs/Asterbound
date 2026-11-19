import std;
import star.string;
import star.host_address;

#include "gtest/gtest.h"

using namespace Star;

TEST(HostAddress, All) {
  auto zero4 = HostAddress::create("*");
  ASSERT_TRUE(zero4);
  auto zero6 = HostAddress::create("::");
  ASSERT_TRUE(zero6);
  auto local4 = HostAddress::create("127.0.0.1");
  ASSERT_TRUE(local4);
  auto local6 = HostAddress::create("::1");
  ASSERT_TRUE(local6);
  EXPECT_EQ(HostAddress(), *zero4);
  EXPECT_EQ(HostAddress(NetworkMode::IPv6), *zero6);
  EXPECT_TRUE(HostAddress::localhost(NetworkMode::IPv4).isLocalHost());
  EXPECT_TRUE(HostAddress::localhost(NetworkMode::IPv6).isLocalHost());
  EXPECT_TRUE(zero4->isZero());
  EXPECT_TRUE(zero6->isZero());
  EXPECT_TRUE(local4->isLocalHost());
  EXPECT_TRUE(local6->isLocalHost());
  EXPECT_EQ(zero4->mode(), NetworkMode::IPv4);
  EXPECT_EQ(zero6->mode(), NetworkMode::IPv6);
}

TEST(HostAddressWithPort, All) {
  auto zero = HostAddress::create("*");
  ASSERT_TRUE(zero);
  EXPECT_EQ(HostAddressWithPort().address(), *zero);
  EXPECT_EQ(HostAddressWithPort().port(), 0);
  for (String const& text : {String("*:80"), String(":::80"), String("[::]:80")}) {
    auto address = HostAddressWithPort::create(text);
    ASSERT_TRUE(address);
    EXPECT_EQ(address->port(), 80);
  }
  auto zero6 = HostAddressWithPort::create("[::]:80");
  ASSERT_TRUE(zero6);
  EXPECT_TRUE(zero6->address().isZero());
  auto local6 = HostAddressWithPort::create("[::1]:80");
  ASSERT_TRUE(local6);
  EXPECT_TRUE(local6->address().isLocalHost());
}
