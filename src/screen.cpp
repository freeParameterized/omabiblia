// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "screen.h"
#include <algorithm>
#include <cstdio>
#include "math.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

static const char* kFsVS = R"(
out vec2 vUV;
void main(){ vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2); vUV=p; gl_Position=vec4(p*2.0-1.0,0,1); })";

// Wallpaper backdrop: cover-fit, blurred through the mip chain, tinted by the theme, with the
// screen's light spilling onto it.
static const char* kBgFS = R"(
in vec2 vUV; out vec4 o;
uniform sampler2D uWall; uniform int uHasWall; uniform vec2 uWin; uniform vec2 uWallSize;
uniform vec3 uBg; uniform vec3 uGlow; uniform vec2 uCenter; uniform vec2 uHalf; uniform float uDark; uniform float uT; uniform int uPop;
void main(){
  if(uPop==1){
    // pop-out: transparent desktop, only a soft premultiplied halo of screen light around the monitor
    vec2 p=vUV*uWin, d=max(abs(p-uCenter)-uHalf,0.0);
    float k=exp(-length(d)/(0.06*uWin.y))*(0.30+0.05*sin(uT*1.7));
    o=vec4(uGlow*k,k); return;
  }
  vec3 c;
  if(uHasWall==1){
    float wa=uWallSize.x/uWallSize.y, va=uWin.x/uWin.y;
    vec2 uv=vUV; if(va>wa){ uv.y=(uv.y-0.5)*wa/va+0.5; } else { uv.x=(uv.x-0.5)*va/wa+0.5; }
    uv.y=1.0-uv.y;
    c=textureLod(uWall,uv,4.5).rgb*0.6+textureLod(uWall,uv,6.0).rgb*0.4;
    c=mix(c,uBg,0.35);
  } else {
    c=mix(uBg, uBg*0.6, vUV.y);
  }
  c*=mix(1.0,0.55,uDark);                       // dark themes: dim the room so the screen pops
  vec2 p=vUV*uWin, d=max(abs(p-uCenter)-uHalf,0.0);
  float spill=exp(-length(d)/(0.18*uWin.y));
  c+=uGlow*spill*(0.22+0.04*sin(uT*1.7));
  float v=smoothstep(1.25,0.35,length(vUV-0.5)*1.4);
  o=vec4(c*mix(0.75,1.0,v),1);
})";

static const char* kBrightFS = R"(
in vec2 vUV; out vec4 o; uniform sampler2D uTex;
void main(){ vec3 c=texture(uTex,vUV).rgb; float l=max(c.r,max(c.g,c.b)); float k=smoothstep(0.75,1.6,l); o=vec4(c*k,1); })";

static const char* kBlurFS = R"(
in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform vec2 uDir;
void main(){
  float w[5]=float[](0.227027,0.1945946,0.1216216,0.054054,0.016216);
  vec3 c=texture(uTex,vUV).rgb*w[0];
  for(int i=1;i<5;i++){ c+=texture(uTex,vUV+uDir*float(i)*1.6).rgb*w[i]; c+=texture(uTex,vUV-uDir*float(i)*1.6).rgb*w[i]; }
  o=vec4(c,1);
})";

// The monitor: a quad slightly larger than the screen. Inside the screen rect it shows the
// scene through a CRT; around it, a bezel with a theme-coloured rim light; outside, transparent.
static const char* kCrtVS = R"(
layout(location=0) in vec2 aPos; uniform mat4 uMVP; uniform vec2 uHalf; out vec2 vP;
void main(){ vP=aPos; gl_Position=uMVP*vec4(aPos*uHalf,0,1); })";
static const char* kCrtFS = R"(
in vec2 vP; out vec4 o;
uniform sampler2D uScene; uniform sampler2D uBloom;
uniform vec2 uHalf; uniform float uBezel; uniform float uT; uniform float uCrt; uniform float uBloomK; uniform float uCurve;
uniform vec3 uBezelCol; uniform vec3 uRim; uniform int uRetro; uniform vec2 uLow;
float rbox(vec2 p, vec2 b, float r){ vec2 q=abs(p)-b+r; return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r; }
float bayer(vec2 p){ ivec2 i=ivec2(mod(p,4.0)); int m[16]=int[](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5); return float(m[i.y*4+i.x])/16.0-0.5; }
vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0); }
void main(){
  vec2 p=vP*uHalf;                       // local units, screen spans [-uHalf+uBezel, uHalf-uBezel]
  vec2 sh=uHalf-vec2(uBezel);
  float outer=rbox(p,uHalf,uBezel*0.9);
  if(outer>0.0) discard;
  float inner=rbox(p,sh,uBezel*0.35);
  if(inner>0.0){
    // bezel: darkish theme tone, rim light on the outer edge, inner lip
    float rim=smoothstep(-uBezel*0.25,0.0,outer);
    float lip=smoothstep(uBezel*0.25,0.0,inner);
    vec3 c=uBezelCol*(0.75+0.25*(vP.y*0.5+0.5))+uRim*(rim*1.6+lip*0.5);
    o=vec4(c,1); return;
  }
  vec2 uv=p/sh*0.5+0.5;
  // barrel curvature
  vec2 cc=uv*2.0-1.0; cc*=1.0+uCurve*0.06*dot(cc,cc)*vec2(1.0,1.0); uv=cc*0.5+0.5;
  if(any(lessThan(uv,vec2(0.0)))||any(greaterThan(uv,vec2(1.0)))){ o=vec4(0,0,0,1); return; }
  vec3 col;
  if(uRetro==1){
    vec2 px=floor(uv*uLow)+0.5;
    col=texture(uScene,px/uLow).rgb;
    col=floor(col*5.0+bayer(px)*0.9+0.5)/5.0;       // posterise + ordered dither, chunky palette
  } else {
    float ab=0.0009*uCrt*(1.0+length(cc));
    col.r=texture(uScene,uv+vec2(ab,0)).r; col.g=texture(uScene,uv).g; col.b=texture(uScene,uv-vec2(ab,0)).b;
  }
  col+=texture(uBloom,uv).rgb*uBloomK*1.4;
  col=aces(col*1.05);
  // scanlines + aperture mask + flicker
  float lines=uRetro==1?uLow.y:540.0;
  float sl=0.5+0.5*cos(uv.y*lines*6.2831853);
  col*=mix(1.0,0.78+0.22*sl,uCrt);
  float mx=mod(gl_FragCoord.x,3.0);
  vec3 mask=vec3(mx<1.0?1.0:0.86, mx>=1.0&&mx<2.0?1.0:0.86, mx>=2.0?1.0:0.86);
  col*=mix(vec3(1.0),mask,0.5*uCrt);
  col*=1.0-0.025*uCrt*sin(uT*120.0);
  // vignette + glass reflection
  col*=smoothstep(1.45,0.55,length(cc));
  float refl=smoothstep(0.35,0.0,abs(uv.x-uv.y*0.6-0.15))*0.05;
  col+=vec3(refl);
  o=vec4(col,1);
})";

void Screen::init() {
    bgProg_ = glxProgram("bg", kFsVS, kBgFS);
    brightProg_ = glxProgram("bright", kFsVS, kBrightFS);
    blurProg_ = glxProgram("blur", kFsVS, kBlurFS);
    crtProg_ = glxProgram("crt", kCrtVS, kCrtFS);
    const float q[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, nullptr);
}

void Screen::setWallpaper(const std::string& path) {
    if (path == wallPath_) return;
    wallPath_ = path;
    if (wall_) { glDeleteTextures(1, &wall_); wall_ = 0; }
    if (path.empty()) return;
    int n = 0;
    unsigned char* px = stbi_load(path.c_str(), &wallW_, &wallH_, &n, 4);
    if (!px) { std::fprintf(stderr, "omabiblia: cannot read wallpaper %s\n", path.c_str()); return; }
    wall_ = glxTexture(wallW_, wallH_, px, true, true);
    stbi_image_free(px);
}

void Screen::render(int W, int H, GLuint sceneTex, int sw, int sh, const Palette& pal, float t, const ScreenParams& p) {
    // ---- bloom from the scene ----
    bright_.ensure(sw / 2, sh / 2, false);
    blurA_.ensure(sw / 4, sh / 4, false);
    blurB_.ensure(sw / 4, sh / 4, false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    bright_.bind();
    glUseProgram(brightProg_);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glUniform1i(glGetUniformLocation(brightProg_, "uTex"), 0);
    glxFullscreen();
    glUseProgram(blurProg_);
    glUniform1i(glGetUniformLocation(blurProg_, "uTex"), 0);
    GLuint src = bright_.tex;
    for (int pass = 0; pass < 3; pass++) {
        blurA_.bind();
        glBindTexture(GL_TEXTURE_2D, src);
        glUniform2f(glGetUniformLocation(blurProg_, "uDir"), 1.0f / blurA_.w, 0);
        glxFullscreen();
        blurB_.bind();
        glBindTexture(GL_TEXTURE_2D, blurA_.tex);
        glUniform2f(glGetUniformLocation(blurProg_, "uDir"), 0, 1.0f / blurB_.h);
        glxFullscreen();
        src = blurB_.tex;
    }

    // ---- where the monitor sits ----
    float winAspect = (float)W / H;
    M4 proj = perspective(35 * PI / 180, winAspect, 0.1f, 100);
    float halfH = 1.0f, halfW = halfH * aspect, bezel = 0.07f;
    float dist = 1.0f / std::tan(17.5f * PI / 180);        // screen height fills the view at zoom 1
    dist = std::max(dist, (halfW + bezel) / (std::tan(17.5f * PI / 180) * winAspect));
    dist *= 1.10f / std::max(0.4f, p.zoom);
    float swayY = p.flat ? 0 : 0.035f * std::sin(t * 0.23f), swayX = p.flat ? 0 : 0.02f * std::sin(t * 0.31f);
    if (p.cinematic && !p.flat) { swayY += 0.55f * std::sin(t * 0.13f); swayX += 0.18f * std::sin(t * 0.09f + 1.0f); }
    float ry = p.flat ? 0 : p.yaw + swayY, rx = p.flat ? 0 : p.pitch + swayX;
    dist *= 1.0f + 0.45f * std::fabs(std::sin(ry)) + 0.3f * std::fabs(std::sin(rx));   // angled: back off so it stays in frame
    M4 model = rotY(ry) * rotX(rx);
    M4 mvp = proj * translate({0, 0, -dist}) * model;
    // projected monitor bounds (with bezel) -> click-through input region in pop-out mode
    rectX0 = 1e9f; rectY0 = 1e9f; rectX1 = -1e9f; rectY1 = -1e9f;
    for (int k = 0; k < 4; k++) {
        V4 q = mvp * V4((k & 1 ? 1 : -1) * (halfW + bezel), (k & 2 ? 1 : -1) * (halfH + bezel), 0, 1);
        float sx = (q.x / q.w * 0.5f + 0.5f) * W, sy = (1 - (q.y / q.w * 0.5f + 0.5f)) * H;
        rectX0 = std::min(rectX0, sx); rectX1 = std::max(rectX1, sx);
        rectY0 = std::min(rectY0, sy); rectY1 = std::max(rectY1, sy);
    }
    V4 cpos = mvp * V4(0, 0, 0, 1);
    V2 center{(cpos.x / cpos.w * 0.5f + 0.5f) * W, (cpos.y / cpos.w * 0.5f + 0.5f) * H};
    V2 half{halfW / dist / std::tan(17.5f * PI / 180) / winAspect * 0.5f * W, halfH / dist / std::tan(17.5f * PI / 180) * 0.5f * H};

    // ---- backdrop ----
    glxBindDefault(W, H);
    glUseProgram(bgProg_);
    glBindTexture(GL_TEXTURE_2D, wall_);
    glUniform1i(glGetUniformLocation(bgProg_, "uWall"), 0);
    glUniform1i(glGetUniformLocation(bgProg_, "uHasWall"), wall_ ? 1 : 0);
    glUniform2f(glGetUniformLocation(bgProg_, "uWin"), (float)W, (float)H);
    glUniform2f(glGetUniformLocation(bgProg_, "uWallSize"), (float)std::max(1, wallW_), (float)std::max(1, wallH_));
    glUniform3f(glGetUniformLocation(bgProg_, "uBg"), pal.background.x, pal.background.y, pal.background.z);
    V4 g = pal.glowA;
    glUniform3f(glGetUniformLocation(bgProg_, "uGlow"), g.x, g.y, g.z);
    glUniform2f(glGetUniformLocation(bgProg_, "uCenter"), center.x, center.y);
    glUniform2f(glGetUniformLocation(bgProg_, "uHalf"), half.x, half.y);
    glUniform1f(glGetUniformLocation(bgProg_, "uDark"), pal.dark ? 1.f : 0.f);
    glUniform1f(glGetUniformLocation(bgProg_, "uT"), t);
    glUniform1i(glGetUniformLocation(bgProg_, "uPop"), p.popout ? 1 : 0);
    if (p.popout) { glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT); }
    glxFullscreen();

    // ---- the monitor ----
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(crtProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, src);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(crtProg_, "uScene"), 0);
    glUniform1i(glGetUniformLocation(crtProg_, "uBloom"), 1);
    glUniformMatrix4fv(glGetUniformLocation(crtProg_, "uMVP"), 1, GL_FALSE, mvp.m);
    glUniform2f(glGetUniformLocation(crtProg_, "uHalf"), halfW + bezel, halfH + bezel);
    glUniform1f(glGetUniformLocation(crtProg_, "uBezel"), bezel);
    glUniform1f(glGetUniformLocation(crtProg_, "uT"), t);
    glUniform1f(glGetUniformLocation(crtProg_, "uCrt"), p.crt);
    glUniform1f(glGetUniformLocation(crtProg_, "uBloomK"), p.bloom);
    glUniform1f(glGetUniformLocation(crtProg_, "uCurve"), p.flat ? 0.3f * p.curvature : p.curvature);
    V4 bz = pal.dark ? scaleRGB(mix4(pal.background, pal.selection, 0.5f), 0.9f) : scaleRGB(pal.foreground, 1.0f);
    bz = mix4(bz, V4{0.05f, 0.05f, 0.07f, 1}, 0.5f);
    glUniform3f(glGetUniformLocation(crtProg_, "uBezelCol"), bz.x, bz.y, bz.z);
    glUniform3f(glGetUniformLocation(crtProg_, "uRim"), g.x * 0.6f, g.y * 0.6f, g.z * 0.6f);
    glUniform1i(glGetUniformLocation(crtProg_, "uRetro"), p.retro ? 1 : 0);
    glUniform2f(glGetUniformLocation(crtProg_, "uLow"), (float)p.lowW, (float)p.lowH);
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);
}
