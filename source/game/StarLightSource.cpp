import star.format;
import star.vector;
import std;
import star.data_stream;
import star.bi_map;

import star.light_source;
import star.data_stream_extra;

namespace Star {

void LightSource::translate(Vec2F const& pos) {
  position += pos;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, LightSource const& lightSource) {
  if (auto r = ds.write(lightSource.position); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(lightSource.color); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(lightSource.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(lightSource.pointBeam); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(lightSource.beamAngle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.write(lightSource.beamAmbience); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, LightSource& lightSource) {
  if (auto r = ds.read(lightSource.position); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(lightSource.color); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(lightSource.type); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(lightSource.pointBeam); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(lightSource.beamAngle); !r) return std::unexpected(std::move(r.error()));
  if (auto r = ds.read(lightSource.beamAmbience); !r) return std::unexpected(std::move(r.error()));
  return std::ref(ds);
}

}
