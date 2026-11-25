import std;
import star.data_stream;
import star.data_stream_devices;
import star.format;
import star.game_test;
import star.game_types;
import star.interaction_types;
import star.json;
import star.map;
import star.net_packets;
import star.string;
import star.string_view;
import star.unicode;
import star.uuid;
import star.vector;
#include "gtest/gtest.h"

using namespace Star;
using namespace std::string_view_literals;

namespace {

bool check(bool passed, std::string_view message, std::source_location at = std::source_location::current()) {
  if (!passed)
    testing::internal::AssertHelper(testing::TestPartResult::kNonFatalFailure, at.file_name(), static_cast<int>(at.line()), "")
        = testing::Message() << message;
  return passed;
}

bool same_request(InteractRequest const& a, InteractRequest const& b) {
  return a.source_id == b.source_id && a.source_position == b.source_position && a.target_id == b.target_id &&
      a.interact_position == b.interact_position;
}

bool same_action(InteractAction const& a, InteractAction const& b) {
  return a.type == b.type && a.entity_id == b.entity_id && a.data == b.data;
}

const GameTestRegistration names_and_inventory("InteractionTypes", "NamesAndInventory", [](GameTest&) {
  struct NameCase {
    std::string_view name;
    InteractActionType type;
    bool inventory;
  };
  using enum InteractActionType;
  constexpr auto cases = std::to_array<NameCase>({
      {"None", None, false}, {"OpenContainer", OpenContainer, false}, {"SitDown", SitDown, false},
      {"OpenCraftingInterface", OpenCraftingInterface, true}, {"OpenSongbookInterface", OpenSongbookInterface, true},
      {"OpenNpcCraftingInterface", OpenNpcCraftingInterface, true}, {"OpenMerchantInterface", OpenMerchantInterface, true},
      {"OpenAiInterface", OpenAiInterface, true}, {"OpenTeleportDialog", OpenTeleportDialog, true},
      {"ShowPopup", ShowPopup, true}, {"ScriptPane", ScriptPane, true}, {"Message", Message, false}});
  for (std::size_t i = 0; i < cases.size(); ++i) {
    auto const& [name, type, inventory] = cases[i];
    check(std::to_underlying(type) == static_cast<std::int32_t>(i), name);
    check(parse_interact_action_type(name) == type, name);
    auto lower = String(name).to_lower();
    check(lower && parse_interact_action_type(*lower) == type, name);
    check(can_open_from_inventory(type) == inventory, name);
  }
  check(!can_open_from_inventory(static_cast<InteractActionType>(123456)), "unknown inventory action");
  for (auto name : {""sv, "NotAnAction"sv, "SitDown\0"sv, "座席"sv}) {
    auto unknown = parse_interact_action_type(name);
    check(!unknown && unknown.error().is<MapException>(), "unknown but well-formed name");
  }
  auto invalid = parse_interact_action_type("座席\xff"sv);
  check(!invalid && invalid.error().is<UnicodeException>(), "invalid UTF-8 after a nonmatching prefix");
});

const GameTestRegistration script_results("InteractionTypes", "ScriptResults", [](GameTest&) {
  constexpr EntityId entity_id = -47;
  auto text = interact_action_from_script("sHoWpOpUp", entity_id);
  check(text && same_action(*text, {InteractActionType::ShowPopup, entity_id, {}}), "string result");
  Json payload = JsonObject{{"nested", JsonArray{1, 2, 3}}};
  for (Json const& data : {Json{}, payload}) {
    auto result = interact_action_from_script(JsonArray{"ScriptPane", data, false}, entity_id);
    check(result && same_action(*result, {InteractActionType::ScriptPane, entity_id, data}), "array payload and ignored trailing element");
  }
  for (Json const& malformed : {Json(false), Json(JsonObject{}), Json(JsonArray{}), Json(JsonArray{2})}) {
    auto result = interact_action_from_script(malformed, entity_id);
    auto first = malformed.get_string(0);
    check(!result && !first && result.error().is<JsonException>() && result.error().what() == first.error().what(),
        "malformed name is diagnosed before reading data");
  }
  for (auto name : {"Message"sv, "NotAnAction"sv, "\xff"sv}) {
    Json short_array = JsonArray{name};
    auto result = interact_action_from_script(short_array, entity_id);
    auto missing = short_array.get(1);
    check(!result && !missing && result.error().is<JsonException>() && result.error().what() == missing.error().what(),
        "missing data is diagnosed before validating name");
  }
  auto unknown = interact_action_from_script(JsonArray{"NotAnAction", 1}, entity_id);
  check(!unknown && unknown.error().is<MapException>(), "unknown array action");
  auto invalid = interact_action_from_script(JsonArray{"\xff"sv, 1}, entity_id);
  check(!invalid && invalid.error().is<UnicodeException>(), "invalid UTF-8 array action");
});

const GameTestRegistration request_wire("InteractionTypes", "RequestWire", [](GameTest&) {
  // Two signed IDs interleaved with two binary32 vectors, followed by an unpacked UUID.
  constexpr auto fixture = "\x01\x02\x03\x04\x3f\x80\x00\x00\xc0\x00\x00\x00"
                           "\xff\xff\xff\xfd\x40\x60\x00\x00\x3e\x80\x00\x00"
                           "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"sv;
  static_assert(fixture.size() == 40);
  std::span<char const> bytes(fixture);
  auto request_bytes = bytes.first(24);
  auto uuid = Uuid::from_bytes(bytes.last(16));
  if (!check(uuid.has_value(), "request UUID fixture")) return;
  InteractRequest const sent{0x01020304, {1, -2}, -3, {3.5f, 0.25f}};
  EntityInteractPacket packet(sent, *uuid);
  DataStreamBuffer encoded_request, encoded_packet;
  check(encoded_request.write(sent) && std::ranges::equal(encoded_request.span(), request_bytes), "request bytes");
  check(packet.write(encoded_packet) && std::ranges::equal(encoded_packet.span(), bytes), "request packet bytes");

  EntityInteractPacket target;
  check(same_request(target.interact_request, {null_entity_id, {}, null_entity_id, {}}) && target.request_id == Uuid::nil(),
      "default packet and request");
  InteractRequest const sentinel{5, {6, 7}, 8, {9, 10}};
  InteractRequest value = sentinel;
  target.interact_request = sentinel;
  for (std::size_t n = 0; n < bytes.size(); ++n) {
    if (n < request_bytes.size()) {
      DataStreamExternalBuffer stream(bytes.first(n));
      check(!stream.read(value) && same_request(value, sentinel), "truncated request modified destination");
    }
    DataStreamExternalBuffer stream(bytes.first(n));
    check(!target.read(stream) && same_request(target.interact_request, sentinel) && target.request_id == Uuid::nil(),
        "truncated request packet modified destination");
  }
  DataStreamExternalBuffer input(request_bytes), packet_input(bytes);
  check(input.read(value) && input.remaining() == 0u && same_request(value, sent), "complete request");
  check(target.read(packet_input) && packet_input.remaining() == 0u && same_request(target.interact_request, sent) &&
      target.request_id == *uuid, "complete request packet");
});

const GameTestRegistration result_wire("InteractionTypes", "ResultWire", [](GameTest&) {
  // The signed 32-bit action and target precede tagged JSON; UUID precedes source ID.
  constexpr auto fixture = "\x00\x00\x00\x0a\xff\xff\xff\xfa\x05\x03" "abc"
                           "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
                           "\x12\x34\x56\x78"sv;
  static_assert(fixture.size() == 33);
  std::span<char const> bytes(fixture);
  auto action_bytes = bytes.first(13);
  auto uuid = Uuid::from_bytes(bytes.subspan(13, 16));
  if (!check(uuid.has_value(), "result UUID fixture")) return;
  InteractAction const sent{InteractActionType::ScriptPane, -6, "abc"};
  EntityInteractResultPacket packet(sent, *uuid, 0x12345678);
  DataStreamBuffer encoded_action, encoded_packet;
  check(encoded_action.write(sent) && std::ranges::equal(encoded_action.span(), action_bytes), "action bytes");
  check(packet.write(encoded_packet) && std::ranges::equal(encoded_packet.span(), bytes), "result packet bytes");

  EntityInteractResultPacket target;
  check(!target.action && target.action.entity_id == null_entity_id && target.action.data.is_null() &&
      target.request_id == Uuid::nil() && target.source_entity_id == null_entity_id, "default result packet and action");
  InteractAction const sentinel{InteractActionType::Message, 9, true};
  InteractAction value = sentinel;
  target.action = sentinel;
  target.source_entity_id = -9;
  for (std::size_t n = 0; n < bytes.size(); ++n) {
    if (n < action_bytes.size()) {
      DataStreamExternalBuffer stream(bytes.first(n));
      check(!stream.read(value) && same_action(value, sentinel), "truncated action modified destination");
    }
    DataStreamExternalBuffer stream(bytes.first(n));
    check(!target.read(stream) && same_action(target.action, sentinel) && target.request_id == Uuid::nil() &&
        target.source_entity_id == -9, "truncated result packet modified destination");
  }
  DataStreamExternalBuffer input(action_bytes), packet_input(bytes);
  check(input.read(value) && input.remaining() == 0u && same_action(value, sent), "complete action");
  check(target.read(packet_input) && packet_input.remaining() == 0u && same_action(target.action, sent) &&
      target.request_id == *uuid && target.source_entity_id == 0x12345678, "complete result packet");

  constexpr auto unknown = "\x81\x23\x45\x67\x00\x00\x00\x01\x01"sv;
  DataStreamExternalBuffer unknown_input{std::span<char const>(unknown)};
  DataStreamBuffer forwarded;
  check(unknown_input.read(value) && static_cast<std::uint32_t>(value.type) == 0x81234567u && value &&
      value.entity_id == 1 && value.data.is_null() && forwarded.write(value) && std::ranges::equal(forwarded.span(), unknown),
      "unknown wire action survives decode and forwarding");
});

}
