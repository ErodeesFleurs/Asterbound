module;
import std;
import star.type_identity;
import star.json;
import star.id_map;
import star.game_types;
import star.bi_map;
import star.a_star;
import star.casting;
import star.data_stream;
import star.strong_typedef;
import star.list;
import star.rpc_promise;

import star.mobile_entity;
import star.actor_movement_controller;

namespace Star {

class StatusController;
class ActorMovementController;

// this is just used to have a base for what the game generally considers 'actors' as they all use the ActorMovementController, as well as have a StatusController
// theres potentially more things shared that could be moved here
class ActorEntity : public virtual MobileEntity {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<ActorEntity, MobileEntity>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ActorEntity>(); }

  // Successfully constructed actors always own both controllers.
  ActorMovementController& movementController() override = 0;
  virtual StatusController& statusController() = 0;
};
}

export module star.actor_entity;

export namespace Star {
  using ::Star::StatusController;
  using ::Star::ActorMovementController;
  using ::Star::ActorEntity;
}
