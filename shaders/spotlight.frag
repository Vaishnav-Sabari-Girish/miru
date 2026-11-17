precision mediump float;
uniform vec2 u_cursor;
uniform vec2 u_resolution;
uniform float u_radius;
uniform float u_softness;
uniform float u_dim;
void main() {
  float dist = distance(gl_FragCoord.xy, u_cursor);
  float soft = max(u_softness, 1.0);
  float inner = max(u_radius - soft, 0.0);
  float outer = u_radius + soft;
  float t = smoothstep(inner, outer, dist);
  float a = u_dim * t;
  gl_FragColor = vec4(0.0, 0.0, 0.0, a);
}
