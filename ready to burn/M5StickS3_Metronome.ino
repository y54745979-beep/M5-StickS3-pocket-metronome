/*
 * M5StickS3 音乐节拍器
 *
 * 依赖库: M5Unified, M5GFX  (Arduino IDE 库管理器安装；开发板选 M5StickS3)
 *
 * 操作:
 *   A 单击   : BPM +1        (按住连续增加，按住超过2秒加速 +5)
 *   B 单击   : BPM -1        (按住连续减少，按住超过2秒加速 -5)
 *   A 双击 / 摇一摇 : 开始 / 结束
 *   B 双击   : 切换视觉风格 (5种，音色也随风格变化)
 *   A 三击   : 解锁 / 锁定 180-400 BPM
 *   B 三击   : 解锁 / 锁定 10-60 BPM
 *   默认范围 60-180 BPM
 */

#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>

// ---------------- 配置 ----------------
static const int      BPM_SAFE_MIN    = 60;    // 默认下限
static const int      BPM_SAFE_MAX    = 180;   // 默认上限
static const int      BPM_LOW_LIMIT   = 10;    // 解锁后下限
static const int      BPM_HIGH_LIMIT  = 400;   // 解锁后上限
static const int      BEATS_PER_BAR   = 4;     // 4/4 拍，每小节第1拍重音
static const int      STYLE_COUNT     = 5;
static const uint32_t FRAME_MS        = 33;    // ~30fps
static const int      SCREEN_ROTATION = 1;     // 画面方向，不满意改成 0/1/2/3
static const int      SPEAKER_VOLUME  = 200;   // 0-255

static const float    SHAKE_THRESHOLD_G = 1.2f; // 加速度偏离 1g 超过该值算一次"抖动"
static const int      SHAKE_SPIKES      = 3;    // 700ms 内抖动次数达到该值触发

static const char* STYLE_NAMES[STYLE_COUNT] = {"PENDULUM", "RIPPLE", "DOTS", "FLASH", "BOUNCE"};
static const uint16_t TONE_NORMAL[STYLE_COUNT] = {1000, 800, 1200, 600, 1400};
static const uint16_t TONE_ACCENT[STYLE_COUNT] = {1600, 1300, 1900, 1000, 2100};

// ---------------- 状态 ----------------
M5Canvas    canvas(&M5.Display);
Preferences prefs;

int      bpm          = 100;
bool     running      = false;
bool     lowUnlocked  = false;
bool     highUnlocked = false;
uint8_t  styleIdx     = 0;

uint32_t beatCount  = 0;   // 已触发的拍数
uint32_t lastBeatUs = 0;   // 上一拍时间
uint32_t nextBeatUs = 0;   // 下一拍时间

char     toastText[32] = "";
uint32_t toastUntil    = 0;

bool     prefsDirty   = false;
uint32_t prefsDirtyAt = 0;
uint32_t lastFrameMs  = 0;

// ---------------- 工具函数 ----------------
static inline int minBpm() { return lowUnlocked ? BPM_LOW_LIMIT : BPM_SAFE_MIN; }
static inline int maxBpm() { return highUnlocked ? BPM_HIGH_LIMIT : BPM_SAFE_MAX; }
static inline uint32_t intervalUs() { return 60000000UL / (uint32_t)bpm; }

static uint16_t rgb(float r, float g, float b, float k = 1.0f) {
  if (k < 0) k = 0;
  if (k > 1) k = 1;
  return lgfx::color565((uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k));
}

static const char* tempoName(int b) {
  if (b < 40)  return "Grave";
  if (b < 60)  return "Largo";
  if (b < 76)  return "Adagio";
  if (b < 108) return "Andante";
  if (b < 120) return "Moderato";
  if (b < 156) return "Allegro";
  if (b < 200) return "Presto";
  return "Prestissimo";
}

static void showToast(const char* msg, uint32_t ms = 1500) {
  strncpy(toastText, msg, sizeof(toastText) - 1);
  toastText[sizeof(toastText) - 1] = 0;
  toastUntil = millis() + ms;
}

static void markDirty() {
  prefsDirty = true;
  prefsDirtyAt = millis();
}

// ---------------- 节拍核心 ----------------
static void setBpm(int v) {
  int lo = minBpm(), hi = maxBpm();
  if (v > hi) {
    v = hi;
    if (bpm == hi) showToast(highUnlocked ? "MAX 400" : "MAX 180 (A x3 unlock)");
  } else if (v < lo) {
    v = lo;
    if (bpm == lo) showToast(lowUnlocked ? "MIN 10" : "MIN 60 (B x3 unlock)");
  }
  if (v != bpm) {
    bpm = v;
    if (running && beatCount > 0) nextBeatUs = lastBeatUs + intervalUs();
    markDirty();
  }
}

static void toggleRun() {
  running = !running;
  if (running) {
    beatCount = 0;
    nextBeatUs = micros();
    lastBeatUs = nextBeatUs - intervalUs();
    showToast("START", 700);
  } else {
    M5.Speaker.stop();
    showToast("STOP", 700);
  }
}

static void toggleHighUnlock() {
  highUnlocked = !highUnlocked;
  if (highUnlocked) {
    showToast("UNLOCK 180-400");
  } else {
    showToast("LOCK  max 180");
    if (bpm > BPM_SAFE_MAX) setBpm(BPM_SAFE_MAX);
  }
}

static void toggleLowUnlock() {
  lowUnlocked = !lowUnlocked;
  if (lowUnlocked) {
    showToast("UNLOCK 10-60");
  } else {
    showToast("LOCK  min 60");
    if (bpm < BPM_SAFE_MIN) setBpm(BPM_SAFE_MIN);
  }
}

static void nextStyle() {
  styleIdx = (styleIdx + 1) % STYLE_COUNT;
  showToast(STYLE_NAMES[styleIdx], 900);
  markDirty();
}

static void tickMetronome() {
  if (!running) return;
  uint32_t now = micros();
  int32_t late = (int32_t)(now - nextBeatUs);
  if (late < 0) return;
  if (late > (int32_t)intervalUs()) nextBeatUs = now;  // 落后太多则重新对齐，避免连发

  bool accent = (beatCount % BEATS_PER_BAR) == 0;
  M5.Speaker.tone(accent ? TONE_ACCENT[styleIdx] : TONE_NORMAL[styleIdx], 30);

  beatCount++;
  lastBeatUs = nextBeatUs;
  nextBeatUs += intervalUs();
}

static float beatPhase() {
  if (!running || beatCount == 0) return 0.0f;
  float p = (float)(uint32_t)(micros() - lastBeatUs) / (float)intervalUs();
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  return p;
}

// ---------------- 输入 ----------------
static uint32_t holdLastStep[2] = {0, 0};

static void holdRepeat(m5::Button_Class& b, int dir, int idx) {
  if (b.isPressed() && b.pressedFor(600)) {
    bool fast = b.pressedFor(2000);
    uint32_t gap = fast ? 60 : 90;
    if (millis() - holdLastStep[idx] >= gap) {
      holdLastStep[idx] = millis();
      setBpm(bpm + dir * (fast ? 5 : 1));
    }
  }
}

static void handleButtons() {
  if (M5.BtnA.wasDecideClickCount()) {
    switch (M5.BtnA.getClickCount()) {
      case 1: setBpm(bpm + 1);   break;
      case 2: toggleRun();       break;
      case 3: toggleHighUnlock(); break;
      default: break;
    }
  }
  if (M5.BtnB.wasDecideClickCount()) {
    switch (M5.BtnB.getClickCount()) {
      case 1: setBpm(bpm - 1);   break;
      case 2: nextStyle();       break;
      case 3: toggleLowUnlock(); break;
      default: break;
    }
  }
  holdRepeat(M5.BtnA, +1, 0);
  holdRepeat(M5.BtnB, -1, 1);
}

static void handleShake() {
  static uint32_t winStart = 0, lastSpike = 0, lockUntil = 0;
  static uint8_t spikes = 0;

  M5.Imu.update();
  float ax = 0, ay = 0, az = 0;
  M5.Imu.getAccel(&ax, &ay, &az);
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float dev = fabsf(mag - 1.0f);

  uint32_t now = millis();
  if (now < lockUntil) return;

  if (dev > SHAKE_THRESHOLD_G && now - lastSpike > 90) {
    lastSpike = now;
    if (spikes == 0 || now - winStart > 700) {
      winStart = now;
      spikes = 0;
    }
    spikes++;
    if (spikes >= SHAKE_SPIKES) {
      spikes = 0;
      lockUntil = now + 1200;
      toggleRun();
    }
  }
}

// ---------------- 绘制 ----------------
static void drawBpmNumber(int x, int y, textdatum_t datum, float size = 1.0f) {
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(size);
  canvas.setTextDatum(datum);
  canvas.setTextColor(running ? TFT_WHITE : rgb(170, 170, 170));
  canvas.drawNumber(bpm, x, y);
  canvas.setTextSize(1);
}

static void drawBpmLabel(int x, int y) {
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(rgb(120, 120, 120));
  canvas.drawString("BPM", x, y);
}

// 风格0: 摆锤
static void drawPendulum(float p, int cur, bool accent) {
  const int px = 182, py = 98;
  const float L = 76.0f;
  drawBpmNumber(8, 30, top_left);
  drawBpmLabel(10, 80);

  // 机身
  canvas.drawLine(px - 34, py, px, py - 84, rgb(70, 70, 70));
  canvas.drawLine(px + 34, py, px, py - 84, rgb(70, 70, 70));
  canvas.fillRect(px - 40, py, 80, 4, rgb(90, 90, 90));

  float ang = 0;
  if (running && cur >= 0) ang = 32.0f * cosf(PI * ((float)cur + p));
  float rad = ang * DEG_TO_RAD;
  int tx = px + (int)(L * sinf(rad));
  int ty = py - (int)(L * cosf(rad));
  int wx = px + (int)(L * 0.62f * sinf(rad));
  int wy = py - (int)(L * 0.62f * cosf(rad));

  bool hit = running && p < 0.2f;
  uint16_t rodC = hit ? (accent ? rgb(255, 120, 0) : rgb(0, 200, 255)) : TFT_WHITE;
  for (int d = -1; d <= 1; d++) canvas.drawLine(px + d, py, tx + d, ty, rodC);
  canvas.fillCircle(wx, wy, 7, rodC);
  canvas.fillCircle(px, py, 4, rgb(150, 150, 150));
}

// 风格1: 涟漪
static void drawRipple(float p, int cur, bool accent) {
  const int cx = 176, cy = 50;
  drawBpmNumber(8, 30, top_left);
  drawBpmLabel(10, 80);

  float r = accent ? 255 : 0, g = accent ? 120 : 200, b = accent ? 0 : 255;
  if (!running || cur < 0) {
    canvas.drawCircle(cx, cy, 14, rgb(80, 80, 80));
    return;
  }
  float k = 1.0f - p * 0.9f;
  int r1 = 12 + (int)(36 * p);
  int r2 = 12 + (int)(22 * p);
  for (int d = -1; d <= 1; d++) canvas.drawCircle(cx, cy, r1 + d, rgb(r, g, b, k));
  canvas.drawCircle(cx, cy, r2, rgb(r, g, b, k * 0.6f));
  canvas.fillCircle(cx, cy, 6 + (int)((1.0f - p) * 10), rgb(r, g, b, 0.35f + 0.65f * (1.0f - p)));
}

// 风格2: 四拍圆点
static void drawDots(float p, int cur, bool accent) {
  (void)accent;
  drawBpmNumber(120, 4, top_center);
  for (int i = 0; i < BEATS_PER_BAR; i++) {
    int x = 36 + i * 56, y = 80;
    bool active = running && cur >= 0 && (cur % BEATS_PER_BAR) == i;
    float r = (i == 0) ? 255 : 0, g = (i == 0) ? 60 : 210, b = (i == 0) ? 60 : 255;
    if (active) {
      canvas.fillCircle(x, y, 16, rgb(r, g, b, 1.0f - 0.5f * p));
      canvas.drawCircle(x, y, 16 + (int)(9 * p), rgb(r, g, b, 1.0f - p));
    } else {
      canvas.drawCircle(x, y, 16, rgb(70, 70, 70));
    }
  }
}

// 风格3: 全屏闪烁
static void drawFlash(float p, int cur, bool accent) {
  float k = 0;
  if (running && cur >= 0) k = 1.0f - p * 1.6f;
  uint16_t bg = accent ? rgb(210, 30, 30, k) : rgb(30, 90, 220, k);
  canvas.fillScreen(bg);
  drawBpmNumber(120, 50, middle_center, 1.2f);
}

// 风格4: 弹跳球
static void drawBounce(float p, int cur, bool accent) {
  const int ground = 94;
  const int radius = 8;
  drawBpmNumber(120, 2, top_center, 0.7f);
  canvas.drawFastHLine(10, ground, 220, rgb(90, 90, 90));

  float x = 120, h = 0;
  if (running && cur >= 0) {
    bool even = (cur % 2) == 0;
    x = 24 + 192.0f * (even ? p : 1.0f - p);
    h = 40.0f * 4.0f * p * (1.0f - p);
  }
  int bx = (int)x;
  int by = ground - radius - (int)h;

  // 阴影
  int sw = 12 - (int)(h * 0.15f);
  canvas.fillEllipse(bx, ground + 2, sw, 2, rgb(60, 60, 60));

  float r = accent ? 255 : 0, g = accent ? 120 : 220, b = accent ? 0 : 255;
  uint16_t c = rgb(r, g, b);
  if (running && (p < 0.1f || p > 0.9f)) {
    canvas.fillEllipse(bx, ground - 6, radius + 4, 6, c);  // 落地压扁
  } else {
    canvas.fillCircle(bx, by, radius, c);
  }
}

static void drawHud() {
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(rgb(160, 160, 160));
  canvas.setTextDatum(bottom_left);
  canvas.drawString(STYLE_NAMES[styleIdx], 4, 122);
  canvas.setTextDatum(bottom_center);
  canvas.drawString(tempoName(bpm), 120, 122);

  char rng[16];
  snprintf(rng, sizeof(rng), "%d-%d", minBpm(), maxBpm());
  bool unlocked = lowUnlocked || highUnlocked;
  canvas.setTextColor(unlocked ? rgb(255, 160, 0) : rgb(160, 160, 160));
  canvas.setTextDatum(bottom_right);
  canvas.drawString(rng, 236, 122);

  // 进度条
  int bx = 8, bw = 224, by = 127;
  canvas.fillRoundRect(bx, by, bw, 4, 2, rgb(40, 40, 40));
  float t = (float)(bpm - minBpm()) / (float)(maxBpm() - minBpm());
  int mx = bx + (int)(t * bw);
  bool outside = (bpm < BPM_SAFE_MIN) || (bpm > BPM_SAFE_MAX);
  canvas.fillRoundRect(bx, by, mx - bx + 1, 4, 2, outside ? rgb(255, 160, 0) : rgb(0, 200, 120));
  canvas.fillCircle(mx, by + 2, 3, TFT_WHITE);

  // 运行 / 停止图标
  if (running) {
    canvas.fillTriangle(226, 4, 226, 16, 236, 10, rgb(0, 220, 100));
  } else {
    canvas.fillRect(226, 4, 4, 12, rgb(230, 60, 60));
    canvas.fillRect(233, 4, 4, 12, rgb(230, 60, 60));
  }
}

static void drawToast() {
  if (millis() >= toastUntil) return;
  canvas.setFont(&fonts::Font2);
  canvas.setTextDatum(middle_center);
  int w = canvas.textWidth(toastText) + 20;
  int x = 120 - w / 2;
  canvas.fillRoundRect(x, 78, w, 24, 6, rgb(20, 20, 20));
  canvas.drawRoundRect(x, 78, w, 24, 6, rgb(255, 210, 0));
  canvas.setTextColor(rgb(255, 210, 0));
  canvas.drawString(toastText, 120, 90);
}

static void drawFrame() {
  float p = beatPhase();
  int cur = (int)beatCount - 1;
  bool accent = (cur >= 0) && ((cur % BEATS_PER_BAR) == 0);

  canvas.fillScreen(TFT_BLACK);
  switch (styleIdx) {
    case 0: drawPendulum(p, cur, accent); break;
    case 1: drawRipple(p, cur, accent);   break;
    case 2: drawDots(p, cur, accent);     break;
    case 3: drawFlash(p, cur, accent);    break;
    case 4: drawBounce(p, cur, accent);   break;
  }
  drawHud();
  drawToast();
  canvas.pushSprite(0, 0);
}

// ---------------- Arduino ----------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(SCREEN_ROTATION);
  M5.Display.setBrightness(120);
  M5.Speaker.setVolume(SPEAKER_VOLUME);

  canvas.setColorDepth(16);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  prefs.begin("metronome", false);
  bpm      = constrain(prefs.getInt("bpm", 100), BPM_SAFE_MIN, BPM_SAFE_MAX);
  styleIdx = prefs.getUChar("style", 0) % STYLE_COUNT;

  showToast("A+ B-  2xA start", 2000);
}

void loop() {
  M5.update();
  handleButtons();
  handleShake();
  tickMetronome();   // 节拍检查放在绘制之前，减小抖动

  uint32_t now = millis();
  if (now - lastFrameMs >= FRAME_MS) {
    lastFrameMs = now;
    drawFrame();
    tickMetronome();
  }

  if (prefsDirty && now - prefsDirtyAt > 2000) {
    prefs.putInt("bpm", constrain(bpm, BPM_SAFE_MIN, BPM_SAFE_MAX));
    prefs.putUChar("style", styleIdx);
    prefsDirty = false;
  }
  delay(1);
}
