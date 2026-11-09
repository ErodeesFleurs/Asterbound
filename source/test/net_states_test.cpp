
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

TEST(NetElements, DataRounding) {
  NetElementFloat masterField1;
  NetElementFloat masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));

  masterField1.setFixedPointBase(0.1f);
  masterField2.setFixedPointBase(0.5);

  masterField1.set(2.1999f);
  masterField2.set(100.04);

  // Check to make sure encoded data is actually sent with the expected
  // limitations to the client side.

  NetElementFloat slaveField1;
  NetElementFloat slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));

  slaveField1.setFixedPointBase(0.1f);
  slaveField2.setFixedPointBase(0.5);

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));

  EXPECT_LT(std::fabs(slaveField1.get() - 2.2f), 0.00001f);
  EXPECT_LT(std::fabs(slaveField2.get() - 100), 0.0000001);

  // Make sure that jittering a fixed point or limited value doesn't cause
  // extra deltas

  masterField1.set(2.155f);
  masterField1.set(2.24f);
  masterField2.set(99.96);
  masterField2.set(100.00);

  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_TRUE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate2->first));

  masterField1.set(10.0f);
  masterField2.set(50.0f);

  auto masterUpdate3 = master.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate3->first));

  EXPECT_LT(std::fabs(slaveField1.get() - 10.0f), 0.00001f);
  EXPECT_LT(std::fabs(slaveField2.get() - 50.0f), 0.0000001);
}

TEST(NetElements, DirectWriteRead) {
  NetElementUInt masterField1;
  NetElementUInt masterField2;
  NetElementUInt masterField3;
  NetElementUInt masterField4;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));
  ASSERT_TRUE(master.addNetElement(masterField4));

  masterField1.set(1);
  masterField2.set(2);
  masterField3.set(3);
  masterField4.set(4);

  NetElementUInt slaveField1;
  NetElementUInt slaveField2;
  NetElementUInt slaveField3;
  NetElementUInt slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));
  ASSERT_TRUE(slave.addNetElement(slaveField3));
  ASSERT_TRUE(slave.addNetElement(slaveField4));

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));

  EXPECT_EQ(slaveField1.get(), 1u);
  EXPECT_EQ(slaveField2.get(), 2u);
  EXPECT_EQ(slaveField3.get(), 3u);
  EXPECT_EQ(slaveField4.get(), 4u);

  masterField1.set(10);
  masterField3.set(30);

  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slave.readNetState(masterUpdate2->first));

  EXPECT_EQ(slaveField1.get(), 10u);
  EXPECT_EQ(slaveField2.get(), 2u);
  EXPECT_EQ(slaveField3.get(), 30u);
  EXPECT_EQ(slaveField4.get(), 4u);

  masterField2.set(20);
  masterField4.set(40);

  auto masterUpdate3 = master.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slave.readNetState(masterUpdate3->first));

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
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));
  ASSERT_TRUE(master.addNetElement(masterField4));

  NetElementInt slaveField1;
  NetElementUInt slaveField2;
  NetElementUInt slaveField3;
  NetElementSize slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));
  ASSERT_TRUE(slave.addNetElement(slaveField3));
  ASSERT_TRUE(slave.addNetElement(slaveField4));

  masterField1.set(10);
  masterField2.set(20);
  masterField3.set(30);
  masterField4.set(40);

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), 20u);
  EXPECT_EQ(masterField3.get(), 30u);
  EXPECT_EQ(masterField4.get(), 40u);

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);

  // Initial state should be 5 bytes, 1 byte for header, then 4 1 byte values.
  EXPECT_EQ(masterUpdate1->first.size(), 5u);

  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));
  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 20u);
  EXPECT_EQ(slaveField3.get(), 30u);
  EXPECT_EQ(slaveField4.get(), 40u);

  masterField1.set(50);
  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);

  // Second delta should be not include any other data than the single 1 byte
  // changed state, so make sure that it is 1 byte for header, 1 byte for field
  // number, 1 byte for state, 1 byte for end marker.
  EXPECT_EQ(masterUpdate2->first.size(), 4u);

  ASSERT_TRUE(slave.readNetState(masterUpdate2->first));
  EXPECT_EQ(slaveField1.get(), 50);
}

TEST(NetElements, Forwarding) {
  NetElementInt masterField1;
  NetElementData<String> masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));

  NetElementInt forwarderField1;
  NetElementData<String> forwarderField2;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.addNetElement(forwarderField1));
  ASSERT_TRUE(forwarder.addNetElement(forwarderField2));

  NetElementInt slaveField1;
  NetElementData<String> slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));

  masterField1.set(413);
  masterField2.set("foo");

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.readNetState(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.writeNetState();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.readNetState(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 413);
  EXPECT_EQ(forwarderField2.get(), "foo");
  EXPECT_EQ(slaveField1.get(), 413);
  EXPECT_EQ(slaveField2.get(), "foo");
}

TEST(NetElements, StepForwarding) {
  NetElementInt masterField1;
  NetElementInt masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));

  NetElementInt forwarderField1;
  NetElementInt forwarderField2;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.addNetElement(forwarderField1));
  ASSERT_TRUE(forwarder.addNetElement(forwarderField2));

  NetElementInt slaveField1;
  NetElementInt slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));

  // First emulate store / load that would happen in entity initialization.

  masterField1.set(10);
  masterField2.set(20);

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.readNetState(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.writeNetState();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.readNetState(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 20);

  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 20);

  // Then, update one field and transmit that delta[ to the forwarder.

  masterField1.set(413);

  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(forwarder.readNetState(masterUpdate2->first));
  EXPECT_EQ(forwarderField1.get(), 413);

  auto forwarderUpdate2 = forwarder.writeNetState(forwarderUpdate1->second);
  ASSERT_TRUE(forwarderUpdate2);
  ASSERT_TRUE(slave.readNetState(forwarderUpdate2->first));
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
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));

  NetElementInt forwarderField1;
  NetElementFloat forwarderField2;
  NetElementData<String> forwarderField3;

  NetElementTop<NetElementGroup> forwarder;
  ASSERT_TRUE(forwarder.addNetElement(forwarderField1));
  ASSERT_TRUE(forwarder.addNetElement(forwarderField2));
  ASSERT_TRUE(forwarder.addNetElement(forwarderField3));

  NetElementInt slaveField1;
  NetElementFloat slaveField2;
  NetElementData<String> slaveField3;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));
  ASSERT_TRUE(slave.addNetElement(slaveField3));

  ASSERT_TRUE(forwarder.enableNetInterpolation());
  ASSERT_TRUE(slave.enableNetInterpolation());

  masterField1.set(10);
  masterField2.set(10.0f);
  masterField3.set("10");

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(forwarder.readNetState(masterUpdate1->first));

  auto forwarderUpdate1 = forwarder.writeNetState();
  ASSERT_TRUE(forwarderUpdate1);
  ASSERT_TRUE(slave.readNetState(forwarderUpdate1->first));

  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 10.0f);
  EXPECT_EQ(forwarderField3.get(), "10");

  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 10.0f);
  EXPECT_EQ(slaveField3.get(), "10");

  masterField1.set(20);
  masterField2.set(20.0f);
  masterField3.set("20");

  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(forwarder.readNetState(masterUpdate2->first, 1.0f));

  // Forwarder should not be updated yet, still 1.0f interpolationTime behind
  EXPECT_EQ(forwarderField1.get(), 10);
  EXPECT_EQ(forwarderField2.get(), 10.0f);
  EXPECT_EQ(forwarderField3.get(), "10");

  // But the forwarder should STILL forward the absolute latest data to the
  // slave
  auto forwarderUpdate2 = forwarder.writeNetState(forwarderUpdate1->second);
  ASSERT_TRUE(forwarderUpdate2);
  ASSERT_TRUE(slave.readNetState(forwarderUpdate2->first, 1.0f));

  // Slave should not be updated yet, still 1.0f interpolationTime behind
  EXPECT_EQ(slaveField1.get(), 10);
  EXPECT_EQ(slaveField2.get(), 10.0f);
  EXPECT_EQ(slaveField3.get(), "10");

  // After ticking forward interpolation, both the forwarder and the slave
  // should both pick up the new values.

  ASSERT_TRUE(forwarder.tickNetInterpolation(1.0f));
  EXPECT_EQ(forwarderField1.get(), 20);
  EXPECT_EQ(forwarderField2.get(), 20.0f);
  EXPECT_EQ(forwarderField3.get(), "20");

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
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
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));

  EXPECT_EQ(masterField3.pullOccurrences(), 0u);

  masterField1.set(10);
  masterField2.set("foo");

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), "foo");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pullOccurrences(), 2u);
  EXPECT_EQ(masterField3.pullOccurrences(), 0u);
}

TEST(NetElements, IsolatedSetGet) {
  // Make sure fields work without being connected

  NetElementInt masterField1;
  NetElementData<String> masterField2;
  NetElementEvent masterField3;

  EXPECT_EQ(masterField2.pullUpdated(), false);
  EXPECT_EQ(masterField3.pullOccurrences(), 0u);

  masterField1.set(10);
  masterField2.set("foo");

  EXPECT_EQ(masterField2.pullUpdated(), true);
  EXPECT_EQ(masterField2.pullUpdated(), false);

  EXPECT_EQ(masterField1.get(), 10);
  EXPECT_EQ(masterField2.get(), "foo");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pullOccurrences(), 2u);
  EXPECT_EQ(masterField3.pullOccurrences(), 0u);

  masterField1.set(20);
  masterField2.set("bar");

  masterField3.trigger();
  masterField3.trigger();
  EXPECT_EQ(masterField3.pullOccurrences(), 2u);
}

TEST(NetElements, EventTest) {
  NetElementData<String> masterField1;
  NetElementEvent masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));

  NetElementData<String> slaveField1;
  NetElementEvent slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));

  // Events should always start with occurred false.
  EXPECT_EQ(slaveField1.pullUpdated(), false);
  EXPECT_EQ(slaveField2.pullOccurrences(), 0u);

  masterField1.set("foo");
  masterField2.trigger();
  EXPECT_TRUE(masterField1.pullUpdated());
  EXPECT_FALSE(masterField1.pullUpdated());

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));

  EXPECT_TRUE(slaveField1.pullUpdated());
  EXPECT_FALSE(slaveField1.pullUpdated());
  EXPECT_EQ(slaveField2.pullOccurrences(), 1u);
  EXPECT_EQ(slaveField2.pullOccurrences(), 0u);

  // Delta should be empty, nothing happened on step 1.
  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_TRUE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate2->first));

  EXPECT_EQ(slaveField2.pullOccurrences(), 0u);

  masterField2.trigger();

  auto masterUpdate3 = master.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate3->first));

  EXPECT_EQ(slaveField2.pullOccurrences(), 1u);
  EXPECT_EQ(slaveField2.pullOccurrences(), 0u);
}

TEST(NetElements, FieldUpdated) {
  NetElementData<String> masterField1;
  NetElementInt masterField2;
  NetElementEvent masterField3;
  NetElementEvent masterField4;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));
  ASSERT_TRUE(master.addNetElement(masterField4));

  NetElementData<String> slaveField1;
  NetElementInt slaveField2;
  NetElementEvent slaveField3;
  NetElementEvent slaveField4;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));
  ASSERT_TRUE(slave.addNetElement(slaveField3));
  ASSERT_TRUE(slave.addNetElement(slaveField4));

  slaveField4.setIgnoreOccurrencesOnNetLoad(true);

  masterField1.set("foo");

  masterField3.trigger();
  masterField4.trigger();

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));

  EXPECT_EQ(slaveField1.pullUpdated(), true);
  EXPECT_EQ(slaveField1.get(), "foo");
  EXPECT_EQ(slaveField1.pullUpdated(), false);

  EXPECT_EQ(masterField3.pullOccurrences(), 1u);
  EXPECT_EQ(slaveField3.pullOccurrences(), 1u);

  // Ignore occurrences on full load should stop slaveField4 from getting any
  // occurrences
  EXPECT_EQ(masterField4.pullOccurrences(), 1u);
  EXPECT_EQ(slaveField4.pullOccurrences(), 0u);

  masterField1.set("baz");

  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  EXPECT_FALSE(masterUpdate2->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate2->first));

  EXPECT_EQ(slaveField1.get(), "baz");
  EXPECT_EQ(slaveField1.pullUpdated(), true);

  masterField1.set("bar");

  auto masterUpdate3 = master.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  EXPECT_FALSE(masterUpdate3->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate3->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pullUpdated(), true);

  masterField1.push("bar");

  auto masterUpdate4 = master.writeNetState(masterUpdate3->second);
  ASSERT_TRUE(masterUpdate4);
  EXPECT_FALSE(masterUpdate4->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate4->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pullUpdated(), true);

  auto masterUpdate5 = master.writeNetState(masterUpdate4->second);
  ASSERT_TRUE(masterUpdate5);
  EXPECT_TRUE(masterUpdate5->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate5->first));

  EXPECT_EQ(slaveField1.get(), "bar");
  EXPECT_EQ(slaveField1.pullUpdated(), false);

  masterField3.trigger();
  masterField3.trigger();

  auto masterUpdate6 = master.writeNetState(masterUpdate5->second);
  ASSERT_TRUE(masterUpdate6);
  EXPECT_FALSE(masterUpdate6->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate6->first));

  slaveField3.ignoreOccurrences();
  // occurrence should not come through after "ignoreOccurrences"
  EXPECT_EQ(slaveField3.pullOccurrences(), 0u);
  EXPECT_EQ(masterField3.pullOccurrences(), 2u);

  masterField3.trigger();
  masterField3.trigger();
  masterField3.ignoreOccurrences();

  auto masterUpdate7 = master.writeNetState(masterUpdate6->second);
  ASSERT_TRUE(masterUpdate7);
  EXPECT_FALSE(masterUpdate7->first.empty());
  ASSERT_TRUE(slave.readNetState(masterUpdate7->first));

  // ignoreOccurrences is LOCAL only, so events should still go through to the
  // slave
  EXPECT_EQ(masterField3.pullOccurrences(), 0u);
  EXPECT_EQ(slaveField3.pullOccurrences(), 2u);
  EXPECT_EQ(slaveField3.pullOccurrences(), 0u);
}

TEST(NetElements, Interpolation) {
  NetElementFloat masterField1;
  NetElementData<String> masterField2;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));

  masterField1.set(1.0f);
  masterField2.set("yes");

  NetElementFloat slaveField1;
  NetElementData<String> slaveField2;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));

  slaveField1.setInterpolator(lerp<double, double>);

  ASSERT_TRUE(slave.enableNetInterpolation());

  auto masterUpdate1 = master.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slave.readNetState(masterUpdate1->first));

  masterField1.set(2.0f);
  masterField2.set("no");
  auto masterUpdate2 = master.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slave.readNetState(masterUpdate2->first, 2.0));

  masterField1.set(3.0f);
  masterField2.set("yes");
  auto masterUpdate3 = master.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slave.readNetState(masterUpdate3->first, 4.0));

  masterField1.set(4.0f);
  masterField2.set("no");
  auto masterUpdate4 = master.writeNetState(masterUpdate3->second);
  ASSERT_TRUE(masterUpdate4);
  ASSERT_TRUE(slave.readNetState(masterUpdate4->first, 6.0));

  EXPECT_TRUE(std::fabs(slaveField1.get() - 1.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pullUpdated(), true);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 1.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pullUpdated(), false);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 2.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "no");
  EXPECT_EQ(slaveField2.pullUpdated(), true);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 2.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "no");
  EXPECT_EQ(slaveField2.pullUpdated(), false);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 3.0) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pullUpdated(), true);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 3.5) < 0.001);
  EXPECT_EQ(slaveField2.get(), "yes");
  EXPECT_EQ(slaveField2.pullUpdated(), false);

  ASSERT_TRUE(slave.tickNetInterpolation(1.0f));
  EXPECT_TRUE(std::fabs(slaveField1.get() - 4.0) < 0.001);
  EXPECT_EQ(slaveField2.pullUpdated(), true);
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
  masterField6.setFixedPointBase(0.01f);
  NetElementDouble masterField7;
  masterField7.setFixedPointBase(0.01);
  NetElementBool masterField8;
  NetElementEnum<TestEnum> masterField9;
  NetElementEvent masterField10;
  NetElementData<Vec2F> masterField11;

  NetElementTop<NetElementGroup> master;
  ASSERT_TRUE(master.addNetElement(masterField1));
  ASSERT_TRUE(master.addNetElement(masterField2));
  ASSERT_TRUE(master.addNetElement(masterField3));
  ASSERT_TRUE(master.addNetElement(masterField4));
  ASSERT_TRUE(master.addNetElement(masterField5));
  ASSERT_TRUE(master.addNetElement(masterField6));
  ASSERT_TRUE(master.addNetElement(masterField7));
  ASSERT_TRUE(master.addNetElement(masterField8));
  ASSERT_TRUE(master.addNetElement(masterField9));
  ASSERT_TRUE(master.addNetElement(masterField10));
  ASSERT_TRUE(master.addNetElement(masterField11));

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
  EXPECT_TRUE(masterField10.pullOccurred());
  EXPECT_TRUE(masterField11.get() == Vec2F(2.0f, 2.0f));

  NetElementInt slaveField1;
  NetElementUInt slaveField2;
  NetElementSize slaveField3;
  NetElementFloat slaveField4;
  NetElementDouble slaveField5;
  NetElementFloat slaveField6;
  slaveField6.setFixedPointBase(0.01f);
  NetElementDouble slaveField7;
  slaveField7.setFixedPointBase(0.01);
  NetElementBool slaveField8;
  NetElementEnum<TestEnum> slaveField9;
  NetElementEvent slaveField10;
  NetElementData<Vec2F> slaveField11;

  NetElementTop<NetElementGroup> slave;
  ASSERT_TRUE(slave.addNetElement(slaveField1));
  ASSERT_TRUE(slave.addNetElement(slaveField2));
  ASSERT_TRUE(slave.addNetElement(slaveField3));
  ASSERT_TRUE(slave.addNetElement(slaveField4));
  ASSERT_TRUE(slave.addNetElement(slaveField5));
  ASSERT_TRUE(slave.addNetElement(slaveField6));
  ASSERT_TRUE(slave.addNetElement(slaveField7));
  ASSERT_TRUE(slave.addNetElement(slaveField8));
  ASSERT_TRUE(slave.addNetElement(slaveField9));
  ASSERT_TRUE(slave.addNetElement(slaveField10));
  ASSERT_TRUE(slave.addNetElement(slaveField11));

  auto state = master.writeNetState();
  ASSERT_TRUE(state);
  ASSERT_TRUE(slave.readNetState(state->first));

  EXPECT_EQ(slaveField1.get(), 567);
  EXPECT_EQ(slaveField2.get(), 17000u);
  EXPECT_EQ(slaveField3.get(), 22222u);
  EXPECT_FLOAT_EQ(slaveField4.get(), 1.55f);
  EXPECT_FLOAT_EQ(slaveField5.get(), 1.12345678910111213);
  EXPECT_FLOAT_EQ(slaveField6.get(), 2000.62f);
  EXPECT_FLOAT_EQ(slaveField7.get(), 2000.62);
  EXPECT_EQ(slaveField8.get(), true);
  EXPECT_EQ(slaveField9.get(), TestEnum::Value2);
  EXPECT_TRUE(slaveField10.pullOccurred());
  EXPECT_TRUE(slaveField11.get() == Vec2F(2.0f, 2.0f));
}

TEST(NetElements, NetElementDynamicGroup) {
  class TestElement : public NetElementGroup {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TestElement>, StarException> create(int value = 0) {
      auto element = std::shared_ptr<TestElement>(new TestElement());
      auto added = element->addNetElement(element->dataState);
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
    auto p = masterGroup.writeNetState(lastSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastSlaveUpdateVersion = p->second;
    auto read = slaveGroup.readNetState(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return {};
  };

  auto element1 = TestElement::create(1000);
  ASSERT_TRUE(element1);
  auto objId1Result = masterGroup.addNetElement(std::move(*element1));
  ASSERT_TRUE(objId1Result);
  auto objId1 = *objId1Result;
  auto element2 = TestElement::create(2000);
  ASSERT_TRUE(element2);
  auto objId2Result = masterGroup.addNetElement(std::move(*element2));
  ASSERT_TRUE(objId2Result);
  auto objId2 = *objId2Result;

  EXPECT_EQ(masterGroup.netElementIds().size(), 2u);
  {
    auto element = masterGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1000);
  }
  {
    auto element = masterGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2000);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.netElementIds().size(), 2u);
  {
    auto element = slaveGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1000);
  }
  {
    auto element = slaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2000);
  }

  {
    auto element = masterGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(1001);
  }
  {
    auto element = masterGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2001);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  {
    auto element = slaveGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1001);
  }
  {
    auto element = slaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2001);
  }

  {
    auto element = masterGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(1002);
  }
  {
    auto element = masterGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2002);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  {
    auto element = masterGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = masterGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = slaveGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = slaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }

  auto element3 = TestElement::create(3001);
  ASSERT_TRUE(element3);
  auto objId3Result = masterGroup.addNetElement(std::move(*element3));
  ASSERT_TRUE(objId3Result);
  auto objId3 = *objId3Result;

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.netElementIds().size(), 3u);
  {
    auto element = slaveGroup.getNetElement(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3001);
  }

  // Delta must be greater than MaxChangeDataSteps
  auto element4 = TestElement::create(4001);
  ASSERT_TRUE(element4);
  auto objId4Result = masterGroup.addNetElement(std::move(*element4));
  ASSERT_TRUE(objId4Result);
  auto objId4 = *objId4Result;
  {
    auto element = masterGroup.getNetElement(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(3002);
  }
  {
    auto element = masterGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(4002);
  }

  ASSERT_TRUE(sendSlaveUpdate());

  EXPECT_EQ(slaveGroup.netElementIds().size(), 4u);
  {
    auto element = slaveGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = slaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = slaveGroup.getNetElement(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3002);
  }
  {
    auto element = slaveGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4002);
  }

  NetElementTop<NetElementDynamicGroup<TestElement>> forwardedSlaveGroup([] { return TestElement::create(); });
  std::uint64_t lastForwardedSlaveUpdateVersion = 0;
  auto sendForwardedSlaveUpdate = [&]() -> std::expected<void, StarException> {
    auto p = slaveGroup.writeNetState(lastForwardedSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastForwardedSlaveUpdateVersion = p->second;
    auto read = forwardedSlaveGroup.readNetState(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return {};
  };

  ASSERT_TRUE(sendForwardedSlaveUpdate());

  EXPECT_EQ(forwardedSlaveGroup.netElementIds().size(), 4u);
  {
    auto element = forwardedSlaveGroup.getNetElement(objId1);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 1002);
  }
  {
    auto element = forwardedSlaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2002);
  }
  {
    auto element = forwardedSlaveGroup.getNetElement(objId3);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 3002);
  }
  {
    auto element = forwardedSlaveGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4002);
  }

  masterGroup.removeNetElement(objId1);
  masterGroup.removeNetElement(objId3);
  {
    auto element = masterGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(2003);
  }
  {
    auto element = masterGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    (*element)->setData(4003);
  }

  ASSERT_TRUE(sendSlaveUpdate());
  ASSERT_TRUE(sendForwardedSlaveUpdate());

  auto element5 = TestElement::create(5001);
  ASSERT_TRUE(element5);
  auto obj5Result = masterGroup.addNetElement(std::move(*element5));
  ASSERT_TRUE(obj5Result);
  auto obj5 = *obj5Result;
  masterGroup.removeNetElement(obj5);

  ASSERT_TRUE(sendSlaveUpdate());
  ASSERT_TRUE(sendForwardedSlaveUpdate());

  EXPECT_EQ(slaveGroup.netElementIds().size(), 2u);
  {
    auto element = slaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2003);
  }
  {
    auto element = slaveGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4003);
  }

  EXPECT_EQ(forwardedSlaveGroup.netElementIds().size(), 2u);
  {
    auto element = forwardedSlaveGroup.getNetElement(objId2);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 2003);
  }
  {
    auto element = forwardedSlaveGroup.getNetElement(objId4);
    ASSERT_TRUE(element);
    ASSERT_TRUE(*element);
    EXPECT_EQ((*element)->getData(), 4003);
  }
}

TEST(NetElements, NetElementMap) {
  NetElementTop<NetElementMap<String, String>> masterMap;
  NetElementTop<NetElementMap<String, String>> slaveMap;

  std::uint64_t lastUpdateVersion = 0;

  auto sendUpdate = [&]() -> std::expected<bool, StarException> {
    auto p = masterMap.writeNetState(lastUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    auto read = slaveMap.readNetState(p->first);
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

TEST(NetElements, MapFullStorePreservesPendingWireOrder) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(7, 0x17);
  map.set(3, 0x13);
  ASSERT_TRUE(map.enableNetInterpolation());

  // Pending set, remove, clear, set must follow the live entries unchanged.
  std::array<char, 14> const delta{2, 0, 3, 0x33, 2, 1, 7, 2, 2, 2, 0, 9, 0x19, 0};
  DataStreamBuffer pending;
  ASSERT_TRUE(pending.write_data(delta));
  ASSERT_TRUE(pending.seek(0));
  ASSERT_TRUE(map.readNetDelta(pending, 1.0f));
  EXPECT_EQ(map.size(), 2u);

  DataStreamBuffer stored;
  ASSERT_TRUE(map.netStore(stored));
  std::array<char, 16> const expected{6, 0, 3, 0x13, 0, 7, 0x17, 0, 3, 0x33, 1, 7, 2, 0, 9, 0x19};
  ASSERT_EQ(stored.size(), expected.size());
  EXPECT_TRUE(std::equal(stored.span().begin(), stored.span().end(), expected.begin()));

  NetElementMap<std::uint8_t, std::uint8_t> restored;
  ASSERT_TRUE(stored.seek(0));
  ASSERT_TRUE(restored.netLoad(stored, {}));
  ASSERT_EQ(restored.size(), 1u);
  auto value = restored.get(9);
  ASSERT_TRUE(value);
  EXPECT_EQ(value->get(), 0x19);
  ASSERT_TRUE(map.tickNetInterpolation(1.0f));
  EXPECT_EQ(map.baseMap(), restored.baseMap());
}

TEST(NetElements, MapDeltaChangeTagsRoundTrip) {
  NetElementMap<std::uint8_t, std::uint8_t> map;
  map.set(3, 0x13);
  map.set(7, 0x17);
  EXPECT_TRUE(map.remove(3));
  map.clear();
  map.set(9, 0x19);
  DataStreamBuffer stream;
  auto written = map.writeNetDelta(stream, 0);
  ASSERT_TRUE(written);
  ASSERT_TRUE(*written);
  std::array<char, 18> const expected{2, 0, 3, 0x13, 2, 0, 7, 0x17, 2, 1, 3, 2, 2, 2, 0, 9, 0x19, 0};
  ASSERT_EQ(stream.size(), expected.size());
  EXPECT_TRUE(std::equal(stream.span().begin(), stream.span().end(), expected.begin()));

  NetElementMap<std::uint8_t, std::uint8_t> restored;
  ASSERT_TRUE(stream.seek(0));
  ASSERT_TRUE(restored.readNetDelta(stream, 0.0f));
  EXPECT_EQ(restored.baseMap(), map.baseMap());
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
    auto result = map.netStore(stream);
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
    EXPECT_FALSE(restored.netLoad(input, {}));
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
    auto result = map.writeNetDelta(stream, 0);
    ASSERT_FALSE(result);
    EXPECT_TRUE(result.error().is<OutOfRangeException>());
    EXPECT_FALSE(wroteAfterError);
    ASSERT_EQ(prefix.size(), limit);
    EXPECT_TRUE(std::equal(prefix.begin(), prefix.end(), expected.begin()));

    DataStreamBuffer input;
    ASSERT_TRUE(input.write_data(prefix));
    ASSERT_TRUE(input.seek(0));
    NetElementMap<std::uint8_t, std::uint8_t> restored;
    EXPECT_FALSE(restored.readNetDelta(input, 0.0f));
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
    auto p = masterMap.writeNetState(lastForwarderUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastForwarderUpdateVersion = p->second;
    auto read = forwarderMap.readNetState(p->first);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return !p->first.empty();
  };

  TestMap slaveMap;
  std::uint64_t lastSlaveUpdateVersion = 0;
  auto sendSlaveUpdate = [&](float interpolationTime) -> std::expected<bool, StarException> {
    auto p = forwarderMap.writeNetState(lastSlaveUpdateVersion);
    if (!p)
      return std::unexpected(std::move(p.error()));
    lastSlaveUpdateVersion = p->second;
    auto read = slaveMap.readNetState(p->first, interpolationTime);
    if (!read)
      return std::unexpected(std::move(read.error()));
    return !p->first.empty();
  };

  ASSERT_TRUE(slaveMap.enableNetInterpolation());

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

  ASSERT_TRUE(slaveMap.tickNetInterpolation(1.0f));

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

  ASSERT_TRUE(slaveMap.tickNetInterpolation(1.0f));

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
  ASSERT_TRUE(slaveMap.disableNetInterpolation());
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

TEST(NetElements, NetElementSignal) {
  NetElementSignal<int> masterSignal1;
  NetElementSignal<int> masterSignal2;

  NetElementTopGroup masterGroup;
  ASSERT_TRUE(masterGroup.addNetElement(masterSignal1));
  ASSERT_TRUE(masterGroup.addNetElement(masterSignal2));

  NetElementSignal<int> slaveSignal1;
  NetElementSignal<int> slaveSignal2;

  NetElementTopGroup slaveGroup;
  ASSERT_TRUE(slaveGroup.addNetElement(slaveSignal1));
  ASSERT_TRUE(slaveGroup.addNetElement(slaveSignal2));

  // No signals are supposed to be sent for the initial write
  auto masterUpdate1 = masterGroup.writeNetState();
  ASSERT_TRUE(masterUpdate1);
  ASSERT_TRUE(slaveGroup.readNetState(masterUpdate1->first));

  masterSignal1.send(101);
  masterSignal2.send(201);

  EXPECT_EQ(masterSignal1.receive(), List<int>({101}));
  EXPECT_EQ(masterSignal2.receive(), List<int>({201}));

  EXPECT_EQ(masterSignal1.receive(), List<int>({}));
  EXPECT_EQ(masterSignal2.receive(), List<int>({}));

  auto masterUpdate2 = masterGroup.writeNetState(masterUpdate1->second);
  ASSERT_TRUE(masterUpdate2);
  ASSERT_TRUE(slaveGroup.readNetState(masterUpdate2->first));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({101}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({201}));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));

  masterSignal1.send(102);
  masterSignal2.send(202);

  ASSERT_TRUE(slaveGroup.enableNetInterpolation());

  auto masterUpdate3 = masterGroup.writeNetState(masterUpdate2->second);
  ASSERT_TRUE(masterUpdate3);
  ASSERT_TRUE(slaveGroup.readNetState(masterUpdate3->first, 1.0f));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));

  ASSERT_TRUE(slaveGroup.tickNetInterpolation(1.0f));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({102}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({202}));

  EXPECT_EQ(slaveSignal1.receive(), List<int>({}));
  EXPECT_EQ(slaveSignal2.receive(), List<int>({}));
}
