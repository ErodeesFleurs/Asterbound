module;

import std;
import star.world_geometry;
import star.vector;
import star.game_types;
import star.interpolation;

import star.rect;
import star.math_common;
namespace Star {

class WorldCamera {
public:
  void setScreenSize(Vec2U screenSize);
  Vec2U screenSize() const;

  void setTargetPixelRatio(float targetPixelRatio);
  void setPixelRatio(float pixelRatio);
  float pixelRatio() const;

  void setWorldGeometry(WorldGeometry geometry);
  WorldGeometry worldGeometry() const;

  // Set the camera center position (in world space) to as close to the given
  // location as possible while keeping the screen within world bounds.
  void setCenterWorldPosition(Vec2F position, bool force = false);
  // Returns the actual camera position.
  Vec2F centerWorldPosition() const;

  // Transforms world coordinates into one set of screen coordinates.  Since
  // the world is non-euclidean, one world coordinate can transform to
  // potentially an infinite number of screen coordinates.  This will retrun
  // the closest to the center of the screen.
  Vec2F worldToScreen(Vec2F worldCoord) const;

  // Assumes top left corner of screen is (0, 0) in screen coordinates.
  Vec2F screenToWorld(Vec2F screen) const;

  // Returns screen dimensions in world space.
  RectF worldScreenRect() const;

  // Returns tile dimensions of the tiles that overlap with the screen
  RectI worldTileRect() const;

  // Returns the position of the lower left corner of the lower left tile of
  // worldTileRect, in screen coordinates.
  Vec2F tileMinScreen() const;

  void update(float dt);

  // Logic samples stay independent of the camera used to present and hit-test.
  void beginUpdate();
  void setRenderInterpolation(float interpolation);
  void resetPresentation();
  void restoreRenderTransform(WorldCamera const& displayed);

private:
  void updateRenderPosition(Vec2F position, bool force);

  WorldGeometry m_worldGeometry;
  Vec2U m_screenSize;
  float m_pixelRatio = 1.0f;
  float m_targetPixelRatio = 1.0f;
  Vec2F m_worldCenter;
  Vec2F m_rawWorldCenter;
  Vec2F m_previousWorldCenter;
  float m_previousPixelRatio = 1.0f;
  float m_renderPixelRatio = 1.0f;
  bool m_presentationValid = false;
};

inline void WorldCamera::setScreenSize(Vec2U screenSize) {
  m_screenSize = screenSize;
}

inline Vec2U WorldCamera::screenSize() const {
  return m_screenSize;
}

inline void WorldCamera::setTargetPixelRatio(float targetPixelRatio) {
  m_targetPixelRatio = targetPixelRatio;
}

inline void WorldCamera::setPixelRatio(float pixelRatio) {
  m_pixelRatio = m_targetPixelRatio = m_renderPixelRatio = m_previousPixelRatio = pixelRatio;
  updateRenderPosition(m_rawWorldCenter, true);
}

inline float WorldCamera::pixelRatio() const {
  return m_renderPixelRatio;
}

inline void WorldCamera::setWorldGeometry(WorldGeometry geometry) {
  if (m_worldGeometry != geometry)
    m_presentationValid = false;
  m_worldGeometry = std::move(geometry);
}

inline WorldGeometry WorldCamera::worldGeometry() const {
  return m_worldGeometry;
}

inline Vec2F WorldCamera::centerWorldPosition() const {
  return Vec2F(m_worldCenter);
}

inline Vec2F WorldCamera::worldToScreen(Vec2F worldCoord) const {
  Vec2F wrappedCoord = m_worldGeometry.nearestTo(Vec2F(m_worldCenter), worldCoord);
  return Vec2F((wrappedCoord[0] - m_worldCenter[0]) * (TilePixels * m_renderPixelRatio)
                 + static_cast<float>(m_screenSize[0]) / 2.0,
               (wrappedCoord[1] - m_worldCenter[1]) * (TilePixels * m_renderPixelRatio)
                 + static_cast<float>(m_screenSize[1]) / 2.0);
}

inline Vec2F WorldCamera::screenToWorld(Vec2F screen) const {
  return Vec2F(
    (screen[0] - static_cast<float>(m_screenSize[0]) / 2.0) / (TilePixels * m_renderPixelRatio) + m_worldCenter[0],
    (screen[1] - static_cast<float>(m_screenSize[1]) / 2.0) / (TilePixels * m_renderPixelRatio) + m_worldCenter[1]);
}

inline RectF WorldCamera::worldScreenRect() const {
  // screen dimensions in world space
  float w = static_cast<float>(m_screenSize[0]) / (TilePixels * m_renderPixelRatio);
  float h = static_cast<float>(m_screenSize[1]) / (TilePixels * m_renderPixelRatio);
  return RectF::withSize(Vec2F(m_worldCenter[0] - w / 2, m_worldCenter[1] - h / 2), Vec2F(w, h));
}

inline RectI WorldCamera::worldTileRect() const {
  RectF screen = worldScreenRect();
  Vec2I min = Vec2I::floor(screen.min());
  Vec2I size = Vec2I::ceil(Vec2F(m_screenSize) / (TilePixels * m_renderPixelRatio) + (screen.min() - Vec2F(min)));
  return RectI::withSize(min, size);
}

inline Vec2F WorldCamera::tileMinScreen() const {
  RectF screenRect = worldScreenRect();
  RectI tileRect = worldTileRect();
  return (Vec2F(tileRect.min()) - screenRect.min()) * (TilePixels * m_renderPixelRatio);
}

inline void WorldCamera::update(float dt) {
  float newPixelRatio = std::lerp(m_targetPixelRatio, m_pixelRatio, std::exp(-20.0f * dt));
  if (std::abs(newPixelRatio - m_targetPixelRatio) < 0.0125f)
    newPixelRatio = m_targetPixelRatio;
  if (m_pixelRatio != newPixelRatio) {
    m_pixelRatio = newPixelRatio;
    setCenterWorldPosition(m_rawWorldCenter, true);
  }
}

inline void WorldCamera::restoreRenderTransform(WorldCamera const& displayed) {
  m_worldCenter = displayed.m_worldCenter;
  m_renderPixelRatio = displayed.m_renderPixelRatio;
}

}

export module star.world_camera;

export namespace Star {
  using ::Star::WorldCamera;
}
