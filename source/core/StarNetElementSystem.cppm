module;

import star.net_element_top;
import star.net_element_sync_group;

namespace Star {

// Makes a good default top-level NetElement group.
using NetElementTopGroup = NetElementTop<NetElementCallbackGroup>;
}

export module star.net_element_system;

export namespace Star {
  using ::Star::NetElementTopGroup;
}
