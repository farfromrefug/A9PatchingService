#extension GL_OES_EGL_image_external : require
precision mediump float;
uniform samplerExternalOES texUnit;
uniform float opacity;
uniform float gamma;
varying vec2 UV;

void main() {{
    vec4 color = texture2D(texUnit, UV);
    vec2 adjustedUV = vec2(UV.x, UV.y * 2.0);

    float dist = distance(adjustedUV, vec2({center_x}, {center_y}));

    float xDist = abs(adjustedUV.x - {center_x});
    float yDist = abs(adjustedUV.y - {center_y});

    if ((dist < {radius} && dist > {round(radius * 0.85, 3)}) || (xDist > {round(radius * 0.15, 3)} && xDist < {round(radius * 0.4, 3)} && yDist < {round(radius * 0.45, 3)})) {{
        vec3 rgb2 = mix(vec3({mix_color}, {mix_color}, {mix_color}), color.rgb, opacity * {opacity} + (1.0 - {opacity}));
        gl_FragColor = vec4(rgb2, 1.0);
    }} else {{
        vec3 rgb = mix(vec3({bg_mix_color}, {bg_mix_color}, {bg_mix_color}), color.rgb, opacity * {bg_opacity} + (1.0 - {bg_opacity}));
        gl_FragColor = vec4(rgb, 1.0);
    }}
}}