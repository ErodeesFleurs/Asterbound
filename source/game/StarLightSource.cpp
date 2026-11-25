import star.format;
import std;
import star.data_stream;
import star.bi_map;
import star.json;

import star.light_source;
import star.data_stream_extra;

namespace Star {

std::expected<LightType, StarException> parse_light_type(Json const& config) {
  auto type = config.opt_string("lightType");
  if (!type)
    return std::unexpected(std::move(type.error()));
  if (*type)
    return enum_value<light_type_names>(**type);

  auto point_light = config.get_bool("pointLight", false);
  if (!point_light)
    return std::unexpected(std::move(point_light.error()));
  return *point_light ? LightType::Point : LightType::Spread;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, LightSource const& light_source) {
  return stream << light_source.position << light_source.color << light_source.type
      << light_source.point_beam << light_source.beam_angle << light_source.beam_ambience;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, LightSource& light_source) {
  LightSource decoded;
  auto result = stream >> decoded.position >> decoded.color >> decoded.type
      >> decoded.point_beam >> decoded.beam_angle >> decoded.beam_ambience;
  if (result)
    light_source = decoded;
  return result;
}

}
