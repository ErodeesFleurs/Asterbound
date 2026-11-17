#version 450
layout(location = 0) in vec2 fragmentTextureCoordinate;
layout(location = 1) flat in int fragmentTextureIndex;
layout(location = 2) in vec4 fragmentColor;
layout(location = 0) out vec4 outColor;
void main() {
  vec4 texColor;
  if (fragmentTextureIndex == 3) texColor = texture(texture3, fragmentTextureCoordinate);
  else if (fragmentTextureIndex == 2) texColor = texture(texture2, fragmentTextureCoordinate);
  else if (fragmentTextureIndex == 1) texColor = texture(texture1, fragmentTextureCoordinate);
  else texColor = texture(texture0, fragmentTextureCoordinate);
  if (texColor.a <= 0.0) discard;
  outColor = texColor * fragmentColor;
}
