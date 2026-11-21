
import std;
import star.format;
#include "gtest/gtest.h"

import star.data_stream;
import star.data_stream_devices;
import star.list;
import star.string;
import star.vector;
import star.data_stream_extra;
import star.interpolation;
import star.net_compatibility;
import star.net_element;
import star.net_element_basic_fields;
import star.net_element_float_fields;
import star.net_element_containers;
import star.net_element_dynamic_group;
import star.net_element_group;
import star.net_element_signal;
import star.net_element_system;
import star.net_element_top;

import star.math_common;
using namespace Star;

// Use a copyable leaf: noncopyable group bases would mask a movable publisher.
static_assert(!std::is_copy_constructible_v<NetElementTop<NetElementFloat>>);
static_assert(!std::is_move_constructible_v<NetElementTop<NetElementFloat>>);
static_assert(!std::is_copy_assignable_v<NetElementTop<NetElementFloat>>);
static_assert(!std::is_move_assignable_v<NetElementTop<NetElementFloat>>);

TEST(NetElements, FilteredFloatDeltaLeavesValueAndCursorUntouched) {
  NetElementFloat field;
  field.set_compatibility_version(1);
  field.set(2.0f);
  DataStreamBuffer wire;
  ASSERT_TRUE(wire.write(5.0f));
  ASSERT_TRUE(wire.seek(0));
  ASSERT_TRUE(field.read_net_delta(wire, 0.0f, LegacyVersion));
  EXPECT_EQ(field.get(), 2.0f);
  EXPECT_EQ(wire.pos(), 0u);
  ASSERT_TRUE(field.read_net_delta(wire, 0.0f));
  EXPECT_EQ(field.get(), 5.0f);
}

TEST(NetElements, DataRounding) {
  NetElementFloat masterField1;
  NetElementFloat masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));

  masterField1.set_fixed_point_base(0.1f);
  masterField2.set_fixed_point_base(0.5);

  masterField1.set(2.1999f);
  masterField2.set(100.04);

  // Check to make sure encoded data is actually sent with the expected
  // limitations to the client side.

  NetElementFloat slaveField1;
  NetElementFloat slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));

  slaveField1.set_fixed_point_base(0.1f);
  slaveField2.set_fixed_point_base(0.5);

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));

  EXPECT_LT(std::fabs(slaveField1.get() - 2.2f), 0.00001f);
  EXPECT_LT(std::fabs(slaveField2.get() - 100), 0.0000001);

  // Make sure that jittering a fixed point or limited value doesn't cause
  // extra deltas

  masterField1.set(2.155f);
  masterField1.set(2.24f);
  masterField2.set(99.96);
  masterField2.set(100.00);

  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_TRUE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first));

  masterField1.set(10.0f);
  masterField2.set(50.0f);

  auto masterUpdate3 = master.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate3->first));

  EXPECT_LT(std::fabs(slaveField1.get() - 10.0f), 0.00001f);
  EXPECT_LT(std::fabs(slaveField2.get() - 50.0f), 0.0000001);
}

TEST(NetElements, DirectWriteRead) {
  NetElementUInt masterField1;
  NetElementUInt masterField2;
  NetElementUInt masterField3;
  NetElementUInt masterField4;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));
  ASSERT_TRUE(master.add_net_element(masterField4));

  masterField1.set(1);
  masterField2.set(2);
  masterField3.set(3);
  masterField4.set(4);

  NetElementUInt slaveField1;
  NetElementUInt slaveField2;
  NetElementUInt slaveField3;
  NetElementUInt slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));
  ASSERT_TRUE(slave.add_net_element(slaveField3));
  ASSERT_TRUE(slave.add_net_element(slaveField4));

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));

  EXPECT_EQ(slaveField1.get(), 1u);
  EXPECT_EQ(slaveField2.get(), 2u);
  EXPECT_EQ(slaveField3.get(), 3u);
  EXPECT_EQ(slaveField4.get(), 4u);

  masterField1.set(10);
  masterField3.set(30);

  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first));

  EXPECT_EQ(slaveField1.get(), 10u);
  EXPECT_EQ(slaveField2.get(), 2u);
  EXPECT_EQ(slaveField3.get(), 30u);
  EXPECT_EQ(slaveField4.get(), 4u);

  masterField2.set(20);
  masterField4.set(40);

  auto masterUpdate3 = master.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slave.read_net_state(masterUpdate3->first));

  EXPECT_EQ(slaveField1.get(), 10u);
  EXPECT_EQ(slaveField2.get(), 20u);
  EXPECT_EQ(slaveField3.get(), 30u);
  EXPECT_EQ(slaveField4.get(), 40u);
}

TEST(NetElements, DeltaSize) {
  NetElementInt masterField1;
  NetElementUInt masterField2;
  NetElementUInt masterField3;
  NetElementSize masterField4;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));
  ASSERT_TRUE(master.add_net_element(masterField4));

  NetElementInt slaveField1;
  NetElementUInt slaveField2;
  NetElementUInt slaveField3;
  NetElementSize slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));
  ASSERT_TRUE(slave.add_net_element(slaveField3));
  ASSERT_TRUE(slave.add_net_element(slaveField4));

  masterField1.set(10);
  masterField2.set(20);
  masterField3.set(30);
  masterField4.set(40);

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), 20u);
  EXPECT_EQ(masterField3.get(), 30u);
  EXPECT_EQ(masterField4.get(), 40u);

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);

  // Initial state should be 5 bytes, 1 byte for header, then 4 1 byte values.
  EXPECT_EQ(masterUpdate1->first.size(), 5u);

  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));
  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 20u);
  EXPECT_EQ(slaveField3.get(), 30u);
  EXPECT_EQ(slaveField4.get(), 40u);

  masterField1.set(50);
  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);

  // Second delta should be not include any other data than the single 1 byte
  // changed state, so make sure that it is 1 byte for header, 1 byte for field
  // number, 1 byte for state, 1 byte for end marker.
  EXPECT_EQ(masterUpdate2->first.size(), 4u);

  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first));
  EXPECT_EQ(slaveField1.get(), 50);
}

TEST(NetElements, GroupDeltaUsesCurrentlyVisibleFields) {
  for (auto protocol : {LegacyVersion, OpenProtocolVersion + 1}) {
    SCOPED_TRACE(protocol);
    NetCompatibilityRules rules(protocol);
    NetElementVersion version;
    std::array<NetElementUInt, 5> sent, received;
    NetElementGroup sender, receiver;
    sender.init_net_version(std::cref(version));
    auto set_visible = [&](std::size_t index, bool visible) {
      auto minimum = visible ? AnyVersion : protocol + 1;
      sent[index].set_compatibility_version(minimum);
      received[index].set_compatibility_version(minimum);
    };
    for (std::size_t i = 0; i < sent.size(); ++i) {
      set_visible(i, i == 1);
      ASSERT_TRUE(sender.add_net_element(sent[i]));
      ASSERT_TRUE(receiver.add_net_element(received[i]));
      sent[i].set(10 + i);
    }

    DataStreamBuffer wire;
    EXPECT_EQ(sender.write_net_delta(wire, 0, rules), true);
    EXPECT_TRUE(std::ranges::equal(wire.span(), std::array<char, 1>{11}));
    ASSERT_TRUE(wire.seek(0));
    ASSERT_TRUE(receiver.read_net_delta(wire, 0.0f, rules));
    EXPECT_EQ(received[1].get(), 11u);

    set_visible(1, false);
    wire.clear();
    EXPECT_EQ(sender.write_net_delta(wire, 0, rules), false);
    EXPECT_TRUE(wire.empty());
    ASSERT_TRUE(wire.write<std::uint8_t>(0));
    ASSERT_TRUE(wire.seek(0));
    EXPECT_FALSE(receiver.read_net_delta(wire, 0.0f, rules));
    EXPECT_EQ(wire.pos(), 0u);

    // Registration hasn't changed, but the second visible field is now dirty.
    set_visible(1, true);
    set_visible(3, true);
    auto from_version = version.increment();
    sent[3].set(42);
    wire.clear();
    EXPECT_EQ(sender.write_net_delta(wire, from_version, rules), true);
    EXPECT_TRUE(std::ranges::equal(wire.span(), std::array<char, 3>{2, 42, 0}));
    ASSERT_TRUE(wire.seek(0));
    ASSERT_TRUE(receiver.read_net_delta(wire, 0.0f, rules));
    EXPECT_EQ(received[3].get(), 42u);
    EXPECT_EQ(received[2].get(), 0u);
  }
}

namespace {
template <typename T>
concept has_default_net_field = requires { typename NetElementData<T>; };

template <typename Mutator>
concept updates_int_field = requires(NetElementInt& field, Mutator&& mutator) {
  field.update(std::forward<Mutator>(mutator));
};

struct ContextualFlag {
  bool changed;
  explicit operator bool() const { return changed; }
};

struct FieldMutator {
  FieldMutator() = default;
  FieldMutator(FieldMutator const&) = delete;
  ContextualFlag operator()(std::int64_t& value) & { return {++value == 2}; }
};

struct RvalueMutator {
  bool operator()(std::int64_t&) &&;
};

struct CustomPayload {
  std::uint8_t count = 0;
};

static_assert(updates_int_field<FieldMutator>);
static_assert(!updates_int_field<RvalueMutator>);
static_assert(!updates_int_field<decltype([](std::int64_t&) {})>);
static_assert(!has_default_net_field<CustomPayload>);
static_assert(has_default_net_field<String>);
}

TEST(NetElements, MutatorUsesLvalueAndContextualBool) {
  NetElementInt field;
  FieldMutator mutator;
  field.update(mutator);
  EXPECT_FALSE(field.pull_updated());
  field.update(FieldMutator{});
  EXPECT_TRUE(field.pull_updated());
  EXPECT_EQ(field.get(), 2);
}

TEST(NetElements, CustomCodecForwardsPendingWireValue) {
  auto reader = [](DataStream& stream, CustomPayload& value) { return stream.read(value.count); };
  auto writer = [](DataStream& stream, CustomPayload const& value) { return stream.write(value.count); };
  NetElementCustomData<CustomPayload> field(reader, writer);
  field.push({3});
  ASSERT_TRUE(field.enable_net_interpolation());
  DataStreamBuffer wire;
  ASSERT_TRUE(wire.write<std::uint8_t>(91));
  ASSERT_TRUE(wire.seek(0));
  ASSERT_TRUE(field.read_net_delta(wire, 1.0f));
  ASSERT_EQ(field.get().count, 3);
  wire.clear();
  ASSERT_TRUE(field.net_store(wire));
  EXPECT_EQ(field.write_net_delta(wire, 0), true);
  EXPECT_TRUE(std::ranges::equal(wire.span(), std::array<char, 2>{91, 91}));
  ASSERT_TRUE(field.tick_net_interpolation(1.0f));
  EXPECT_EQ(field.get().count, 91);

  NetElementCustomData<CustomPayload> missing({}, {});
  wire.clear();
  EXPECT_FALSE(missing.net_load(wire, {}));
  EXPECT_FALSE(missing.net_store(wire));
  EXPECT_TRUE(wire.empty());
}

TEST(NetElements, Forwarding) {
  NetElementInt masterField1;
  NetElementData<String> masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));

  NetElementInt forwarderField1;
  NetElementData<String> forwarderField2;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.add_net_element(forwarderField1));
  ASSERT_TRUE(forwarder.add_net_element(forwarderField2));

  NetElementInt slaveField1;
  NetElementData<String> slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));

  masterField1.set(413);
  masterField2.set("foo");

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.read_net_state(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.write_net_state();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.read_net_state(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 413);
  EXPECT_EQ(forwarderField2.get(), "foo");
  EXPECT_EQ(slaveField1.get(), 413);
  EXPECT_EQ(slaveField2.get(), "foo");
}

TEST(NetElements, StepForwarding) {
  NetElementInt masterField1;
  NetElementInt masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));

  NetElementInt forwarderField1;
  NetElementInt forwarderField2;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.add_net_element(forwarderField1));
  ASSERT_TRUE(forwarder.add_net_element(forwarderField2));

  NetElementInt slaveField1;
  NetElementInt slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));

  // First emulate store / load that would happen in entity initialization.

  masterField1.set(10);
  masterField2.set(20);

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.read_net_state(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.write_net_state();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.read_net_state(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 20);

  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 20);

  // Then, update one field and transmit that delta[ to the forwarder.

  masterField1.set(413);

  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(forwarder.read_net_state(masterUpdate2->first));
  EXPECT_EQ(forwarderField1.get(), 413);

  auto forwarderUpdate2 = forwarder.write_net_state(forwarderUpdate1->second);
  ASSERT_TRUE(forwarderUpdate2);
  ASSERT_TRUE(slave.read_net_state(forwarderUpdate2->first));
  EXPECT_EQ(slaveField1.get(), 413);

  EXPECT_EQ(forwarderField1.get(), 413);
  EXPECT_EQ(forwarderField2.get(), 20);

  EXPECT_EQ(slaveField1.get(), 413);
  EXPECT_EQ(slaveField2.get(), 20);
}

TEST(NetElements, InterpolationForwarding) {
  NetElementInt masterField1;
  NetElementFloat masterField2;
  NetElementData<String> masterField3;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));

  NetElementInt forwarderField1;
  NetElementFloat forwarderField2;
  NetElementData<String> forwarderField3;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.add_net_element(forwarderField1));
  ASSERT_TRUE(forwarder.add_net_element(forwarderField2));
  ASSERT_TRUE(forwarder.add_net_element(forwarderField3));

  NetElementInt slaveField1;
  NetElementFloat slaveField2;
  NetElementData<String> slaveField3;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));
  ASSERT_TRUE(slave.add_net_element(slaveField3));

  ASSERT_TRUE(forwarder.enable_net_interpolation());
  ASSERT_TRUE(slave.enable_net_interpolation());

  masterField1.set(10);
  masterField2.set(10.0f);
  masterField3.set("10");

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.read_net_state(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.write_net_state();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.read_net_state(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 10.0f);
  EXPECT_EQ(forwarderField3.get(), "10");

  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 10.0f);
  EXPECT_EQ(slaveField3.get(), "10");

  masterField1.set(20);
  masterField2.set(20.0f);
  masterField3.set("20");

  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(forwarder.read_net_state(masterUpdate2->first, 1.0f));

  // Forwarder should not be updated yet, still 1.0f interpolationTime behind
  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 10.0f);
  EXPECT_EQ(forwarderField3.get(), "10");

  // But the forwarder should STILL forward the absolute latest data to the
  // slave
  auto forwarderUpdate2 = forwarder.write_net_state(forwarderUpdate1->second);
  ASSERT_TRUE(forwarderUpdate2);
  ASSERT_TRUE(slave.read_net_state(forwarderUpdate2->first, 1.0f));

  // Slave should not be updated yet, still 1.0f interpolationTime behind
  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 10.0f);
  EXPECT_EQ(slaveField3.get(), "10");

  // After ticking forward interpolation, both the forwarder and the slave
  // should both pick up the new values.

  ASSERT_TRUE(forwarder.tick_net_interpolation(1.0f));
  EXPECT_EQ(forwarderField1.get(), 20);
  EXPECT_EQ(forwarderField2.get(), 20.0f);
  EXPECT_EQ(forwarderField3.get(), "20");

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_EQ(slaveField1.get(), 20);
  EXPECT_EQ(slaveField2.get(), 20.0f);
  EXPECT_EQ(slaveField3.get(), "20");
}

TEST(NetElements, MasterSetGet) {
  // Make sure that Master mode sets, gets, and pullEventOccurred work
  // properly.

  NetElementInt masterField1;
  NetElementData<String> masterField2;
  NetElementEvent masterField3;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));

  EXPECT_EQ(masterField3.pull_occurrences(), 0u);

  masterField1.set(10);
  masterField2.set("foo");

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), "foo");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pull_occurrences(), 2u);
  EXPECT_EQ(masterField3.pull_occurrences(), 0u);
}

TEST(NetElements, IsolatedSetGet) {
  // Make sure fields work without being connected

  NetElementInt masterField1;
  NetElementData<String> masterField2;
  NetElementEvent masterField3;

  EXPECT_EQ(masterField2.pull_updated(), false);
  EXPECT_EQ(masterField3.pull_occurrences(), 0u);

  masterField1.set(10);
  masterField2.set("foo");

  EXPECT_EQ(masterField2.pull_updated(), true);
  EXPECT_EQ(masterField2.pull_updated(), false);

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), "foo");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pull_occurrences(), 2u);
  EXPECT_EQ(masterField3.pull_occurrences(), 0u);

  masterField1.set(20);
  masterField2.set("bar");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pull_occurrences(), 2u);
}

TEST(NetElements, EventTest) {
  NetElementData<String> masterField1;
  NetElementEvent masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));

  NetElementData<String> slaveField1;
  NetElementEvent slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));

  // Events should always start with occurred false.
  EXPECT_EQ(slaveField1.pull_updated(), false);
  EXPECT_EQ(slaveField2.pull_occurrences(), 0u);

  masterField1.set("foo");
  masterField2.trigger();
  EXPECT_TRUE(masterField1.pull_updated());
  EXPECT_FALSE(masterField1.pull_updated());

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));

  EXPECT_TRUE(slaveField1.pull_updated());
  EXPECT_FALSE(slaveField1.pull_updated());
  EXPECT_EQ(slaveField2.pull_occurrences(), 1u);
  EXPECT_EQ(slaveField2.pull_occurrences(), 0u);

  // Delta should be empty, nothing happened on step 1.
  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_TRUE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first));

  EXPECT_EQ(slaveField2.pull_occurrences(), 0u);

  masterField2.trigger();

  auto masterUpdate3 = master.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate3->first));

  EXPECT_EQ(slaveField2.pull_occurrences(), 1u);
  EXPECT_EQ(slaveField2.pull_occurrences(), 0u);
}

TEST(NetElements, FieldUpdated) {
  NetElementData<String> masterField1;
  NetElementInt masterField2;
  NetElementEvent masterField3;
  NetElementEvent masterField4;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));
  ASSERT_TRUE(master.add_net_element(masterField4));

  NetElementData<String> slaveField1;
  NetElementInt slaveField2;
  NetElementEvent slaveField3;
  NetElementEvent slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));
  ASSERT_TRUE(slave.add_net_element(slaveField3));
  ASSERT_TRUE(slave.add_net_element(slaveField4));

  slaveField4.set_ignore_occurrences_on_net_load(true);

  masterField1.set("foo");

  masterField3.trigger();
  masterField4.trigger();

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));

  EXPECT_EQ(slaveField1.pull_updated(), true);
  EXPECT_EQ(slaveField1.get(), "foo");
  EXPECT_EQ(slaveField1.pull_updated(), false);

  EXPECT_EQ(masterField3.pull_occurrences(), 1u);
  EXPECT_EQ(slaveField3.pull_occurrences(), 1u);

  // Ignore occurrences on full load should stop slaveField4 from getting any
  // occurrences
  EXPECT_EQ(masterField4.pull_occurrences(), 1u);
  EXPECT_EQ(slaveField4.pull_occurrences(), 0u);

  masterField1.set("baz");

  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_FALSE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first));

  EXPECT_EQ(slaveField1.get(), "baz");
  EXPECT_EQ(slaveField1.pull_updated(), true);

  masterField1.set("bar");

  auto masterUpdate3 = master.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate3->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pull_updated(), true);

  masterField1.push("bar");

  auto masterUpdate4 = master.write_net_state(masterUpdate3->second);
  ASSERT_TRUE(masterUpdate4);
  EXPECT_FALSE(masterUpdate4->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate4->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pull_updated(), true);

  auto masterUpdate5 = master.write_net_state(masterUpdate4->second);
  ASSERT_TRUE(masterUpdate5);
  EXPECT_TRUE(masterUpdate5->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate5->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pull_updated(), false);

  masterField3.trigger();
  masterField3.trigger();

  auto masterUpdate6 = master.write_net_state(masterUpdate5->second);
  ASSERT_TRUE(masterUpdate6);
  EXPECT_FALSE(masterUpdate6->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate6->first));

  slaveField3.ignore_occurrences();
  // occurrence should not come through after "ignore_occurrences"
  EXPECT_EQ(slaveField3.pull_occurrences(), 0u);
  EXPECT_EQ(masterField3.pull_occurrences(), 2u);

  masterField3.trigger();
  masterField3.trigger();
  masterField3.ignore_occurrences();

  auto masterUpdate7 = master.write_net_state(masterUpdate6->second);
  ASSERT_TRUE(masterUpdate7);
  EXPECT_FALSE(masterUpdate7->first.empty());
  ASSERT_TRUE(slave.read_net_state(masterUpdate7->first));

  // ignore_occurrences is LOCAL only, so events should still go through to the
  // slave
  EXPECT_EQ(masterField3.pull_occurrences(), 0u);
  EXPECT_EQ(slaveField3.pull_occurrences(), 2u);
  EXPECT_EQ(slaveField3.pull_occurrences(), 0u);
}

TEST(NetElements, Interpolation) {
  NetElementFloat masterField1;
  NetElementData<String> masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));

  masterField1.set(1.0f);
  masterField2.set("yes");

  NetElementFloat slaveField1;
  NetElementData<String> slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));

  slaveField1.set_interpolator(lerp<double, double>);

  ASSERT_TRUE(slave.enable_net_interpolation());

  auto masterUpdate1 = master.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.read_net_state(masterUpdate1->first));

  masterField1.set(2.0f);
  masterField2.set("no");
  auto masterUpdate2 = master.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slave.read_net_state(masterUpdate2->first, 2.0));

  masterField1.set(3.0f);
  masterField2.set("yes");
  auto masterUpdate3 = master.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slave.read_net_state(masterUpdate3->first, 4.0));

  masterField1.set(4.0f);
  masterField2.set("no");
  auto masterUpdate4 = master.write_net_state(masterUpdate3->second);
  ASSERT_TRUE(masterUpdate4);
  ASSERT_TRUE(slave.read_net_state(masterUpdate4->first, 6.0));

  EXPECT_TRUE(std::fabs(slaveField1.get() - 1.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pull_updated(), true);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 1.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pull_updated(), false);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 2.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "no");
  EXPECT_EQ(slaveField2.pull_updated(), true);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 2.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "no");
  EXPECT_EQ(slaveField2.pull_updated(), false);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 3.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pull_updated(), true);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 3.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pull_updated(), false);

  ASSERT_TRUE(slave.tick_net_interpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 4.0) < 0.001);
  EXPECT_EQ(slaveField2.pull_updated(), true);
  EXPECT_EQ(slaveField2.get(), "no");
}

enum class TestEnum {
  Value1,
  Value2,
  Value3
};

TEST(NetElements, AllTypes) {
  NetElementInt masterField1;
  NetElementUInt masterField2;
  NetElementSize masterField3;
  NetElementFloat masterField4;
  NetElementDouble masterField5;
  NetElementFloat masterField6;
  masterField6.set_fixed_point_base(0.01f);
  NetElementDouble masterField7;
  masterField7.set_fixed_point_base(0.01);
  NetElementBool masterField8;
  NetElementEnum<TestEnum> masterField9;
  NetElementEvent masterField10;
  NetElementData<Vec2F> masterField11;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.add_net_element(masterField1));
  ASSERT_TRUE(master.add_net_element(masterField2));
  ASSERT_TRUE(master.add_net_element(masterField3));
  ASSERT_TRUE(master.add_net_element(masterField4));
  ASSERT_TRUE(master.add_net_element(masterField5));
  ASSERT_TRUE(master.add_net_element(masterField6));
  ASSERT_TRUE(master.add_net_element(masterField7));
  ASSERT_TRUE(master.add_net_element(masterField8));
  ASSERT_TRUE(master.add_net_element(masterField9));
  ASSERT_TRUE(master.add_net_element(masterField10));
  ASSERT_TRUE(master.add_net_element(masterField11));

  masterField1.set(567);
  masterField2.set(17000);
  masterField3.set(22222);
  masterField4.set(1.55f);
  masterField5.set(1.12345678910111213);
  masterField6.set(2000.62f);
  masterField7.set(2000.62);
  masterField8.set(true);
  masterField9.set(TestEnum::Value2);
  masterField10.trigger();
  masterField11.set(Vec2F(2.0f, 2.0f));

  EXPECT_EQ(masterField1.get(), 567);
  EXPECT_EQ(masterField2.get(), 17000u);
  EXPECT_EQ(masterField3.get(), 22222u);
  EXPECT_FLOAT_EQ(masterField4.get(), 1.55f);
  EXPECT_FLOAT_EQ(masterField5.get(), 1.12345678910111213);
  EXPECT_FLOAT_EQ(masterField6.get(), 2000.62f);
  EXPECT_FLOAT_EQ(masterField7.get(), 2000.62);
  EXPECT_EQ(masterField8.get(), true);
  EXPECT_EQ(masterField9.get(), TestEnum::Value2);
  EXPECT_TRUE(masterField10.pull_occurred());
  EXPECT_TRUE(masterField11.get() == Vec2F(2.0f, 2.0f));

  NetElementInt slaveField1;
  NetElementUInt slaveField2;
  NetElementSize slaveField3;
  NetElementFloat slaveField4;
  NetElementDouble slaveField5;
  NetElementFloat slaveField6;
  slaveField6.set_fixed_point_base(0.01f);
  NetElementDouble slaveField7;
  slaveField7.set_fixed_point_base(0.01);
  NetElementBool slaveField8;
  NetElementEnum<TestEnum> slaveField9;
  NetElementEvent slaveField10;
  NetElementData<Vec2F> slaveField11;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.add_net_element(slaveField1));
  ASSERT_TRUE(slave.add_net_element(slaveField2));
  ASSERT_TRUE(slave.add_net_element(slaveField3));
  ASSERT_TRUE(slave.add_net_element(slaveField4));
  ASSERT_TRUE(slave.add_net_element(slaveField5));
  ASSERT_TRUE(slave.add_net_element(slaveField6));
  ASSERT_TRUE(slave.add_net_element(slaveField7));
  ASSERT_TRUE(slave.add_net_element(slaveField8));
  ASSERT_TRUE(slave.add_net_element(slaveField9));
  ASSERT_TRUE(slave.add_net_element(slaveField10));
  ASSERT_TRUE(slave.add_net_element(slaveField11));

  auto state = master.write_net_state();
  ASSERT_TRUE(state);
  ASSERT_TRUE(slave.read_net_state(state->first));

  EXPECT_EQ(slaveField1.get(), 567);
  EXPECT_EQ(slaveField2.get(), 17000u);
  EXPECT_EQ(slaveField3.get(), 22222u);
  EXPECT_FLOAT_EQ(slaveField4.get(), 1.55f);
  EXPECT_FLOAT_EQ(slaveField5.get(), 1.12345678910111213);
  EXPECT_FLOAT_EQ(slaveField6.get(), 2000.62f);
  EXPECT_FLOAT_EQ(slaveField7.get(), 2000.62);
  EXPECT_EQ(slaveField8.get(), true);
  EXPECT_EQ(slaveField9.get(), TestEnum::Value2);
  EXPECT_TRUE(slaveField10.pull_occurred());
  EXPECT_TRUE(slaveField11.get() == Vec2F(2.0f, 2.0f));
}

TEST(NetElements, NetElementDynamicGroup) {
  class TestElement : public NetElementGroup {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TestElement>, StarException> create(int value = 0) {
      auto element = std::shared_ptr<TestElement>(new TestElement());
      auto added = element->add_net_element(element->dataState);
      if (!added)
        return std::unexpected(std::move(added.error()));
      element->dataState.set(value);
      return element;
    }

    void setData(int value) {
      dataState.set(value);
    }

    int getData() const {
      return dataState.get();
    }

  private:
    TestElement() = default;
    NetElementInt dataState;
  };

  NetElementTop<NetElementDynamicGroup<TestElement>> masterGroup([] { return TestElement::create(); });

  NetElementTop<NetElementDynamicGroup<TestElement>> slaveGroup([] { return TestElement::create(); });
  std::uint64_t lastSlaveUpdateVersion = 0;
  auto sendSlaveUpdate = [&]() -> std::expected<void, StarException> {
    auto p = masterGroup.write_net_state(lastSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastSlaveUpdateVersion = p->second;
    auto read = slaveGroup.read_net_state(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return {};
  };

  auto element1 = TestElement::create(1000);
  ASSERT_TRUE(element1);
  auto objId1Result = masterGroup.add_net_element(std::move(*element1));
  ASSERT_TRUE(objId1Result);
  auto objId1 = *objId1Result;
  auto element2 = TestElement::create(2000);
  ASSERT_TRUE(element2);
  auto objId2Result = masterGroup.add_net_element(std::move(*element2));
  ASSERT_TRUE(objId2Result);
  auto objId2 = *objId2Result;

  EXPECT_EQ(masterGroup.net_element_ids().size(), 2u);
  {
    auto element = masterGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1000);
  }
  {
    auto element = masterGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2000);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.net_element_ids().size(), 2u);
  {
    auto element = slaveGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1000);
  }
  {
    auto element = slaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2000);
  }

  {
    auto element = masterGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(1001);
  }
  {
    auto element = masterGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2001);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  {
    auto element = slaveGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1001);
  }
  {
    auto element = slaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2001);
  }

  {
    auto element = masterGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(1002);
  }
  {
    auto element = masterGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2002);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  {
    auto element = masterGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = masterGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = slaveGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = slaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }

  auto element3 = TestElement::create(3001);
  ASSERT_TRUE(element3);
  auto objId3Result = masterGroup.add_net_element(std::move(*element3));
  ASSERT_TRUE(objId3Result);
  auto objId3 = *objId3Result;

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.net_element_ids().size(), 3u);
  {
    auto element = slaveGroup.get_net_element(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3001);
  }

  // Delta must be greater than MaxChangeDataSteps
  auto element4 = TestElement::create(4001);
  ASSERT_TRUE(element4);
  auto objId4Result = masterGroup.add_net_element(std::move(*element4));
  ASSERT_TRUE(objId4Result);
  auto objId4 = *objId4Result;
  {
    auto element = masterGroup.get_net_element(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(3002);
  }
  {
    auto element = masterGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(4002);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.net_element_ids().size(), 4u);
  {
    auto element = slaveGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = slaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = slaveGroup.get_net_element(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3002);
  }
  {
    auto element = slaveGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4002);
  }

  NetElementTop<NetElementDynamicGroup<TestElement>> forwardedSlaveGroup([] { return TestElement::create(); });
  std::uint64_t lastForwardedSlaveUpdateVersion = 0;
  auto sendForwardedSlaveUpdate = [&]() -> std::expected<void, StarException> {
    auto p = slaveGroup.write_net_state(lastForwardedSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastForwardedSlaveUpdateVersion = p->second;
    auto read = forwardedSlaveGroup.read_net_state(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return {};
  };

  ASSERT_TRUE(sendForwardedSlaveUpdate());

  EXPECT_EQ(forwardedSlaveGroup.net_element_ids().size(), 4u);
  {
    auto element = forwardedSlaveGroup.get_net_element(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = forwardedSlaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = forwardedSlaveGroup.get_net_element(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3002);
  }
  {
    auto element = forwardedSlaveGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4002);
  }

  masterGroup.remove_net_element(objId1);
  masterGroup.remove_net_element(objId3);
  {
    auto element = masterGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2003);
  }
  {
    auto element = masterGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(4003);
  }

  ASSERT_TRUE(sendSlaveUpdate());
  ASSERT_TRUE(sendForwardedSlaveUpdate());

  auto element5 = TestElement::create(5001);
  ASSERT_TRUE(element5);
  auto obj5Result = masterGroup.add_net_element(std::move(*element5));
  ASSERT_TRUE(obj5Result);
  auto obj5 = *obj5Result;
  masterGroup.remove_net_element(obj5);

  ASSERT_TRUE(sendSlaveUpdate());
  ASSERT_TRUE(sendForwardedSlaveUpdate());

  EXPECT_EQ(slaveGroup.net_element_ids().size(), 2u);
  {
    auto element = slaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2003);
  }
  {
    auto element = slaveGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4003);
  }

  EXPECT_EQ(forwardedSlaveGroup.net_element_ids().size(), 2u);
  {
    auto element = forwardedSlaveGroup.get_net_element(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2003);
  }
  {
    auto element = forwardedSlaveGroup.get_net_element(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4003);
  }
}

TEST(NetElements, DynamicGroupForwardsMaximumIdWithoutTerminatingDelta) {
  NetElementVersion version;
  NetElementDynamicGroup<NetElementUInt> relay, receiver;
  relay.init_net_version(std::cref(version));
  auto loaded_version = version.increment();
  constexpr auto max_id = std::numeric_limits<std::uint32_t>::max();
  DataStreamBuffer full;
  ASSERT_TRUE(full.write_vlq_u(1)); // count
  ASSERT_TRUE(full.write_vlq_u(max_id));
  ASSERT_TRUE(full.write_vlq_u(1)); // child payload length
  ASSERT_TRUE(full.write_vlq_u(41));
  ASSERT_TRUE(full.seek(0));
  ASSERT_TRUE(relay.net_load(full, {}));
  ASSERT_TRUE(full.seek(0));
  ASSERT_TRUE(receiver.net_load(full, {}));

  auto changed_version = version.increment();
  auto child = relay.get_net_element(max_id);
  ASSERT_TRUE(child);
  (*child)->set(42);
  DataStreamBuffer delta;
  ASSERT_EQ(relay.write_net_delta(delta, changed_version), true);
  ASSERT_TRUE(delta.seek(0));
  ASSERT_EQ(delta.read<bool>(), false);
  EXPECT_EQ(delta.read_vlq_u(), std::uint64_t{1} << 32);
  EXPECT_EQ(delta.read_vlq_u(), 42u);
  EXPECT_EQ(delta.read_vlq_u(), 0u);
  EXPECT_EQ(delta.at_end(), true);
  ASSERT_TRUE(delta.seek(0));
  ASSERT_TRUE(receiver.read_net_delta(delta));
  auto received = receiver.get_net_element(max_id);
  ASSERT_TRUE(received);
  EXPECT_EQ((*received)->get(), 42u);

  // A peer starting at the full load also gets Reset/Add. The addition must
  // contain the live value, not the original payload retained by the relay.
  DataStreamBuffer backlog;
  ASSERT_EQ(relay.write_net_delta(backlog, loaded_version), true);
  ASSERT_TRUE(backlog.seek(0));
  ASSERT_EQ(backlog.read<bool>(), false);
  ASSERT_EQ(backlog.read_vlq_u(), 1u);
  ASSERT_EQ(backlog.read<std::uint8_t>(), 0u);
  ASSERT_EQ(backlog.read_vlq_u(), 1u);
  ASSERT_EQ(backlog.read<std::uint8_t>(), 2u);
  ASSERT_EQ(backlog.read<std::uint32_t>(), max_id);
  ASSERT_EQ(backlog.read_vlq_u(), 1u);
  EXPECT_EQ(backlog.read_vlq_u(), 42u);
}

TEST(NetElements, DynamicGroupHistoryRetainsVersionsAndSkipsRemovedAdditions) {
  NetElementVersion version;
  NetElementDynamicGroup<NetElementGroup> group;
  group.init_net_version(std::cref(version));
  auto first = version.increment();
  // More than 100 records at one version. No longer-live additions must not
  // be serialized, but every removal remains in its original slot and order.
  for (std::uint32_t id = 1; id <= 101; ++id) {
    ASSERT_EQ(group.add_net_element(std::make_shared<NetElementGroup>()), id);
    group.remove_net_element(id);
  }
  for (int i = 0; i < 100; ++i)
    version.increment();
  group.remove_net_element(102);
  DataStreamBuffer retained;
  ASSERT_EQ(group.write_net_delta(retained, first), true);
  ASSERT_TRUE(retained.seek(0));
  ASSERT_EQ(retained.read<bool>(), false);
  for (std::uint32_t id = 1; id <= 102; ++id) {
    ASSERT_EQ(retained.read_vlq_u(), 1u);
    ASSERT_EQ(retained.read<std::uint8_t>(), 1u);
    ASSERT_EQ(retained.read<std::uint32_t>(), id);
  }
  EXPECT_EQ(retained.read_vlq_u(), 0u);
  EXPECT_EQ(retained.at_end(), true);

  version.increment();
  group.remove_net_element(103);
  DataStreamBuffer expired;
  ASSERT_EQ(group.write_net_delta(expired, first), true);
  EXPECT_TRUE(std::ranges::equal(expired.span(), std::array<char, 2>{1, 0}));
}

TEST(NetElements, DynamicGroupClearLogsReentrantChangesInRemovalOrder) {
  struct ReentrantElement : NetElementGroup {
    std::function<void()> on_destroy;
    ~ReentrantElement() override {
      if (on_destroy)
        on_destroy();
    }
  };
  NetElementTop<NetElementDynamicGroup<ReentrantElement>> group;
  auto first = std::make_shared<ReentrantElement>();
  ASSERT_EQ(group.add_net_element(first), 1u);
  ASSERT_EQ(group.add_net_element(std::make_shared<ReentrantElement>()), 2u);
  auto baseline = group.write_net_state();
  ASSERT_TRUE(baseline);
  first->on_destroy = [&group] {
    EXPECT_FALSE(group.get_net_element(1));
    EXPECT_EQ(group.add_net_element(std::make_shared<ReentrantElement>()), 3u);
  };
  first.reset();
  group.clear_net_elements();
  ASSERT_TRUE(group.get_net_element(3));
  auto delta = group.write_net_state(baseline->second);
  ASSERT_TRUE(delta);
  constexpr std::array<char, 22> expected{
      0, 0,                    // frame, inner delta
      1, 2, 0, 0, 0, 3, 0,     // destructor added id 3 with empty payload
      1, 1, 0, 0, 0, 1,        // completed removal of id 1
      1, 1, 0, 0, 0, 2,        // completed removal of id 2
      0};
  EXPECT_TRUE(std::ranges::equal(delta->first.span(), expected));
}

TEST(NetElements, DynamicGroupForwardsNestedFieldsWithPeerProtocol) {
  class VersionedField : public NetElementCustomData<std::uint32_t> {
  public:
    VersionedField() : NetElementCustomData(
        [](DataStream& stream, std::uint32_t& value) {
          return stream.stream_compatibility_version() == LegacyVersion ? stream.cread<std::uint8_t>(value) : stream.read(value);
        },
        [](DataStream& stream, std::uint32_t const& value) {
          return stream.stream_compatibility_version() == LegacyVersion ? stream.cwrite<std::uint8_t>(value) : stream.write(value);
        }) {}
  };
  static_assert(dynamic_net_element<VersionedField> && !dynamic_net_element<VersionedField const>);
  NetElementTop<NetElementDynamicGroup<VersionedField>> sender, relay, receiver;
  NetCompatibilityRules legacy(LegacyVersion);

  auto first = std::make_shared<VersionedField>();
  first->set(41);
  ASSERT_EQ(sender.add_net_element(std::move(first)), 1u);
  auto full = sender.write_net_state();
  ASSERT_TRUE(full);
  ASSERT_TRUE(relay.read_net_state(std::move(full->first)));
  auto legacy_full = relay.write_net_state(0, legacy);
  ASSERT_TRUE(legacy_full);
  // Full frame, count, id, payload length, one-byte legacy value.
  EXPECT_TRUE(std::ranges::equal(legacy_full->first.span(), std::array<char, 5>{1, 1, 1, 1, 41}));
  ASSERT_TRUE(receiver.read_net_state(std::move(legacy_full->first), 0.0f, legacy));
  auto received_first = receiver.get_net_element(1);
  ASSERT_TRUE(received_first);
  EXPECT_EQ((*received_first)->get(), 41u);

  auto second = std::make_shared<VersionedField>();
  second->set(42);
  ASSERT_EQ(sender.add_net_element(std::move(second)), 2u);
  auto delta = sender.write_net_state(full->second);
  ASSERT_TRUE(delta);
  ASSERT_TRUE(relay.read_net_state(std::move(delta->first)));
  auto legacy_delta = relay.write_net_state(legacy_full->second, legacy);
  ASSERT_TRUE(legacy_delta);
  // Frame/inner delta flags, change code, add tag, uint32 id, length, value, terminator.
  EXPECT_TRUE(std::ranges::equal(legacy_delta->first.span(), std::array<char, 11>{0, 0, 1, 2, 0, 0, 0, 2, 1, 42, 0}));
  ASSERT_TRUE(receiver.read_net_state(std::move(legacy_delta->first), 0.0f, legacy));
  auto received_second = receiver.get_net_element(2);
  ASSERT_TRUE(received_second);
  EXPECT_EQ((*received_second)->get(), 42u);
}

TEST(NetElements, DynamicGroupClearSendsOrderedRemovalsAndRetainsCursor) {
  using Group = NetElementTop<NetElementDynamicGroup<NetElementGroup>>;
  Group cleared;
  Group individually_removed;
  for (std::uint32_t id = 1; id <= 3; ++id) {
    for (Group& group : {std::ref(cleared), std::ref(individually_removed)}) {
      auto added = group.add_net_element(std::make_shared<NetElementGroup>());
      ASSERT_TRUE(added);
      ASSERT_EQ(*added, id);
    }
  }
  auto clear_baseline = cleared.write_net_state(0);
  auto individual_baseline = individually_removed.write_net_state(0);
  ASSERT_TRUE(clear_baseline);
  ASSERT_TRUE(individual_baseline);
  cleared.clear_net_elements();
  for (std::uint32_t id = 1; id <= 3; ++id)
    individually_removed.remove_net_element(id);
  EXPECT_TRUE(cleared.net_element_ids().empty());
  auto clear_delta = cleared.write_net_state(clear_baseline->second);
  auto individual_delta = individually_removed.write_net_state(individual_baseline->second);
  ASSERT_TRUE(clear_delta);
  ASSERT_TRUE(individual_delta);
  EXPECT_FALSE(clear_delta->first.empty());
  EXPECT_EQ(clear_delta->first, individual_delta->first);
  auto next = cleared.add_net_element(std::make_shared<NetElementGroup>());
  ASSERT_TRUE(next);
  EXPECT_EQ(*next, 4u);
}

TEST(NetElements, NetElementMap) {
  NetElementTop<NetElementMap<String, String>> masterMap;
  NetElementTop<NetElementMap<String, String>> slaveMap;

  std::uint64_t lastUpdateVersion = 0;

  auto sendUpdate = [&]() -> std::expected<bool, StarException> {
    auto p = masterMap.write_net_state(lastUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    auto read = slaveMap.read_net_state(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    lastUpdateVersion = p->second;
    return !p->first.empty();
  };

  ASSERT_TRUE(masterMap.add("foo", "bar"));
  ASSERT_TRUE(masterMap.add("baz", "bof"));

  EXPECT_EQ(masterMap.size(), 2u);

  ASSERT_TRUE(sendUpdate());

  EXPECT_EQ(slaveMap.size(), 2u);
  {
    auto value = slaveMap.get("foo");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bar");
  }
  {
    auto value = slaveMap.get("baz");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bof");
  }

  ASSERT_TRUE(masterMap.add("bif", "fob"));
  masterMap.remove("foo");

  ASSERT_TRUE(sendUpdate());

  EXPECT_EQ(slaveMap.size(), 2u);
  {
    auto value = slaveMap.get("bif");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fob");
  }

  masterMap.clear();
  masterMap.set("fib", "fab");

  ASSERT_TRUE(sendUpdate());

  EXPECT_EQ(slaveMap.size(), 1u);
  {
    auto value = slaveMap.get("fib");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fab");
  }

  masterMap.set("fib", "fab");

  EXPECT_EQ(sendUpdate(), false);

  masterMap.reset({{"a", "b"}, {"c", "d"}, {"e", "f"}});

  ASSERT_TRUE(sendUpdate());

  EXPECT_EQ(slaveMap.size(), 3u);
  {
    auto value = slaveMap.get("a");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "b");
  }
  {
    auto value = slaveMap.get("c");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "d");
  }
  {
    auto value = slaveMap.get("e");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "f");
  }
}

TEST(NetElements, MapRemoveOwnLongKey) {
  NetElementVersion version;
  NetElementHashMap<String, String> source, replica;
  source.init_net_version(std::cref(version));
  String const key(std::string(256, 'k'));
  source.set(key, "value");
  replica.set(key, "value");
  auto since = version.increment();
  ASSERT_TRUE(source.remove(source.begin()->first));
  ASSERT_TRUE(source.empty());
  DataStreamBuffer delta;
  auto written = source.write_net_delta(delta, since);
  ASSERT_TRUE(written);
  ASSERT_TRUE(*written);
  ASSERT_TRUE(delta.seek(0));
  ASSERT_TRUE(replica.read_net_delta(delta));
  EXPECT_TRUE(replica.empty());
}

TEST(NetElements, MapFullStorePreservesPendingWireOrder) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(7, 0x17);
  map.set(3, 0x13);
  ASSERT_TRUE(map.enable_net_interpolation());

  // Pending set, remove, clear, set must follow the live entries unchanged.
  std::array<char, 14> const delta{2, 0, 3, 0x33, 2, 1, 7, 2, 2, 2, 0, 9, 0x19, 0};
  DataStreamBuffer pending;
  ASSERT_TRUE(pending.write_data(delta));
  ASSERT_TRUE(pending.seek(0));
  ASSERT_TRUE(map.read_net_delta(pending, 1.0f));
  EXPECT_EQ(map.size(), 2u);

  DataStreamBuffer stored;
  ASSERT_TRUE(map.net_store(stored));
  std::array<char, 16> const expected{6, 0, 3, 0x13, 0, 7, 0x17, 0, 3, 0x33, 1, 7, 2, 0, 9, 0x19};
  ASSERT_EQ(stored.size(), expected.size());
  EXPECT_TRUE(std::equal(stored.span().begin(), stored.span().end(), expected.begin()));

  NetElementMap<std::uint8_t, std::uint8_t> restored;
  ASSERT_TRUE(stored.seek(0));
  ASSERT_TRUE(restored.net_load(stored, {}));
  ASSERT_EQ(restored.size(), 1u);
  auto value = restored.get(9);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 0x19);
  ASSERT_TRUE(map.tick_net_interpolation(1.0f));
  EXPECT_EQ(map.base_map(), restored.base_map());
}

TEST(NetElements, MapDeltaChangeTagsRoundTrip) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(3, 0x13);
  map.set(7, 0x17);
  EXPECT_TRUE(map.remove(3));
  map.clear();
  map.set(9, 0x19);
  DataStreamBuffer stream;
  auto written = map.write_net_delta(stream, 0);
  ASSERT_TRUE(written);
  ASSERT_TRUE(*written);
  std::array<char, 18> const expected{2, 0, 3, 0x13, 2, 0, 7, 0x17, 2, 1, 3, 2, 2, 2, 0, 9, 0x19, 0};
  ASSERT_EQ(stream.size(), expected.size());
  EXPECT_TRUE(std::equal(stream.span().begin(), stream.span().end(), expected.begin()));

  NetElementMap<std::uint8_t, std::uint8_t> restored;
  ASSERT_TRUE(stream.seek(0));
  ASSERT_TRUE(restored.read_net_delta(stream, 0.0f));
  EXPECT_EQ(restored.base_map(), map.base_map());
}

TEST(NetElements, MapFullStoreWriteFailuresRetainCommittedPrefix) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(3, 0x13);
  map.set(7, 0x17);
  std::array<char, 7> const expected{2, 0, 3, 0x13, 0, 7, 0x17};
  // Includes count and each set's tag/key/value, including after a complete set.
  for (std::size_t limit = 0; limit < expected.size(); ++limit) {
    std::vector<char> prefix;
    bool rejected = false;
    bool wroteAfterError = false;
    DataStreamFunctions stream({}, [&](std::span<char const> bytes) -> std::expected<std::size_t, StarException> {
      wroteAfterError |= rejected;
      if (bytes.size() > limit - prefix.size()) {
        rejected = true;
        return std::unexpected(OutOfRangeException("map write boundary", false));
      }
      prefix.insert(prefix.end(), bytes.begin(), bytes.end());
      return bytes.size();
    });
    auto result = map.net_store(stream);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<OutOfRangeException>());
    EXPECT_FALSE(wroteAfterError);
    ASSERT_EQ(prefix.size(), limit);
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), expected.begin()));
    EXPECT_EQ(map.size(), 2u);

    // A real reader commits the first set only once its value is complete.
    DataStreamBuffer input;
    ASSERT_TRUE(input.write_data(prefix));
    ASSERT_TRUE(input.seek(0));
    NetElementMap<std::uint8_t, std::uint8_t> restored;
    restored.set(99, 99);
    EXPECT_FALSE(restored.net_load(input, {}));
    EXPECT_EQ(restored.size(), limit >= 4 ? 1u : 0u);
    EXPECT_FALSE(restored.contains(99));
    if (limit >= 4) {
      auto value = restored.get(3);
      ASSERT_TRUE(value);
      EXPECT_EQ(value->get(), 0x13);
    }
  }
}

TEST(NetElements, MapDeltaWriteFailuresRetainCommittedPrefix) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(3, 0x13);
  map.set(7, 0x17);
  std::array<char, 9> const expected{2, 0, 3, 0x13, 2, 0, 7, 0x17, 0};
  for (std::size_t limit = 0; limit < expected.size(); ++limit) {
    std::vector<char> prefix;
    bool rejected = false;
    bool wroteAfterError = false;
    DataStreamFunctions stream({}, [&](std::span<char const> bytes) -> std::expected<std::size_t, StarException> {
      wroteAfterError |= rejected;
      if (bytes.size() > limit - prefix.size()) {
        rejected = true;
        return std::unexpected(OutOfRangeException("map delta write boundary", false));
      }
      prefix.insert(prefix.end(), bytes.begin(), bytes.end());
      return bytes.size();
    });
    auto result = map.write_net_delta(stream, 0);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<OutOfRangeException>());
    EXPECT_FALSE(wroteAfterError);
    ASSERT_EQ(prefix.size(), limit);
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), expected.begin()));

    DataStreamBuffer input;
    ASSERT_TRUE(input.write_data(prefix));
    ASSERT_TRUE(input.seek(0));
    NetElementMap<std::uint8_t, std::uint8_t> restored;
    EXPECT_FALSE(restored.read_net_delta(input, 0.0f));
    EXPECT_EQ(restored.size(), static_cast<std::size_t>(limit >= 4) + static_cast<std::size_t>(limit >= 8));
    if (limit >= 4) {
      auto value = restored.get(3);
      ASSERT_TRUE(value);
      EXPECT_EQ(value->get(), 0x13);
    }
    if (limit >= 8) {
      auto value = restored.get(7);
      ASSERT_TRUE(value);
      EXPECT_EQ(value->get(), 0x17);
    }
  }
}

TEST(NetElements, NetElementMapInterpolated) {
  typedef NetElementTop<NetElementMap<String, String>> TestMap;

  TestMap masterMap;

  TestMap forwarderMap;
  std::uint64_t lastForwarderUpdateVersion = 0;
  auto sendForwarderUpdate = [&]() -> std::expected<bool, StarException> {
    auto p = masterMap.write_net_state(lastForwarderUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastForwarderUpdateVersion = p->second;
    auto read = forwarderMap.read_net_state(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return !p->first.empty();
  };

  TestMap slaveMap;
  std::uint64_t lastSlaveUpdateVersion = 0;
  auto sendSlaveUpdate = [&](float interpolationTime) -> std::expected<bool, StarException> {
    auto p = forwarderMap.write_net_state(lastSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastSlaveUpdateVersion = p->second;
    auto read = slaveMap.read_net_state(p->first, interpolationTime);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return !p->first.empty();
  };

  ASSERT_TRUE(slaveMap.enable_net_interpolation());

  ASSERT_TRUE(masterMap.add("foo", "bar"));
  ASSERT_TRUE(masterMap.add("baz", "bof"));

  EXPECT_EQ(masterMap.size(), 2u);

  ASSERT_TRUE(sendForwarderUpdate());
  ASSERT_TRUE(sendSlaveUpdate(0.0f));

  EXPECT_EQ(slaveMap.size(), 2u);
  {
    auto value = slaveMap.get("foo");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bar");
  }
  {
    auto value = slaveMap.get("baz");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bof");
  }

  ASSERT_TRUE(masterMap.add("bif", "fob"));
  ASSERT_TRUE(masterMap.add("qux", "qux"));
  masterMap.remove("foo");

  ASSERT_TRUE(sendForwarderUpdate());
  ASSERT_TRUE(sendSlaveUpdate(1.0f));

  EXPECT_EQ(slaveMap.size(), 2u);
  {
    auto value = slaveMap.get("foo");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bar");
  }
  {
    auto value = slaveMap.get("baz");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bof");
  }

  ASSERT_TRUE(slaveMap.tick_net_interpolation(1.0f));

  EXPECT_EQ(slaveMap.size(), 3u);
  {
    auto value = slaveMap.get("baz");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "bof");
  }
  {
    auto value = slaveMap.get("bif");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fob");
  }
  {
    auto value = slaveMap.get("qux");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "qux");
  }

  masterMap.clear();
  masterMap.set("fib", "fab");

  ASSERT_TRUE(sendForwarderUpdate());
  ASSERT_TRUE(sendSlaveUpdate(1.0f));

  EXPECT_EQ(slaveMap.size(), 3u);

  ASSERT_TRUE(slaveMap.tick_net_interpolation(1.0f));

  EXPECT_EQ(forwarderMap.size(), 1u);
  EXPECT_EQ(slaveMap.size(), 1u);
  {
    auto value = slaveMap.get("fib");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fab");
  }

  masterMap.set("fob", "fub");

  ASSERT_TRUE(sendForwarderUpdate());
  ASSERT_TRUE(sendSlaveUpdate(1.0f));

  EXPECT_EQ(slaveMap.size(), 1u);
  ASSERT_TRUE(slaveMap.disable_net_interpolation());
  EXPECT_EQ(slaveMap.size(), 2u);
  {
    auto value = slaveMap.get("fib");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fab");
  }
  {
    auto value = slaveMap.get("fob");
    ASSERT_TRUE(value);
    EXPECT_EQ(value->get(), "fub");
  }
}

TEST(NetElements, ConsumedSignalRetainsPayloadForOtherCursors) {
  NetElementSignal<String> sender;
  sender.send("payload that cannot fit in an inline string");
  auto received = sender.receive();
  ASSERT_EQ(received.size(), 1u);
  received.front() = "changed locally";
  EXPECT_TRUE(sender.receive().empty());
  for (int i = 0; i != 2; ++i) {
    NetElementSignal<String> receiver;
    DataStreamBuffer wire;
    ASSERT_EQ(sender.write_net_delta(wire, 0), true);
    ASSERT_TRUE(wire.seek(0));
    ASSERT_TRUE(receiver.read_net_delta(wire));
    EXPECT_EQ(receiver.receive(), List<String>({"payload that cannot fit in an inline string"}));
  }
}

TEST(NetElements, NetElementSignal) {
  NetElementSignal<int> masterSignal1;
  NetElementSignal<int> masterSignal2;

  NetElementTopGroup masterGroup;
  ASSERT_TRUE(masterGroup.add_net_element(masterSignal1));
  ASSERT_TRUE(masterGroup.add_net_element(masterSignal2));

  NetElementSignal<int> slaveSignal1;
  NetElementSignal<int> slaveSignal2;

  NetElementTopGroup slaveGroup;
  ASSERT_TRUE(slaveGroup.add_net_element(slaveSignal1));
  ASSERT_TRUE(slaveGroup.add_net_element(slaveSignal2));

  // No signals are supposed to be sent for the initial write
  auto masterUpdate1 = masterGroup.write_net_state();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slaveGroup.read_net_state(masterUpdate1->first));

  masterSignal1.send(101);
  masterSignal2.send(201);

  EXPECT_EQ(masterSignal1.receive(), List<int>({101}));
  EXPECT_EQ(masterSignal2.receive(), List<int>({201}));

  EXPECT_EQ(masterSignal1.receive(), List<int>({}));
  EXPECT_EQ(masterSignal2.receive(), List<int>({}));

  auto masterUpdate2 = masterGroup.write_net_state(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slaveGroup.read_net_state(masterUpdate2->first));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({101}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({201}));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));

  masterSignal1.send(102);
  masterSignal2.send(202);

  ASSERT_TRUE(slaveGroup.enable_net_interpolation());

  auto masterUpdate3 = masterGroup.write_net_state(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slaveGroup.read_net_state(masterUpdate3->first, 1.0f));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));

  ASSERT_TRUE(slaveGroup.tick_net_interpolation(1.0f));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({102}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({202}));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));
}
