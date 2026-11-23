module;
import std;
import star.format;
import star.type_identity;
import star.set;
import star.json;
import star.vector;
import star.spline;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.drawable;
import star.non_rotated_drawables_item;
import star.tool_user_item;

namespace Star {
class Root;

class Item;
class ToolUserEntity;
class World;

class BeamItem : public virtual NonRotatedDrawablesItem, public virtual ToolUserItem {
public:
  void query_type(TypeQuery& query) const noexcept override {
    query_type_of<BeamItem, NonRotatedDrawablesItem, ToolUserItem>(*this, query);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<BeamItem>(); }

  enum class EndType { Invalid = -1, Object, Tile, TileGroup, Wire };

  explicit BeamItem(Root& root);
  ~BeamItem() override = default;

  [[nodiscard]]
  std::expected<void, StarException> init(ToolUserEntity& owner, ToolHand hand) override;
  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting,
                                            HashSet<MoveControlType> const& moves) override;

  // Called once after the owner's fixed-tick item control/fire work.
  [[nodiscard]]
  std::expected<void, StarException> updateBeamEffects(bool visible);

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> nonRotatedDrawables() const override;

  [[nodiscard]]
  virtual std::expected<float, StarException> getAngle(float angle);
  [[nodiscard]]
  virtual std::expected<List<Drawable>, StarException> drawables() const;
  virtual Vec2F handPosition() const;
  virtual Vec2F firePosition() const;
  virtual void setRange(float range);
  [[nodiscard]]
  virtual std::expected<float, StarException> getAppropriateOpacity() const;
  virtual void setEnd(EndType type);

protected:
  Root& beamRoot() const;
  [[nodiscard]]
  std::expected<void, StarException> initializeBeam(Json config);
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> beamDrawables() const;
  virtual bool beamActive() const;
  [[nodiscard]]
  virtual std::expected<bool, StarException> beamCanPlace() const;

  String m_image;
  StringList m_endImages;
  EndType m_endType;

  float m_segmentsPerUnit;
  float m_nearControlPointElasticity;
  float m_farControlPointElasticity;
  float m_nearControlPointDistance;
  Vec2F m_handPosition;
  Vec2F m_firePosition;
  float m_range;

  float m_targetSegmentRun;
  float m_minBeamWidth;
  float m_maxBeamWidth;
  float m_beamWidthDev;
  float m_minBeamJitter;
  float m_maxBeamJitter;
  float m_beamJitterDev;
  float m_minBeamTrans;
  float m_maxBeamTrans;
  float m_beamTransDev;
  int m_minBeamLines;
  int m_maxBeamLines;
  float m_innerBrightnessScale;
  float m_firstStripeThickness;
  float m_secondStripeThickness;
  Color m_color;

  bool m_inRangeLastUpdate = false;
  bool m_beamInRange = false;
  bool m_beamCanPlace = true;
  Color m_lastUpdateColor;

private:
  CSplineF m_beam_curve;
  SplineArcLengthTable<float, 100> m_beam_arc;
  float m_target_distance = 0;
  std::reference_wrapper<Root> m_beamRoot;
};

}

export module star.beam_item;

export namespace Star {
  using ::Star::Item;
  using ::Star::ToolUserEntity;
  using ::Star::World;
  using ::Star::BeamItem;
}
