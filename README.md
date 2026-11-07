# M5StickS3 Metronome · 音乐节拍器

一个跑在 **M5Stack StickS3** 上的口袋节拍器：多种视觉风格 + 机身喇叭发声，按键调速，摇一摇启停。

A pocket metronome for the **M5Stack StickS3**: multiple visual styles, built-in speaker clicks, button-based tempo control, and shake-to-start/stop.

---

## 中文

### 特性

- **速度范围**：默认 60–180 BPM，可解锁至 10–400 BPM
- **5 种视觉风格**：摆锤、涟漪、四拍圆点、全屏闪烁、弹跳球
- **声音**：每小节第 1 拍为重音（4/4 拍），音色随视觉风格变化
- **摇一摇**启停，也可以用按键启停
- **音量调节**：0–10 级，带试听
- **掉电记忆**：BPM、风格、音量开机自动恢复
- 显示意大利速度术语（Largo、Allegro、Presto 等）和速度进度条
- 使用微秒级计时，拍间误差不累积

### 操作

| 操作 | 功能 |
|---|---|
| A 单击 | BPM +1（按住连续增加，按住超过 2 秒每次 +5） |
| B 单击 | BPM -1（按住连续减少，按住超过 2 秒每次 -5） |
| A 双击 / 摇一摇 | 开始 / 结束 |
| B 双击 | 切换视觉风格 |
| A 三击 | 解锁 / 锁定 180–400 BPM |
| B 三击 | 解锁 / 锁定 10–60 BPM |
| A+B 同时按住 0.5 秒 | 进入 / 退出音量模式（A 音量+，B 音量-，3 秒无操作自动退出） |

### 编译与烧录

1. 安装 Arduino IDE，并添加 M5Stack 开发板支持包，开发板选择 **M5StickS3**
2. 在库管理器安装 **M5Unified** 和 **M5GFX**
3. 将 `M5StickS3_Metronome.ino` 放入同名文件夹 `M5StickS3_Metronome/`，打开后编译上传

### 可调参数

均在 `.ino` 文件顶部：

| 常量 | 说明 |
|---|---|
| `SCREEN_ROTATION` | 画面方向（0–3） |
| `BEATS_PER_BAR` | 每小节拍数（默认 4） |
| `SHAKE_THRESHOLD_G` / `SHAKE_SPIKES` | 摇一摇灵敏度 |
| `TONE_NORMAL` / `TONE_ACCENT` | 各风格的普通拍 / 重音频率 |

### 已知说明

- 单击需等待约 0.25 秒以区分双击和三击，所以单击调速略有延迟
- 画面刷新时节拍有 10 ms 以内的抖动，一般听不出来
- 解锁状态不会保存，重启后恢复默认 60–180 范围

### 路线图（想法）

- [ ] 拍号切换（3/4、6/8 等）
- [ ] Tap Tempo（敲击测速）
- [ ] 更多视觉风格
- [ ] 自定义重音模式

欢迎提 Issue 和 PR。

---

## English

### Features

- **Tempo range**: 60–180 BPM by default, unlockable down to 10 and up to 400 BPM
- **5 visual styles**: Pendulum, Ripple, Dots, Flash, Bounce
- **Sound**: accented first beat of each bar (4/4); the click timbre changes with the style
- **Shake to start/stop**, or use the buttons
- **Volume control**: 10 levels with audible preview
- **Persistent settings**: BPM, style and volume are restored on boot
- Shows Italian tempo markings (Largo, Allegro, Presto…) and a tempo bar
- Microsecond scheduling, so timing error does not accumulate between beats

### Controls

| Input | Action |
|---|---|
| A click | BPM +1 (hold to repeat; hold over 2 s for +5 steps) |
| B click | BPM −1 (hold to repeat; hold over 2 s for −5 steps) |
| A double-click / shake | Start / stop |
| B double-click | Cycle visual style |
| A triple-click | Unlock / lock 180–400 BPM |
| B triple-click | Unlock / lock 10–60 BPM |
| Hold A+B for 0.5 s | Enter / exit volume mode (A up, B down, auto-exits after 3 s) |

### Build

1. Install Arduino IDE with the M5Stack board package and select **M5StickS3**
2. Install the **M5Unified** and **M5GFX** libraries
3. Put `M5StickS3_Metronome.ino` in a folder of the same name, then compile and upload

### Notes

- Single clicks are delayed by about 0.25 s so double and triple clicks can be told apart
- Unlock state is not persisted; the device boots in the default 60–180 range
- Tunable constants (rotation, beats per bar, shake sensitivity, tones) are at the top of the sketch

---

## License / 许可证

[![CC BY-NC-SA 4.0](https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

本项目采用 [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.zh-hans) 许可协议。

- **署名（BY）**：使用、分享或修改本项目时，请注明作者 **【你的名字】** 并附上原项目链接
- **非商业（NC）**：禁止用于商业目的，包括出售本项目或基于它的产品
- **相同方式共享（SA）**：修改后再分发，须采用相同的许可协议

This project is licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).
You may share and adapt it with attribution to [Young] or [Yunqing] and a link to this repository, for non-commercial purposes only, and derivatives must use the same license.

Copyright (c) 2026 【Young】

## Acknowledgements

Built with [M5Unified](https://github.com/m5stack/M5Unified) and [M5GFX](https://github.com/m5stack/M5GFX).
