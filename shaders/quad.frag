precision mediump float;                                                  
varying vec2 v_texcoord;                                                  
uniform sampler2D u_texture;                                              
uniform vec2 u_crop_origin;                                               
uniform vec2 u_crop_scale;                                                
uniform float u_y_invert;                                                 
uniform vec2 u_cursor_px;                                                 
uniform vec2 u_resolution;                                                
uniform float u_spotlight_enabled;                                        
uniform float u_spotlight_radius;                                         
uniform float u_spotlight_softness;                                       
uniform float u_spotlight_dim;                                            
uniform vec2 u_texel_size;                                                
uniform float u_upscale;                                                  
uniform float u_ellipse_mask;                                             
uniform float u_filter;                                                   
                                                                          
vec4 cubic(float v) {                                                     
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;                                
    vec4 s = n * n * n;                                                   
    float x = s.x;                                                        
    float y = s.y - 4.0 * s.x;                                            
    float z = s.z - 4.0 * s.y + 6.0 * s.x;                                
    float w = 6.0 - x - y - z;                                            
    return vec4(x, y, z, w) * (1.0 / 6.0);                                
}                                                                         
                                                                          
vec3 textureBicubic(sampler2D tex, vec2 uv) {                             
    vec2 texSize = 1.0 / u_texel_size;                                    
    vec2 invTexSize = u_texel_size;                                       
    uv = uv * texSize - 0.5;                                              
    vec2 fxy = fract(uv);                                                 
    uv -= fxy;                                                            
    vec4 xcubic = cubic(fxy.x);                                           
    vec4 ycubic = cubic(fxy.y);                                           
    vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;                              
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);          
    vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;                     
    offset *= invTexSize.xxyy;                                            
    vec4 sample0 = texture2D(tex, offset.xz);                             
    vec4 sample1 = texture2D(tex, offset.yz);                             
    vec4 sample2 = texture2D(tex, offset.xw);                             
    vec4 sample3 = texture2D(tex, offset.yw);                             
    float sx = s.x / (s.x + s.y);                                         
    float sy = s.z / (s.z + s.w);                                         
    return mix(mix(sample3.rgb, sample2.rgb, sx),                         
               mix(sample1.rgb, sample0.rgb, sx), sy);                    
}                                                                         
                                                                          
void main() {                                                             
    vec2 uv = v_texcoord;                                                 
    if (u_ellipse_mask > 0.5) {                                           
        vec2 p = (v_texcoord - vec2(0.5)) * 2.0;                          
        if (dot(p, p) > 1.0) discard;                                     
    }                                                                     
    if (u_y_invert > 0.5) { uv.y = 1.0 - uv.y; }                          
    vec2 sample_uv = u_crop_origin + uv * u_crop_scale;                   
    vec3 color;                                                           
    if (u_upscale > 1.5) {                                                
        color = textureBicubic(u_texture, sample_uv);                     
    } else {                                                              
        color = texture2D(u_texture, sample_uv).rgb;                      
    }                                                                     
    if (u_filter > 2.5) {                                                 
        color = clamp((color - 0.5) * 1.8 + 0.5, 0.0, 1.0);               
    } else if (u_filter > 1.5) {                                          
        color = 1.0 - color;                                              
    } else if (u_filter > 0.5) {                                          
        float g = dot(color, vec3(0.299, 0.587, 0.114));                  
        color = vec3(g);                                                  
    }                                                                     
    if (u_spotlight_enabled > 0.5) {                                      
        vec2 frag_px = gl_FragCoord.xy;                                   
        float dist = distance(frag_px, u_cursor_px);                      
        float inner = max(u_spotlight_radius - u_spotlight_softness, 0.0);
        float outer = u_spotlight_radius + u_spotlight_softness;          
        float t = smoothstep(inner, outer, dist);                         
        color *= (1.0 - u_spotlight_dim * t);                             
    }                                                                     
    gl_FragColor = vec4(color, 1.0);                                      
}                                                                         
