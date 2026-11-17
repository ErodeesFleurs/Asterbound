#version 450
layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexTextureCoordinate;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in int vertexData;
layout(location = 0) out vec2 fragmentTextureCoordinate;
layout(location = 1) flat out int fragmentTextureIndex;
layout(location = 2) out vec4 fragmentColor;
void main() {
  vec2 screenPosition = (vertexTransform * vec3(vertexPosition, 1.0)).xy;
  gl_Position = vec4(screenPosition / screenSize * 2.0 - 1.0, 0.0, 1.0);
  int index = vertexData & 3;
  vec2 size = index == 3 ? textureSize3 : index == 2 ? textureSize2 : index == 1 ? textureSize1 : textureSize0;
  fragmentTextureCoordinate = vertexTextureCoordinate / size;
  fragmentTextureIndex = index;
  fragmentColor = vertexColor;
}
