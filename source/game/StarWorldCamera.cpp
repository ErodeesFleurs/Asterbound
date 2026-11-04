import std;
import star.world_geometry;
import star.vector;
import star.game_types;

import star.world_camera;

import star.math_common;
namespace Star {

void WorldCamera::setCenterWorldPosition(Vec2F position, bool force) {
  m_rawWorldCenter = position;
  m_renderPixelRatio = m_pixelRatio;
  if (!m_presentationValid) {
    m_previousWorldCenter = position;
    m_previousPixelRatio = m_pixelRatio;
    m_presentationValid = true;
  }
  updateRenderPosition(position, force);
}

void WorldCamera::beginUpdate() {
  m_previousWorldCenter = m_rawWorldCenter;
  m_previousPixelRatio = m_pixelRatio;
  m_presentationValid = true;
  m_renderPixelRatio = m_pixelRatio;
  updateRenderPosition(m_rawWorldCenter, true);
}

void WorldCamera::setRenderInterpolation(float interpolation) {
  if (!m_presentationValid) {
    resetPresentation();
    return;
  }
  float alpha = std::clamp(interpolation, 0.0f, 1.0f);
  m_renderPixelRatio = std::lerp(m_previousPixelRatio, m_pixelRatio, alpha);
  auto position = m_previousWorldCenter + m_worldGeometry.diff(m_rawWorldCenter, m_previousWorldCenter) * alpha;
  updateRenderPosition(position, true);
}

void WorldCamera::resetPresentation() {
  m_previousWorldCenter = m_rawWorldCenter;
  m_previousPixelRatio = m_renderPixelRatio = m_pixelRatio;
  m_presentationValid = true;
  updateRenderPosition(m_rawWorldCenter, true);
}

void WorldCamera::updateRenderPosition(Vec2F position, bool force) {
  // Only actually move the world center if a half pixel distance has been
  // moved in any direction.  This is sort of arbitrary, but helps prevent
  // judder if the camera is at a boundary and floating point inaccuracy is
  // causing the focus to jitter back and forth across the boundary.
  if (std::fabs(position[0] - m_worldCenter[0]) < 1.0f / (TilePixels * m_renderPixelRatio * 2)
   && std::fabs(position[1] - m_worldCenter[1]) < 1.0f / (TilePixels * m_renderPixelRatio * 2) && !force)
    return;

  // First, make sure the camera center position is inside the main x
  // coordinate bounds, and that the top and bototm of the screen are not
  // outside of the y coordinate bounds.
  m_worldCenter = m_worldGeometry.xwrap(position);
  if (!m_worldGeometry.isNull())
    m_worldCenter[1] = clamp(m_worldCenter[1],
        (float)m_screenSize[1] / (TilePixels * m_renderPixelRatio * 2),
        m_worldGeometry.height() - (float)m_screenSize[1] / (TilePixels * m_renderPixelRatio * 2));

  // Then, position the camera center position so that the tile grid is as
  // close as possible aligned to whole pixel boundaries.  This is incredibly
  // important, because this means that even without any complicated rounding,
  // elements drawn in world space that are aligned with TilePixels will
  // eventually also be aligned to real screen pixels.

  float ratio = TilePixels * m_renderPixelRatio;

  if (m_screenSize[0] % 2 == 0)
    m_worldCenter[0] = std::round(m_worldCenter[0] * ratio) / ratio;
  else
    m_worldCenter[0] = (std::round(m_worldCenter[0] * ratio + 0.5f) - 0.5f) / ratio;

  if (m_screenSize[1] % 2 == 0)
    m_worldCenter[1] = std::round(m_worldCenter[1] * ratio) / ratio;
  else
    m_worldCenter[1] = (std::round(m_worldCenter[1] * ratio + 0.5f) - 0.5f) / ratio;
}

}
