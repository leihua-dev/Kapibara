# MotifForge v0.3 中心架构

## 0. 总体结构

MotifForge v0.3 不再把系统定义成单一“合成器链路”，而是定义成五层音乐对象：

```text
Seed
→ Creator
→ Motif
→ Structure
→ MixingArrangeView
→ Audio / Stems
```

一句话边界：

```text
Seed = 声音怎么产生。
Creator = 声音怎么被演奏、拼贴、重采样。
Motif = 乐思怎么组织。
Structure = 作品怎么展开。
MixingArrangeView = 最后怎么混、怎么导出。
```

最重要的工程原则：

```text
上层引用下层，不深拷贝下层。
变化通过 override / automation / transform 表示。
只有 freeze / resampling / stem render 时才真正烘焙为 audio buffer。
```

这条原则用来避免工程文件爆炸，也避免一个 Seed 修改后所有 Motif / Structure 复制品不同步。

### 0.0.1 Seed Layer

`Seed` 是声音原子层。它只负责一个声音生命体的生成、调制和局部音色塑形。

```text
Seed
├── Generator
│   ├── Spectral / FunctionalSampleSource path
│   └── SamplePlayback path
├── Generator Preprocessor
├── SpectralTimeline bake path
├── Operator
├── Matrix / Performance Mapping
├── RenderState
├── Tone FX
└── Seed Output
```

严格边界：

```text
Seed 可以知道 partial、sample playback、operator、matrix、Tone FX。
Seed 不知道 DrumRack、PianoLocks、Motif、Structure、Arrange、Mixer。
```

`Tone FX` 是原来第一层 `FXChain 1` 的新语义名。它是音色身份的一部分，例如 distortion、filter、chorus、resonator、局部 freeze character。EQ / compressor / send reverb / limiter 这类混音处理不属于 Seed。

`SamplePlaybackGenerator` 是 Seed 里的普通采样播放声源，但它不是 partial generator。它在 Generator UI 中配置，运行时走独立 sample voice path，不生成 `SpectralTimeline` partial 数据，也不进入 Operator / Matrix 的 partial 协议链。

### 0.0.2 Creator Layer

`Creator` 是可演奏声音对象层。它把多个 Seed 组织成一个可以被键盘、鼓垫、钢琴窗或重采样系统触发的对象。

```text
Creator
├── SeedInstance refs
├── Mapping / KeyZone / Velocity layer
├── DrumRack
├── PianoLocks / NoteGrid
├── SampleCraft
│   ├── ResamplingEngine
│   ├── Recorder
│   ├── BufferSlot import
│   ├── SliceProcessor
│   ├── GranularProcessor
│   └── LoopProcessor
├── Creator Macro Controls
└── Creator Output
```

严格边界：

```text
Creator 可以组合 Seed、映射演奏方式、做局部 resampling / collage。
Creator 不负责完整乐曲时间轴，也不负责最终混音。
```

`SampleCraft` 属于 Creator。它处理 Creator 内部声音材料的冻结、切片、变调、loop、reverse、granular、stutter 和拼贴，但不能吞掉 Motif / Structure 的职责。

### 0.0.3 Motif Layer

`Motif` 是乐思层。它不是完整 DAW arrangement，而是一个短句、pattern、riff、gesture 或声音设计动机。

```text
Motif
├── CreatorInstance refs
├── Local Piano Roll / PianoLocks view
├── Local Automation
├── Motif Variations
├── Motif Resample / Freeze
└── Motif Output
```

Motif 的默认限制：

```text
长度：1/4 bar 到 16 bars，默认 1 / 2 / 4 / 8 bars。
轨道：有限数量 Creator lane。
自动化：局部表情为主，不做全曲宏观 automation。
变体：A、A'、density+、register shift、inversion 等 transform。
```

严格边界：

```text
Motif 可以引用 Creator 并安排事件。
Motif 不复制 Creator 全部数据。
Motif 不承担全曲段落结构。
```

### 0.0.4 Structure Layer

`Structure` 是宏观形式层。它把 Motif 当成 pattern / phrase / developed motif 来排列，并管理段落、能量、密度和全局转变。

```text
Structure
├── MotifClip refs
├── Section Graph
├── Timeline Arrangement
├── Global Automation
├── Motif Development Rules
└── Structure Output
```

内部可以拆成：

```text
StructureGraph: Intro -> A -> A' -> Break -> B -> A''
TimelineArrange: Motif 在真实时间轴上的 placement
```

严格边界：

```text
Structure 可以做 motif placement、section、global automation、development transform。
Structure 不直接编辑 Seed partial，也不替代 MixingArrangeView 的混音职责。
```

### 0.0.5 MixingArrangeView

`MixingArrangeView` 是最终工程 / 混音 / stem 视图。它可以像 Pro Tools 那样展示线性轨道、分轨波形、bus、send 和 master chain，也可以切换波形 / 瀑布 / partial preview。

```text
MixingArrangeView
├── Track View
├── Seed Mix foldout
├── Creator Mix foldout
├── Motif Bus foldout
├── Structure Track foldout
├── Send FX
├── Master Chain
└── Stem Render
```

混音层级：

```text
Seed Mix：音色内部平衡。
Creator Mix：乐器对象平衡。
Motif Bus：乐思组平衡。
Structure Track：编曲轨道平衡。
Master Mix：最终输出。
```

严格边界：

```text
MixingArrangeView 可以做音量、pan、send、bus、stem、master。
MixingArrangeView 不改变 Seed / Creator / Motif 的身份数据。
```

### 0.0.6 当前 JUCE v0.3 实施状态

已经落地的 v0.3 骨架：

```text
CompositionProject
├── SeedPreset
├── CreatorPreset
├── MotifClip
├── StructureProject
├── MixerState
└── v0.2 compatibility fields
```

`ParameterScope` 已从旧的 Track / Pattern 语义扩展为：

```text
Global / Seed / Creator / Motif / Structure / Mixing / Note
```

`Track = Seed`、`Pattern = Motif` 作为 v0.2 兼容别名保留。

当前 UI 页签对应关系：

```text
General               -> preset import/export, MIDI input, audio device, buffer size, panic
Seed                  -> Generator / FunctionalSampleSource / SamplePlayback
Seed Operators        -> Operator
Seed Matrix           -> Matrix / Performance Mapping
Tone FX               -> Seed-local character FX
Creator SampleCraft   -> ResamplingEngine / buffer craft
Mix FX                -> post-resampling mix FX
Creator Piano Locks   -> per-note / per-partial parameter locks
```

当前 Seed 视图里的 partial inspector 已升级为四层信息图：

```text
Partial Inspector
├── per-partial ADSR glyphs
├── position relative to partial 1 + harmonic deviation dots
├── current amp + timeline peak energy bars
└── phase hue strip + phase direction ticks
```

显示语义：

- ADSR glyph 使用全局 ADSR 曲线乘以 `attackScale / decayScale / sustainLevel / releaseScale`，让每根 partial 的局部包络差异可以同时看到。
- position relative to partial 1 使用 `nu_i / nu_1` 映射，并显示 1x、2x、3x... 的 harmonic grid。dot 越偏离最近整数泛音，越向 magenta 放大，所以 harmonic / inharmonic 的实时位置能直接看出来。
- amp bars 用亮柱表示当前 inspector frame 的 `amp_i`，暗柱表示 `SpectralTimeline` 内该 partial 的峰值能量，方便看 FunctionalSampleSource 的时间演化材料。
- phase strip 用 hue 表示 note-on phase，细 tick 表示相位方向；`phaseDriftHz / phaseJitter` 会提高显示强度，提示动态相位不稳定或 residual 倾向。

后续 UI 会继续把 Motif / Structure / MixingArrangeView 展开成独立视图；v0.3 先把数据模型和责任边界固定。

---

## 0.1 Seed 内部 Spectral 主链路

Seed 的 spectral path 仍然使用 `SpectralTimeline` 作为 partial 统一表示：

```text
Generator
→ Generator Preprocessor
→ SpectralTimeline
→ Operator
→ Matrix / Performance Mapping
→ RenderState
→ Tone FX
→ Seed Output
```

核心原则：

```text
Generator 负责产生声音结构。
Generator Preprocessor 负责把频率形状、振幅分布、shape mode、phase init rule 等通用规则应用到 generator 的原始 partial seed 上。
SpectralTimeline 负责统一表示 partial 结构和 source-intrinsic trajectory。
Operator 负责写入规则、写入参数、烘焙结果。
Matrix / Performance Mapping 负责演奏时实时 modulation。
Renderer 只负责发声。
Tone FX 只负责 Seed-local character processing。
```

主 spectral renderer 不直接理解 ODE、PDE、采样分析或具体绘图函数。

主系统只理解统一的 partial timeline：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

其中：

- $\nu_i^{src}(t)$：frequency descriptor。根据 `freqMode`，它可以表示 ratio，也可以表示 absolute Hz。
- $a_i^{src}(t)$：source amplitude。
- $x_i\in[0,1]$：normalized spectral position，用于 Matrix / shape / mask。
- $\mu_i$：metadata，例如 group、source type、material、mode type。

注意：`P_i^{src}` 不保存 audio-rate source phase trajectory。phase 统一交给 runtime phase system 处理；generator 只可以提供 note-on phase init 和压缩后的 phase drift metadata。

---

## 0.2 Frequency Descriptor

为了避免同时保存 $f_i$ 和 $\rho_i$ 造成冲突，使用：

$$

\nu_i^{src}(t)

$$

并配合：

```text

freqMode ∈ {RelativeRatio, AbsoluteHz}

```

### RelativeRatio Mode

如果：

```text

freqMode = RelativeRatio

```

则：

$$

\nu_i^{src}(t)=\rho_i(t)

$$

其中：

$$

\rho_i(t)=\frac{f_i^{src}(t)}{f_0(t)}

$$

实际频率由 voice 的基频计算：

$$

f_i^{src}(t)=f_0(t)\rho_i(t)

$$

例如：

$$

\rho_3=3.02

$$

表示第 3 个 partial 稍微偏高。

### AbsoluteHz Mode

如果：

```text

freqMode = AbsoluteHz

```

则：

$$

\nu_i^{src}(t)=f_i^{src}(t)

$$

实际频率就是：

$$

f_i^{src}(t)=\nu_i^{src}(t)

$$

这个模式适合没有明确基频的 PDE / modal source，例如板、膜、金属体。

---

## 0.3 $x_i$ 和 $\rho_i$ 的区别

$\rho_i$ 决定频率倍率。

$x_i$ 决定这个 partial 在控制空间里的位置。

不要混用。

可以简单定义：

$$

x_i=\frac{i-1}{N-1}

$$

也可以按频率倍率对数归一化：

$$

x_i

=

\frac{

\log \rho_i-\log \rho_{\min}

}{

\log \rho_{\max}-\log \rho_{\min}

}

$$

总结：

```text

rho_i: 频率倍率，例如 3.02

x_i: 0-1 控制位置

```

---

## 0.3 Phase 统一原则

系统不在 `P_i^{src}` 中保存完整 audio-rate source phase trajectory。

generator 可以提供两类 phase metadata：

```text

phaseLocked_i   note-on phase init
phaseDriftHz_i  relative dynamic phase drift summary

```

每个 active voice 持有 runtime phase accumulator：

$$

\theta_i[n]

$$

初始相位由 phase init function 决定：

$$

\theta_i[n_0]

=

\Phi_i^{init}

\left(

i,

x_i,

\mu_i,

note,

velocity,

seed

\right)

$$

之后所有相位连续性都由 runtime phase accumulator 负责：

$$

\theta_i[n+1]

=

\theta_i[n]

- 

2\pi

\frac{

f_i^{final}[n]

}{

F_s

}

$$

最终输出：

$$

y[n]

=

\sum_{i=1}^{N}

a_i^{final}[n]

\cos

\left(

\theta_i[n]

- 

\Delta\phi_i[n]

\right)

$$

其中：

- $\theta_i[n]$：runtime phase accumulator。
- $\Delta\phi_i[n]$：Matrix 产生的 phase offset modulation，可选。
- $\Phi_i^{init}$：初始相位函数。

第一版建议：

$$

\Delta\phi_i[n]=0

$$

先不要做 phase modulation。

---

# 1. Generator

Generator 是声音来源。

## 1.0 Partial Count 上限与 Audible Cap

每个 Generator / Source 的 `partialCount` 上限由两层共同约束：

```text

hardCap   = kMaxPartials                                  // 静态硬上限
audibleCap(f0) = floor(kAudibleMaxHz / f0)                // 与音高相关的智能上限
N_max     = min(hardCap, audibleCap(f0))

```

其中：

- `kMaxPartials = 256`（编译期硬上限，决定所有 partial-shaped 数组的大小）。
- `kAudibleMaxHz = 20000 Hz`。任何位置使 partial 频率超过 20 kHz 都不再有听感意义，必须被裁剪掉。
- `partialMaxRefHz` 是 UI / preset 用来计算 `N_max` 的参考 f0（默认 110 Hz / A2）。它本身不影响 Voice 渲染，只影响"Max"按钮一键填入的 `partialCount` 值。
- Voice 在每个 control block 还会按当前实际 `f0` 重新裁剪 `activeCount = min(frame.partialCount, audibleCap(f0))`，从而不为完全位于 20 kHz 以上的 partial 浪费 CPU。

UI 流程：

```text

(GUI) partialMaxRefHz, partialCount slider
     ↓
   "Max N" = floor(20000 / partialMaxRefHz)
     ↓
   user clicks "Set N = Max" → partialCount := Max N
     ↓
   per-voice render: activeCount = min(partialCount, floor(20000 / f_0))

```

不同 Generator 内部数学可以不同，但最后都必须输出：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

可用 Generator：

```text

DirectPartialGenerator

ModalODEGenerator

PDEModalGenerator

HarmonicGenerator

InharmonicGenerator

FunctionalSampleSourceGenerator

```

注意：`DrawnSpectrumGenerator` 不再作为独立 generator。手绘频谱、函数频谱、表格 partial 都合并进 `DirectPartialGenerator`。

注意：sample 入口经过两次重命名 —— v0.1 `SampleAnalysisGenerator`（单帧 FFT peak picker，无时间演化）→ v0.2 `SamplePartialSetGenerator`（双 pass STFT + per-partial ADSR fit）→ v0.3 `FunctionalSampleSourceGenerator`（真正的 partial track linking + x-t 分离的 functional spectral model）。完整新管线见 §1.1.2。

---

# 1A. Generator Preprocessor

Generator Preprocessor 位于：

```text

Generator

→ Generator Preprocessor

→ SpectralTimeline

```

它独立于具体 generator。

Generator 可以只输出更原始的 partial seed，例如：

```text

index
base ratio / base Hz
base amplitude
x_i
mu_i
material / group metadata

```

Preprocessor 再统一应用：

```text

Frequency Shape Function
Amplitude Distribution Function
Shape Modes
Phase Init Function
normalization / random seed / grouping post process

```

这样 `DirectPartialGenerator`、`ModalODEGenerator`、`PDEModalGenerator`、`FunctionalSampleSourceGenerator` 都可以共享同一套 spectral shaping 语言。

注意：这里的 Frequency / Amplitude / Shape / Phase 不是 generator 内部参数。

它们是 generator 之后的进一步偏移处理：

```text

Generator base seed
  → frequency offset
  → amplitude distribution offset
  → shape / focus / random post-shape
  → phase init rule
  → normalized P_i^src

```

例如：

```text

ModalODEGenerator 先给出 modal ratio table。
Generator Preprocessor 再对这个 modal table 做 inharmonic offset / spectral tilt / focus / phase init。

PDEModalGenerator 先给出 body modal table。
Generator Preprocessor 再对这个 body table 做同一套偏移。

FunctionalSampleSourceGenerator 先从单音样本提取 functional spectral source（§1.1.2）。
Generator Preprocessor 只继续处理它的 partial 频率位置；其 sample-derived amp 包络、locked phase、basis 权重默认保留。
之后 Operator、global ADSR、Matrix 仍然可以继续控制它的 amp、frequency、phase offset、decay/release 等演奏时结果。

DirectPartialGenerator 只给出最简单的默认 harmonic seed。
Generator Preprocessor 再把这个 seed 塑造成自定义 partial 合成。

```

严格边界：

```text

Generator:
  负责产生 source-specific seed / modal table / sampled peak table。

Generator Preprocessor:
  负责把通用 spectral editing rule 写成 P_i^src 之前的预处理结果。

SpectralTimeline:
  只接收统一后的 P_i^src(t)。

```

第一版代码使用独立的 `SpectralPreprocessParams` 表示这一层。

`DirectPartialParams` 只负责最基础的 partial seed，例如 `partialCount`。

---

## 1.1 DirectPartialGenerator

`DirectPartialGenerator` 是第一版最基础的 seed generator。

它先生成一组简单 partial seed：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

如果不经过 Generator Preprocessor，它接近：

```text

nu_i = i
amp_i = 1
x_i = (i - 1) / (N - 1)
mu_i = low / mid / high group

```

真正的 harmonic/inharmonic shape、tilt、focus、random、phase init 都在 `Generator Preprocessor` 中发生。

基础声音形式：

$$

y(t)

=

\sum_{i=1}^{N}

A_i(t)

\cos

\left(

\theta_i(t)

\right)

$$

其中：

- $A_i(t)$：第 $i$ 个 partial 的 amplitude trajectory。
- $\theta_i(t)$：runtime phase。
- 频率由 $\nu_i^{src}(t)$ 和 `freqMode` 决定。

---

## 1.1.1 手动 Partial Table

最直接的形式是手动给出 partial table：

$$

Partial_i

=

\left(

\rho_i,

A_i,

x_i,

\mu_i

\right)

$$

此时通常使用：

```text

freqMode = RelativeRatio

```

所以：

$$

\nu_i^{src}=\rho_i

$$

$$

f_i^{src}(t)=f_0(t)\rho_i

$$

例如：

| i | rho_i | A_i | x_i | group |

|---|---:|---:|---:|---|

| 1 | 1.00 | 1.00 | 0.00 | low |

| 2 | 2.01 | 0.60 | 0.25 | mid |

| 3 | 3.02 | 0.35 | 0.50 | mid |

| 4 | 4.08 | 0.20 | 0.75 | high |

---

## 1.1.2 FunctionalSampleSourceGenerator

`FunctionalSampleSourceGenerator` 不是把 sample 翻成"一堆 FFT 峰值"。它的设计目标是：

> 把单音样本压缩成一个**可编辑、可移调、可函数化控制的频谱生命体**。

也就是说，sample 进来之后，我们提取一个 x-t 分离的函数化模型：

$$
\log A_i(t) = \log A_i^{base} + \sum_m W_m^A(x_i)\,T_m^A(t) \quad\quad
\log \rho_i(t) = \log \rho_i^{base} + \sum_m W_m^F(x_i)\,T_m^F(t)
$$

其中

- $x_i \in [0,1]$ = partial 在频域的归一化位置（用 normalized log-ratio）
- $T_m^A(t)$ / $T_m^F(t)$ = **可解释的共享时间基函数**（attack burst、fast decay、body resonance、…）
- $W_m^A(x_i)$ / $W_m^F(x_i)$ = 每根 partial 对每个 basis 的响应强度
- $A_i^{base}$ / $\rho_i^{base}$ = 该 partial 的静态基准振幅和频率比

这意味着：vibrato 不再是每条 partial 各自估出来的乱飘相位，而是**一条 global 时间 basis** $T_m^F(t)$ 乘上每根 partial 的 `W_m^F(x_i)` 响应权重；attack 不再是每条 partial 各自的 `attackScale[i]`，而是**一条 AttackBurst basis** + 每根 partial 的权重。这就让宏控（"让 attack 更脆"、"让 brightness 衰减更快"、"让所有高泛音 vibrato 变深"）变成有意义的操作。

它属于 Generator 层，不属于 `Frequency Shape Function`。它输出 `FunctionalSpectralSource`（compact、可保存、可宏控、可移调）；下游 bake 阶段才把它展开成 `SpectralTimeline`。

### 1.1.2.1 整条管线（v0.3：v2 runtime 骨架已补齐）

```text

audio sample
  → SamplePreprocess           // DC remove, peak normalize, silence trim
  → RootEstimator              // HPS-style + parabolic; user override
  → STFT (medium, ~5 ms hop)
  → PeakCandidateExtractor     // local max + parabolic interp + magFloor
  → PartialTrackLinker         // greedy + cost = w_f·|Δlog f| + w_a·|Δlog A|
  → TrackCleaner / Classifier  // confidence, importance, HarmonicStable/Transient/…
  → FunctionalCompressor       // ridge-regression W_m^A / W_m^F on shared basis
  → FunctionalSpectralSource   // ≈22 KB; persistable; macro-editable
        │
        ↓ bakeToTimeline (curvature-aware frame placement + macro scaling + loudness equalizer)
        ↓
  SpectralTimeline (kMaxTimelineFrames=32, dense across attack region)
        │
        ↓ Voice residual lane (phase jitter) + transient lane (attack emphasis)

```

实现位置：`src/FunctionalSampleSource.cpp`。

### 1.1.2.2 三种数据层（不要混）

| 层 | 用途 | 持久化 | 内存 |
|---|---|---|---|
| `DensePartialTrack`(intermediate) | 链接出来的逐帧 partial 轨迹；峰、振幅、相位时间序列 | 不持久化 | 大，分析后丢弃 |
| `FunctionalSpectralSource` | 压缩后的 partial + basis bank + 权重；可保存进 preset；可宏控；可移调 | **是**，存进 preset | ≈22 KB |
| `SpectralTimeline` | 运行时主系统吃的东西；只看见 $\nu_i$, $a_i$, $x_i$, $\mu_i$ | bake 时重建 | ≈460 KB（不入 preset） |

主 renderer 只看见 `SpectralTimeline`，不直接看 `FunctionalSpectralSource`，更看不见 `DensePartialTrack`。

### 1.1.2.3 Track linking 成本函数

每一帧新峰候选 $(f_{new}, a_{new})$ 与活着的 track 的预测 $(f_{pred}, a_{pred})$ 匹配，cost：

$$
\text{cost} = w_f \cdot |\log(f_{new}/f_{pred})| + w_a \cdot |\log(a_{new}/a_{pred})|
$$

约束：

- $|\log(f_{new}/f_{pred})| <$ `maxFreqJumpCents`（默认 80 cents）
- $|\log(a_{new}/a_{pred})| <$ `maxAmpJumpDb`（默认 24 dB）

未匹配的峰 → 新 track（birth）。未匹配的 track 累计 gap，gap > `maxGapFrames`(默认 3) 即 death。

> 相位 / harmonic / Hungarian / Viterbi 全部留 v2。greedy 已经能解决 piano / 弦乐这类 partial 不剧烈交叉的样本。

### 1.1.2.4 时间 basis 设计

| Kind | 形式 | 默认 τ / σ | 默认绑定 macro |
|---|---|---|---|
| `GlobalEnvelope` | $\exp(-t/\tau)$ | duration / 2.5 | （无） |
| `AttackBurst` | $\exp(-(t-c)^2/2\sigma^2)$ | $c=0$, $\sigma=15$ ms | `attackSharpness` |
| `FastDecay` | $\exp(-t/\tau)$ | 60 ms | `brightnessDecay` |
| `SlowDecay` | $\exp(-t/\tau)$ | duration · 0.45 | `bodyResonance` |
| `BrightnessDecay` | $\exp(-t/\tau)$ | 180 ms (High) | `brightnessDecay` |
| `BodyResonance` | $\exp(-t/\tau)$ | duration · 1.2 (High) | `bodyResonance` |
| `PitchRelax` | $\exp(-t/\tau)$ | duration-aware, 40 ms–2.4 s | （frequency basis） |
| `VibratoSine` | $\sin(2\pi ft)\exp(-t/\tau)$ | High 默认 5 Hz | （frequency basis） |

basis 数量按 Quality 档位伸缩：

- amplitude basis：Draft=3、Standard=4、High=6
- frequency basis：Draft=1 (`PitchRelax`)、Standard=2 (`PitchRelax`×2)、High=3 (`PitchRelax`×2 + `VibratoSine`)

> 当前仍坚持“可解释 basis 优先”，所以 UI、preset 和后续 Functional Operator 都能按语义寻址。学习型 PCA / NMF basis 仍保留为后续增强，而不是 v2 runtime 的前置条件。

### 1.1.2.5 ridge regression（每根 partial 解一次）

对第 $i$ 根 partial：

1. $L_k = \log A_i(t_k)$（链接出来的逐帧 log 振幅）。
2. $\text{base}_i = \operatorname{median}(L_k)$，作为 `PartialStatic.baseLogAmp`。
3. 残差 $r_k = L_k - \text{base}_i$。
4. 基矩阵 $M_{m,k} = T_m(t_k)$。
5. 闭式解：

$$
w_i = (M M^T + \lambda I)^{-1} M r
$$

振幅和频率残差都走同一个 ridge + Cholesky 小矩阵求解器：

- 振幅：$r_k^A=\log A_i(t_k)-\text{baseLogAmp}_i$
- 频率：$r_k^F=\log(f_i(t_k)/f_i^{base})$

实现用一个 8×8 以内的 Cholesky 分解。$\lambda$ 按 track 长度反比缩放，避免短 track 过拟合。

### 1.1.2.6 Bake：adaptive frame 放置 + macro scaling

bake 阶段：

1. 在 `[0, horizon]` 上先建候选时间网格，再按 amplitude / frequency basis 的二阶差分曲率评分挑选帧点；attack 区域额外加权，尾部平坦段自动降采样。
2. horizon = `clamp(durationSeconds, 0.25, 8.0)` s，自动跟样本长度走，不再写死 2 s。
3. 每一帧 $f$ 在每根 partial $i$ 上重建：

$$
\log A_i(t_f) = \text{base}_i + \sum_m W_{i,m}^A \cdot \text{macro}(\text{kind}_m) \cdot T_m(t_f)
$$

其中 `macro(kind)` 是 `attackSharpness` / `brightnessDecay` / `bodyResonance` 按 basis kind 的路由。`Macro` 改了**不重新分析**，只重新 bake；分析结果 (`FunctionalSpectralSource`) 命中缓存。

4. 频率同时重建：

$$
\log \rho_i(t_f) = \log \rho_i^{base} + \sum_m W_{i,m}^F \cdot T_m^F(t_f)
$$

5. `PartialClass::HarmonicTransient / InharmonicTransient` 在 `transientAmount` 打开时获得受控 attack emphasis；`NoiseLike / Uncertain` 在 `residualAmount` 打开时映射到 Voice 的 phase-jitter residual lane。
6. partial 的 `birthTime` / `deathTime` 用作硬 gate。birth 后 2 ms 内做小淡入，避免 t=0 处的爆 pop。
7. 分析端会按“所选 partial 的归一化峰值能量”写入 `loudnessNormGain`，bake 时统一应用，避免 Draft / Standard / High 之间因 partial 数与 STFT 细节不同导致突然爆音。

### 1.1.2.7 移调

Partial 全部以 `ν_i = ρ_i = f_i / f_0` 的形式存。voice 拿到 MIDI note → 当前 f0 → 直接乘以 ν_i 得到实际 partial 频率。所以这个 generator **天然支持移调**：分析在 root=261 Hz，演奏 A2/C5 用同一份 FunctionalSpectralSource。

### 1.1.2.8 缓存策略

`GeneratorBank::runFunctionalSampleSource` 维护一份 cache：

- key = `(filePath, partialCount, quality, userLockRoot, userRootHz)`
- 命中 → 直接 `bakeToStaticFrame`（约毫秒级）
- 未命中 → 跑整条分析 pipeline → 写 cache → bake

宏控（attackSharpness / brightnessDecay / bodyResonance）以及 `transientAmount` / `residualAmount` **不**触发重新分析。

### 1.1.2.9 与 §1.1.2 (v0.2) 的差别

| 维度 | v0.2 `SamplePartialSetGenerator` | v0.3 `FunctionalSampleSourceGenerator` |
|---|---|---|
| partial 识别 | 每帧 peak pick 后单独跟一根 | 真正的 track linking（greedy + cost） |
| 振幅模型 | 每根 partial 独立估 `attackScale[i]` etc. | 共享 time basis + 每根 partial 权重 |
| 频率模型 | 一个 phaseDriftHz 标量 / partial | shared `freqBasis` + per-partial `freqWeights` |
| 编辑性 | 每个 partial 单独编辑，无宏义 | macro 旋钮 → basis 权重缩放 |
| Quality | 固定 | Draft / Standard / High，控制 partial 数 + basis 数 + hop |
| 缓存 | 无（每次重算） | 文件 / quality / root 不变时只 bake |

### 1.1.2.10 v2 runtime 补全状态

本轮已把原先的空架构接成可运行版本：

- **Frequency time basis**：`freqBasis` / `freqWeights` 已参与分析、压缩和 bake，当前默认覆盖 pitch relax 与 High 档 vibrato-like periodic drift。
- **Transient lane**：`transientAmount` 会针对 transient partial 做早期 attack emphasis。
- **Residual lane**：`residualAmount` 会把 NoiseLike / Uncertain partial 路由到 Voice 的 phase-jitter residual lane。
- **Adaptive frame placement**：timeline 帧点改成 basis curvature 评分，而不是固定 dense-early / sparse-late。
- **Quality loudness equalizer**：不同 STFT 档位与 partial 数量造成的整体响度漂移，在 compressed source 侧统一校正。

仍保留为后续研究项的，不再是“空骨架”，而是质量上限扩展：

- learned PCA / NMF residual basis
- 更精细的 phase residual model
- Functional Operators 的离线编辑器
- CQT / reassigned spectrogram

---

## 1.5 ResamplingEngine

`ResamplingEngine` 在 v0.3 中归入 `Creator / SampleCraft`。它不是全局 arranger，也不是主 mixer；它是 Creator 内部把 Seed 输出录下来、冻结成材料、再做 buffer craft 的模块。

```text

Creator
  ├── Seed outputs
  ├── Tone FX
  → SampleCraft / ResamplingEngine
      ├── Recorder
      ├── BufferSlot
      ├── SliceProcessor
      ├── GranularProcessor
      ├── LoopProcessor
      └── Processor
  → Creator Output
  → Mix FX / Mixer bus

```

### 1.5.1 三种角色

| 角色 | 行为 |
|---|---|
| Recorder | 录下前面所有模块已经处理完的 stereo 输出 |
| Buffer Processor | 对冻结或导入 buffer 做 pitch、slice、loop、reverse、granular、stutter |
| Resampling Materializer | 把系统内部的一次声音事件冻结成后续可再编辑的新材料 |

### 1.5.2 Recorder

Recorder 录制的信号位置固定为：

```text
after Seed Tone FX, before ResamplingEngine playback mix
```

所以它能捕捉：

- generator / timeline / operators / matrix / voices
- 全局 ADSR 与 performance mapping
- Seed 级 Tone FX 结果

录音开始时会清空当前录制槽；停止录音后 buffer 保留，供 playback / slicing / freezing 使用。

### 1.5.3 BufferSlot

`BufferSlot` 有两个来源：

1. 内部 Recorder 录下来的系统输出
2. 外部导入的音频文件

当前 v1 是一个 stereo slot：

- 可导入音频
- 可被冻结播放
- preset 保存参数与外部文件路径
- buffer PCM 本体不写入 preset

### 1.5.4 Processor

当前 v1 已落地：

| 处理 | 当前实现 |
|---|---|
| Pitch | `pitchSemitones` 改变播放速率 |
| Cut | `sliceStart` / `sliceEnd` 约束播放窗口 |
| Loop | `loopEnabled` 控制窗口循环 |
| Reverse | `reverse` 反向播放 |
| Slice Reorder | `sliceCount` + `sliceRotate` 做切片重排 |
| Granular | `granularAmount` + `grainSizeMs` 做邻域 grain tap 混合 |
| Stutter | `stutterAmount` + `stutterRateHz` 做周期性短片段重复 |

它们全部运行在 `ResamplingEngine::process()` 内，属于 audio buffer domain，不回写 `SpectralTimeline`。

### 1.5.5 Tone FX / Mix FX 的区分

两层效果可以共用同一个 `EffectsChain` 实现，但语义不同：

| 模块 | 插入位置 | 目的 |
|---|---|---|
| Tone FX | Seed / Creator 内部，Resampling 之前 | 塑造声音身份 |
| Mix FX | Resampling / Creator 输出之后，Mixer bus 或 Master 之前 | 平衡、空间、总线处理与最终输出 |

这样做的关键好处：

- 录音机捕捉的是“已经定型的一次内部声音”
- 后续 frozen material 又能拥有独立的第二层效果塑形
- 同一套 FX DSP 不重复发明，但“音色塑形”和“混音处理”的插入点不混淆

---

## 1.6 SamplePlaybackGenerator

`SamplePlaybackGenerator` 是普通采样器通路，不属于 partial spectral source：

```text

Keyboard / MIDI
  → SamplePlaybackEngine
      ├── imported sample buffer
      ├── note-to-pitch transposition
      ├── start / end trim
      ├── loop / reverse
      └── attack / release / gain
  → Tone FX
  → Creator SampleCraft / ResamplingEngine
  → Mix FX

```

### 1.6.1 与 FunctionalSampleSource 的区别

| 维度 | FunctionalSampleSource | SamplePlaybackGenerator |
|---|---|---|
| 核心表示 | partial tracks -> functional spectral model | 原始采样 buffer |
| 是否生成 `SpectralTimeline` | 是 | 否 |
| 是否走 Operator / Matrix partial 链 | 是 | 否 |
| 键盘控制 | 用音高驱动 partial ratio | 用音高改变采样播放速率 |
| 适合 | 可编辑频谱材料、宏控 timbre | 普通 one-shot / loop sampler |

### 1.6.2 当前 v1 参数

- `filePath`
- `rootMidi`
- `start01` / `end01`
- `playbackGain`
- `pitchOffsetSemitones`
- `loopEnabled`
- `reverse`
- `attackMs`
- `releaseMs`

UI 里它仍然挂在 Generator 面板，是因为它属于“声源选择”；但工程实现上它是 `SamplePlaybackEngine` 的独立 audio path，而不是 `GeneratorBank -> SpectralTimeline` 的一员。

---

## 1.7 Per-Note Parameter Lock Piano Window

这一层吸收 Octatrack 的 parameter lock 思路，但编辑对象不是 step trig，而是 **piano-roll 上的单个 note event**。

它不是 MPE：

- MPE 是连续演奏控制协议。
- 这里是离线 / 半实时创作编辑器。
- 每个 note event 可以携带一包独立参数覆盖，触发时注入对应 voice。

### 1.7.1 基本对象

```text

PianoLockWindow
  ├── NoteLane
  ├── NoteEvent
  │     ├── pitch / start / duration / velocity
  │     ├── sourceRef
  │     └── parameterLocks[]
  ├── LockInspector
  └── PartialLockEditor

```

当前 ABI 已落为：

```text
CompositionModel.h
  ├── ParameterScope
  ├── ParameterTargetDomain
  ├── LockValueMode / LockValueType
  ├── ParameterTarget
  ├── LockValue
  ├── ParameterLock
  └── PianoNoteEvent
```

### 1.7.2 哪些东西可以被 lock

对 partial-based generator：

- partial amp scale
- partial frequency ratio offset
- x / group-selective emphasis
- phase init override
- per-partial attack / decay / release scale
- FunctionalSampleSource 的 macro 值或 basis-targeted 权重偏移

对普通 sample playback：

- start / end
- reverse
- loop
- pitch offset
- gain
- attack / release

所以这里建议定义统一的：

```text
ParameterTarget =
  GlobalParam
  | SeedParam
  | CreatorParam
  | MotifParam
  | StructureParam
  | MixingParam
  | GeneratorParam
  | PartialParam
  | SamplePlaybackParam
  | FxParam
  | ResampleParam
```

再由不同 source type 决定哪些 target 有效。

### 1.7.3 Partial Lock 的语义

对一个 partial generator note：

```text
VoiceRenderState(note)
  = SourcePresetBase
  + TrackAutomation(t)
  + MotifAutomation(t)
  + NoteParameterLocks
  + LivePerformanceMapping
```

其中 `NoteParameterLocks` 优先级最高，适合做：

- 单个音突然亮一点
- 某个 note 的高频 partial 额外冲出
- 同一个 generator 在一个 pattern 内每个音都有微不同 timbre
- 同一 motive 的 repetition 做“同类但不重复”的局部变形

### 1.7.4 UI 目标

Piano window 不应只是普通 MIDI roll，而应显示：

- note block
- per-note lock badge
- 当前选中 note 的 lock inspector
- partial mini view：显示该 note 相对 base timbre 被改动的 partial 差异
- 快速复制 lock 到同音高、同拍位、同 motive group

这个窗口的核心价值不是“输入音符”，而是 **把音符变成声音设计的粒度单位**。

---

## 1.8 Seed / Creator / Motif / Structure 创作结构

v0.3 的创作结构改成严格五层：

```text

Seed
  ↓
Creator
  ↓
Motif
  ↓
Structure
  ↓
MixingArrangeView

```

当前 ABI 的新主干：

```text
CompositionProject
  ├── seedPresets[]
  ├── creatorPresets[]
  ├── motifClips[]
  ├── structure
  └── mixer
```

旧的 `MotifDefinition / PatternDefinition / ArrangeTrack / ArrangeRegion` 暂时保留为 v0.2 兼容字段，等 UI 完成迁移后再收敛。

### 1.8.1 Creator 代替万能 Generator

`Creator` 是 Seed 的可演奏容器：

```text
CreatorPreset
  ├── seedInstances[]
  ├── macroDefaults[]
  └── kind = MultiSeed / DrumRack / PianoGrid / SampleCraft
```

它可以是 drum rack、piano locks instrument、samplecraft collage 或多个 Seed 的组合。它不能变成完整 arrange，也不能直接承担全曲结构。

### 1.8.2 Motif 代替 Session View

这里的 `Motif` 不是传统 DAW clip 的简单替代，而是“乐思单位”：

- 一段音高型动机
- 一组 rhythm trigger
- 一片 sound-design variation
- 同一个主题在不同音色 / register / 处理方式下的变体

Motif 内部可以包含多类事件：

```text
Motif
  ├── CreatorInstance refs
  ├── PianoNoteEvents
  ├── Rhythm / Resample triggers
  └── LocalAutomation
```

这样“复制去改”的声音设计仍然属于同一个 motive family，不会把 arrange 轨道越堆越乱。

当前 v0.3 ABI：

```text
MotifClip
  ├── creatorRefs[]
  ├── noteEvents[]
  ├── lengthBeats
  └── localAutomation[]
```

### 1.8.3 Structure 是宏观形式层

`Structure` 把 Motif 当作 pattern / phrase / developed motif 来放置：

```text
StructureProject
  ├── events[]
  └── globalAutomation[]

StructureEvent
  ├── motifClipId
  ├── startBeats
  ├── lengthBeats
  └── transform[]
```

它可以做段落、timeline placement、motif development transform 和全局 automation，但不直接编辑 Seed / Creator 的内部定义。

### 1.8.4 MixingArrangeView 是最外层工程视图

最外层是大 arrange + mixer 视图：

```text

MixingArrangeView
  ├── Structure Tracks
  ├── Motif Bus Regions
  ├── Audio-like Frozen Regions
  ├── Automation Groups
  ├── Track Preview Renderer
  └── Mixer Lane Preview
```

当前 v0.3 mixer ABI：

```text
MixerState
  ├── seedChannels[]
  ├── creatorChannels[]
  ├── motifBuses[]
  └── masterAutomation[]
```

### 1.8.5 多视图预览

每条 arrange / mixer 轨可切换预览模式：

| 预览模式 | 用途 |
|---|---|
| Waveform | 看真实能量包络、cut、freeze、resample 结果 |
| Waterfall / Spectrogram | 看频带密度、噪声与 transient |
| Partial Preview | 看 spectral generator 的 partial 结构如何随时间变化 |

目标不是“一个超复杂总谱”，而是让创作时能够用最合适的视角看每条轨。

### 1.8.6 Mixer 借鉴 FL Studio / Pro Tools

可以学习 FL Studio 的 mixer lane 直观性：

- 左侧 route / bus
- 中央 track / lane
- 每个轨上有缩略动态预览
- arrange 中可直接看某个轨的内容强弱变化

但本系统不应只复制传统 mixer，因为这里有三种材料：

1. partial spectral sources
2. ordinary samples
3. resampled / frozen materials

所以 track preview 需要是可切换类型，而不是固定 waveform。

---

## 1.9 Automation Group Matrix

你提到的问题是核心问题：

> 单个 Seed / Creator 的自动化、Structure 自动化、note 级 parameter lock，怎么不互相打架？

建议用类似 Ableton Arrange View 下方 lane/group 的方式，但更显式地做成：

```text

AutomationGroupMatrix
  ├── Global Automation
  ├── Seed Automation
  ├── Creator Automation
  ├── Motif Automation
  ├── Structure Automation
  ├── Mixing Automation
  ├── Note Parameter Locks
  └── Conflict Resolver / Priority Rules

```

当前 ABI：

```text
AutomationGroup
  ├── scope
  └── locks[]

AutomationGroupMatrix
  └── groups[]
```

### 1.9.1 建议的优先级

从低到高：

```text
Base Preset
< Seed / Creator Automation
< Motif Automation
< Structure Automation
< Mixing Automation
< Note Parameter Locks
< Live Performance / Temporary Override
```

### 1.9.2 为什么需要 Group Matrix

因为同一个参数可能会同时被：

- generator 自己的 macro 控制
- Seed / Creator 曲线推动
- motive 局部变奏
- 单个 note lock 覆盖

没有 group matrix，用户会不知道“当前这个 timbre 是谁改出来的”。

所以 matrix 至少要能回答：

- 哪一层正在写这个参数
- 哪一层最终获胜
- 当前参数值由哪些层叠加而来
- 这个 automation 是 local、motif-shared、structure-wide，还是 mix-stage

### 1.9.3 推荐的冲突规则

对连续参数：

- 默认：additive / multiplicative 合成，取决于参数语义
- 可切换：override

对离散参数：

- 默认：highest-priority override

对 per-partial parameter：

- base partial model
- partial-mask automation
- note partial lock

三者叠加，而不是互相覆盖全部数组。

---

## 1.10 Screen Density / One-Screen Composition Principle

你的一个很重要的审美要求是：

> 尽量让全部内容同时出现在屏幕中。

这个方向我认同，但它带来一个结构约束：

- arrange 主画布必须是中心
- mixer / automation / piano-lock 不应都占独立全屏
- 更适合是：
  - 中央 arrange
  - 下方 contextual editor dock
  - 右侧 inspector
  - 左侧 compact browser / source tree

建议目标布局：

```text

Top: transport + global mode
Left: source / motif browser
Center: arrange + track previews
Right: inspector / selected automation target
Bottom: switchable editor dock
  - Piano Lock
  - Automation Matrix
  - Resampling Detail
  - Source Detail

```

这样“全局同时在场”与“局部深编辑”不会互相伤害。

### 1.10.1 当前实现推进顺序

已经按下面顺序开始落地：

1. `PianoNoteEvent + ParameterLock`
2. `ParameterTarget / LockValue / Scope`
3. `Motif / Pattern / Arrange / AutomationGroupMatrix`
4. Preset 的 `composition` 序列化入口

下一步 UI 和调度器都会建立在这层 ABI 上继续推进。

### 1.10.2 当前 UI v1 已落地

已经新增一个可见的 `Piano Locks` 编辑页：

- piano grid 上显示 note block
- note 上有 parameter-lock badge
- 点击 note 可选中
- 右侧 inspector 可编辑：
  - velocity
  - pitch lock
  - gain lock
- 提供 `Seed Demo Notes`，便于验证 composition project / preset 通道

这版 UI 的目标是先把“note -> lock -> project writeback”跑通。后续再接：

- 拖拽新建 note
- note resize / move
- partial mini diff view
- 更多 target domain
- 与真正 arrange scheduler 的播放联动

---

## 1A.1 Frequency Shape Function

这一节属于 `Generator Preprocessor`，不是 `DirectPartialGenerator` 专属能力。

它不是直接替换 generator 产生的频率结构，而是作为后处理偏移作用在 base frequency descriptor 上。

先定义一个 shape descriptor：

$$

\nu_i^{shape}

$$

然后作用到 generator base seed：

$$

\nu_i^{src}

=

\nu_i^{base}

\frac{\nu_i^{shape}}{n_i}

$$

其中 $n_i$ 是第 $i$ 个 partial 的默认 harmonic index。

这意味着：

```text

DirectPartialGenerator:
  nu_i^base = n_i
  所以 nu_i^src = nu_i^shape

Modal/PDE/FunctionalSampleSource:
  nu_i^base 来自 modal table 或 functional spectral source 里的 baseRatio
  Preprocessor 只给它乘上一个 shape offset

```

如果使用 relative ratio：

$$

f_i^{src}(t)=f_0(t)\rho_i(t)

$$

常见 shape descriptor：

### Harmonic

$$

\rho_i=n_i^\alpha

$$

当：

$$

\alpha=1

$$

时：

$$

\rho_i=n_i

$$

这是普通谐波结构。

### Linear

$$

\rho_i=1+k n_i

$$

### Exponential

$$

\rho_i=\exp(k n_i)

$$

其中：

$$

n_i=1+x_i(N-1)

$$

## 1A.2 Amplitude Distribution Function

这一节属于 `Generator Preprocessor`，不是 `DirectPartialGenerator` 专属能力。

不同 generator 可以先给出 source-specific amplitude seed，然后由这里的通用分布函数统一施加 tilt、focus、random、group pattern、normalization。

它作为后处理乘法作用在 generator base amplitude 上：

$$

a_i^{src}

=

a_i^{base}

\exp(L_i^{shape})

$$

最后再做归一化。

先定义 raw log shape：

$$

L_i^{shape}

=

L_i^{base}

- 

a_w\Phi_{mode}(x_i)

- 

a_r\epsilon_i

- 

\lambda x_i^\gamma

- 

d_g\cos

\left(

2\pi M(x_i+o_g)

\right)

- 

a_f

\exp

\left(

- 

\frac{(x_i-c_f)^2}{2\sigma_f^2}

\right)

$$

其中：

- $a_i^{base}$：generator 产生的原始振幅，例如 modal pickup weight、sample peak magnitude、或 Direct seed 的 1。
- $\Phi_{mode}(x_i)$：全局形状函数。
- $a_w$：全局形状强度。
- $a_r\epsilon_i$：随机扰动。
- $-\lambda x_i^\gamma$：谱倾斜 / 高频衰减。
- $d_g\cos(2\pi M(x_i+o_g))$：group pattern。
- $a_f$：formant / focus 强度。
- $c_f$：focus center。
- $\sigma_f$：focus width。

裁剪：

$$

L_i

=

\operatorname{clip}

\left(

s_L L_i^{raw},

0,

1

\right)

$$

转成 amplitude：

$$

a_i^{src}

=

\exp(L_i)

$$

或者归一化：

$$

a_i^{src}

=

\frac{

\exp(L_i)

}{

\sum_{j=1}^{N}\exp(L_j)+\varepsilon

}

$$

---

## 1A.3 Shape Modes

Shape Modes 是 `Generator Preprocessor` 和 `Matrix Weight Function` 共享的 shape library。

它们不属于任何单一 generator。

### Tilt

$$

\Phi_{tilt}(x;p_t)

=

2

\left(

\frac{x^{p_t}}{x^{p_t}+(1-x)^{p_t}}

\right)

- 

1

$$

### Symmetric

$$

\Phi_{sym}(x;c_s,p_s)

=

\operatorname{clip}

\left(

1-2|x-c_s|^{p_s},

-1,

1

\right)

$$

### Skew

$$

\Phi_{skew}(x;p_k)

=

4x(1-x)

\operatorname{sgn}(2x-1)

|2x-1|^{p_k}

$$

这些 shape function 可以用于：

$$

a_i^{src}(t)

$$

也可以用于 Matrix weight：

$$

W_k(x_i,\mu_i)

$$

---

## 1A.4 Phase Init Function

Phase Init Function 属于 `Generator Preprocessor` 输出的 voice 初始化规则。

它不把完整 source phase trajectory 写入 `P_i^{src}`，只提供 runtime voice 在 note-on 时如何初始化 $\theta_i$ 的规则。

一般 generator 不输出持续 source phase；`FunctionalSampleSourceGenerator` 可以输出 sample-derived locked phase 作为 note-on init metadata。

Preprocessor 只定义 phase initialization rule：

$$

\Phi_i^{init}

=

\Phi^{init}

\left(

i,

x_i,

\mu_i,

note,

velocity,

seed

\right)

$$

常见模式：

### Zero Phase

$$

\Phi_i^{init}=0

$$

### Random Phase

$$

\Phi_i^{init}\sim U(0,2\pi)

$$

### Locked Phase

$$

\Phi_i^{init}=\phi_i^{table}

$$

### Alternating Phase

$$

\Phi_i^{init}=\pi(i \bmod 2)

$$

第一版建议使用：

```text

Zero Phase

Random Phase

Locked Phase

```

不要先做复杂 phase modulation。

---

## 1.2 ModalODEGenerator

`ModalODEGenerator` 不是第一版必做核心，但架构兼容。

### 1.2.1 ODE 内部方程

对于第 $i$ 个模态：

$$

\ddot q_i(t)

- 

2\zeta_i\omega_i\dot q_i(t)

- 

\omega_i^2q_i(t)

=

input_i(t)

$$

其中：

- $q_i(t)$：第 $i$ 个模态的位移。
- $\dot q_i(t)$：第 $i$ 个模态的速度。
- $\omega_i$：角频率。
- $\zeta_i$：阻尼系数。
- $input_i(t)$：外部激励。

状态空间形式：

$$

z_i(t)

=

\begin{bmatrix}

q_i(t) 

\dot q_i(t)

\end{bmatrix}

$$

$$

\dot z_i(t)

=

A_i z_i(t)+B_i input_i(t)

$$

$$

A_i

=

\begin{bmatrix}

0 & 1 

-\omega_i^2 & -2\zeta_i\omega_i

\end{bmatrix}

$$

$$

B_i

=

\begin{bmatrix}

0 

1

\end{bmatrix}

$$

---

### 1.2.2 ODE 到 SpectralTimeline 的转换

ODE 内部产生：

$$

q_i(t),\dot q_i(t)

$$

但这些不直接进入主系统。

第一版建议不要使用 ODE 的 instantaneous phase。

只把 ODE 压缩成 frequency descriptor 和 amplitude trajectory：

$$

f_i^{src}(t)=\frac{\omega_i}{2\pi}

$$

如果有明确基频 $f_0(t)$，则：

$$

\nu_i^{src}(t)=\rho_i(t)=\frac{f_i^{src}(t)}{f_0(t)}

$$

如果没有明确基频，则：

$$

\nu_i^{src}(t)=f_i^{src}(t)

$$

并设置：

```text

freqMode = AbsoluteHz

```

amplitude 可以由能量估计得到：

$$

a_i^{src}(t)

=

g_i

\sqrt{

q_i^2(t)

- 

\left(

\frac{\dot q_i(t)}{\omega_i}

\right)^2

}

$$

最终输出：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

整体路径：

```text

Modal ODE

→ q_i, qdot_i

→ frequency descriptor + amplitude trajectory

→ SpectralTimeline

```

---

## 1.3 PDEModalGenerator

PDE 不直接进入 renderer。

PDE 先做模态分解：

$$

y_{field}(x,t)

=

\sum_{i=1}^{N}

q_i(t)\Phi_i(x)

$$

其中：

- $y_{field}(x,t)$：连续介质的位移场。
- $\Phi_i(x)$：第 $i$ 个空间模态形状。
- $q_i(t)$：第 $i$ 个模态的时间系数。

代入 PDE 后，每个 $q_i(t)$ 通常变成类似 ODE 的模态方程：

$$

\ddot q_i(t)

- 

2\zeta_i\omega_i\dot q_i(t)

- 

\omega_i^2q_i(t)

=

F_i(t)

$$

如果存在拾音点 $x_o$，则该模态的拾音权重为：

$$

G_i=\Phi_i(x_o)

$$

转换为 partial amplitude：

$$

a_i^{src}(t)

=

G_i

\sqrt{

q_i^2(t)

- 

\left(

\frac{\dot q_i(t)}{\omega_i}

\right)^2

}

$$

频率 descriptor：

$$

f_i^{src}(t)=\frac{\omega_i}{2\pi}

$$

如果有基频：

$$

\nu_i^{src}(t)=\rho_i(t)=\frac{f_i^{src}(t)}{f_0(t)}

$$

如果没有基频：

$$

\nu_i^{src}(t)=f_i^{src}(t)

$$

并设置：

```text

freqMode = AbsoluteHz

```

最终：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

路径：

```text

PDE

→ Modal Decomposition

→ Modal ODEs

→ SpectralTimeline

```

第一版不要做实时 PDE 求解。

第一版如果需要 PDE 风格，只使用预计算 modal frequency table。

---

# 2. SpectralTimeline

`SpectralTimeline` 是整个系统的统一中心。

它不关心 partial 来自哪里。

可能来自：

```text

DirectPartialGenerator

ModalODEGenerator

PDEModalGenerator

HarmonicGenerator

InharmonicGenerator

FunctionalSampleSourceGenerator

```

但最后都必须输出：

$$

P_i^{src}(t)

=

\left(

\nu_i^{src}(t),

a_i^{src}(t),

x_i,

\mu_i

\right)

$$

整体：

$$

SpectralTimeline(t)

=

\left

P_i^{src}(t)

\right_{i=1}^{N}

$$

离散形式：

$$

P_i^{src}[k]

=

\left(

\nu_i^{src}[k],

a_i^{src}[k],

x_i,

\mu_i

\right)

$$

其中：

- $k$：spectral frame index。
- audio-rate 不直接运行复杂 generator。
- renderer 只在 frame 之间插值。

---

## 2.1 SpectralTimeline 实现

当前实现已经从单个 `StaticSpectralFrame` 升级为真正的 `SpectralTimeline`：

```text

SpectralTimeline
  frameCount
  durationSeconds
  loop
  timeSeconds[k]
  frames[k] = StaticSpectralFrame

```

每个 frame 仍然使用同一组 partial 字段：

$$

P_i^{src}

=

\left(

\nu_i^{src},

a_i^{src},

x_i,

\mu_i

\right)

$$

但它们现在按 source-intrinsic time 排列成一条轨迹。

Audio thread 不运行 generator，而是：

```text

voice note-on
  → seed from SpectralTimeline(0)

each control block
  → t_voice = voice age seconds
  → sample SpectralTimeline(t_voice)
  → interpolate between frames
  → Matrix / ADSR / render

```

静态 generator 会被提升成 1-frame timeline。

`FunctionalSampleSourceGenerator` 会把 functional spectral source（base + basis + weights）展开成多帧 source trajectory。
这让单音样本不只是“点位 preset”，而是可以带有自己的 source-intrinsic amplitude / phase motion。

---

## 2.2 Source / Working Source / Operator Editing Model

`Source` 是原始声音结构。

`Working Source` 是当前编辑中的临时副本。

`Operator` 是对 Working Source 写入规则、写入参数、烘焙结果的处理器。

整体流程：

```text

Source

→ Working Source

→ Operator

→ Edited Working Source

```

如果用户满意当前结果，可以保存当前参数状态：

```text

Save Source State

```

然后：

```text

Edited Working Source

→ Source

```

---

## 2.2.1 Source

`Source` 保存一个 generator 或 spectral model 的原始状态：

$$

Source

=

\left(

GeneratorType,

GeneratorParams,

SpectralTimeline,

OperatorChain,

Metadata

\right)

$$

其中：

- `GeneratorType`：source 类型。
- `GeneratorParams`：生成器参数。
- `SpectralTimeline`：当前生成出的 partial timeline。
- `OperatorChain`：非破坏性 operator 链。
- `Metadata`：source name、material、group、version 等信息。

---

## 2.2.2 Working Source

`Working Source` 是 Source 的可编辑副本。

它用于临时操作，不立即覆盖原始 Source：

$$

WorkingSource

=

Copy(Source)

$$

用户在 operator 里做的修改先作用于 Working Source：

$$

Operator(WorkingSource)

\rightarrow

WorkingSource'

$$

---

## 2.2.3 Operator

Operator 是对 Working Source 的处理函数。

一般形式：

$$

\mathcal O_j:

SpectralTimeline

\rightarrow

SpectralTimeline'

$$

也可以写成：

$$

\mathcal O_j

\left(

P_i^{src}(t),

\Theta_j

\right)

\rightarrow

P_i'(t)

$$

其中：

- $\mathcal O_j$：第 $j$ 个 operator。
- $\Theta_j$：operator 参数。
- $P_i^{src}(t)$：输入 partial track。
- $P_i'(t)$：处理后的 partial track。

Operator 可以处理：

```text

write frequency shape parameters

write amplitude distribution parameters

write phase init rule

write partial grouping

write partial mask

write randomization seed

write spectral tilt parameter

write formant focus parameter

write LFO assignment rule

write ADSR assignment rule

bake result to SpectralTimeline

```

严格边界：

```text

Operator 只能写入规则、写入参数、烘焙结果。

Matrix 才实时执行 modulation。

```

例如：

```text

Operator 可以写入：high partial group。

Matrix 负责实时用 LFO 调制 high partial group。

```

---

## 2.2.4 Save Source State

当用户希望保存当前 Working Source 时：

$$

Source

\leftarrow

WorkingSource'

$$

可以区分：

- `Save`：保存当前 generator 参数、OperatorChain、Matrix、FX、Composition state。
- `OperatorChain` 保持非破坏性，不再提供用户级 Bake / Unbake。
- 真正写入 audio buffer 的冻结只发生在 `SampleCraft / ResamplingEngine` 或 stem render。

例如：

$$

SourceState_{new}
=
SourceParams + OperatorChain + PerformanceState

$$

`FunctionalSampleSource` 内部仍然会把 compact model 展开到 `SpectralTimeline`，但这是 generator runtime 展开步骤，不是 Operator 面板的冻结工作流。

---

# 3. Matrix / Performance Mapping

`Matrix / Performance Mapping` 接收：

$$

P_i^{src}(t)

$$

然后输出：

$$

P_i^{final}(t)

$$

也就是：

$$

P_i^{src}(t)

\rightarrow

P_i^{final}(t)

$$

Matrix 发生在演奏时。

它不改变 Source 本体，除非用户主动 Bake / Commit。

---

## 3.1 Frequency Resolve

Matrix 之前或 Matrix 内部需要把 frequency descriptor 解析成 Hz。

如果：

```text

freqMode = RelativeRatio

```

则：

$$

f_i^{src}(t)=f_0(t)\nu_i^{src}(t)

$$

如果：

```text

freqMode = AbsoluteHz

```

则：

$$

f_i^{src}(t)=\nu_i^{src}(t)

$$

---

## 3.2 振幅映射

$$

a_i^{final}(t)

=

a_i^{src}(t)

E_i(t)

M_i^{amp}(t)

$$

其中：

- $a_i^{src}(t)$：generator 自身产生的振幅轨迹。
- $E_i(t)$：performance envelope，例如 ADSR。
- $M_i^{amp}(t)$：matrix 产生的 partial-specific amplitude modulation。

---

## 3.3 频率映射

$$

f_i^{final}(t)

=

f_i^{src}(t)

M_i^{freq}(t)

$$

其中：

- $f_i^{src}(t)$：source frequency in Hz。
- $M_i^{freq}(t)$：matrix 产生的 frequency modulation。

如果使用 ratio modulation，也可以写成：

$$

\nu_i^{final}(t)

=

\nu_i^{src}(t)

M_i^{ratio}(t)

$$

然后再 resolve 成 Hz。

---

## 3.4 Phase Offset Modulation

source 不提供持续运行的 audio-rate phase trajectory。

某些 generator 可以提供 note-on phase init metadata，例如 `FunctionalSampleSourceGenerator` 的 locked phase table。
这些只决定 voice 启动时的相位初值；演奏中的额外相位变化仍由 Matrix phase offset modulation 负责。

Matrix 只可以提供额外 phase offset modulation：

$$

\Delta\phi_i(t)

=

M_i^{phase}(t)

$$

第一版建议：

$$

\Delta\phi_i(t)=0

$$

等主架构稳定后再加入 phase modulation。

---

## 3.5 Envelope Shape Library

Performance envelope 不应该只有固定线性 ADSR。

可以定义可塑形 envelope：

$$

E_i(t)

=

\begin{cases}

A_i^{env}(t), & 0\leq t<T_A 

D_i^{env}(t), & T_A\leq t<T_A+T_D 

S_i, & T_A+T_D\leq t<T_{off} 

R_i^{env}(t), & T_{off}\leq t<T_{off}+T_R 

0, & t\geq T_{off}+T_R

\end{cases}

$$

归一化时间：

$$

\tau_A=\frac{t}{T_A}

$$

$$

\tau_D=\frac{t-T_A}{T_D}

$$

$$

\tau_R=\frac{t-T_{off}}{T_R}

$$

其中：

$$

0\leq \tau_A,\tau_D,\tau_R\leq 1

$$

定义通用曲线函数：

$$

\widetilde F(\tau;mode,\eta)

$$

### Exp

$$

\widetilde F_{exp}(\tau;\eta)

=

\frac{1-e^{-\eta\tau}}{1-e^{-\eta}}

$$

### Power

$$

\widetilde F_{pow}(\tau;\eta)

=

\tau^\eta

$$

### Sigmoid

$$

\sigma(x)

=

\frac{1}{1+e^{-x}}

$$

$$

\widetilde F_{sig}(\tau;\eta)

=

\frac{

\sigma(\eta(\tau-\frac{1}{2}))-\sigma(-\frac{\eta}{2})

}{

\sigma(\frac{\eta}{2})-\sigma(-\frac{\eta}{2})

}

$$

于是：

$$

A_i^{env}(t)

=

\widetilde F(\tau_A;mode_A,\eta_A)

$$

$$

D_i^{env}(t)

=

S_i

- 

(1-S_i)

\left[

1-\widetilde F(\tau_D;mode_D,\eta_D)

\right]

$$

$$

R_i^{env}(t)

=

S_i

\left[

1-\widetilde F(\tau_R;mode_R,\eta_R)

\right]

$$

如果使用全局 ADSR：

$$

E_i(t)=E(t)

$$

如果使用 partial-specific ADSR：

$$

E_i(t)\neq E_j(t)

$$

第一版建议使用全局 ADSR。

第二版再考虑 partial-specific ADSR。

---

## 3.6 Matrix Rule

每条 matrix rule 定义为：

$$

R_k

=

\left(

m_k(t),

d_k,

\tau_k,

\alpha_k,

T_k,

W_k

\right)

$$

其中：

- $m_k(t)$：modulation source，例如 LFO、envelope、gesture、random、MIDI。
- `Shape` 例外：它不是时间函数，而是 partial-axis 静态函数 $m_k(i)$。
- $d_k$：destination，例如 amp、freq、ratio、phase、decay。
- $\tau_k$：smoothing time / delay / response time。
- $\alpha_k$：modulation depth。
- $T_k$：transfer function / curve。
- $W_k(x_i,\mu_i)$：partial weighting function。

第一版实现的 modulation source：

```text

None
LFO1..LFO8
Velocity
KeyTrack
Random
ADSR
GeneratorSelf
Chaos
Shape

```

`GeneratorSelf` 表示 generator + frequency shape / preprocess 之后的 partial 自身位置。
它把当前 partial 的 `nu_i` 在整组 partial 的 log-frequency 范围里归一化到 `[-1, 1]`，所以可以做类似 FM / AM 的自映射：

```text

low partial  -> -1
mid partial  ->  0
high partial -> +1

```

`Chaos` 是全局 chaos modulation source，可以选择 noise type，并用 `frequencyHz` 控制更新速率，用 `amount` 控制输出强度。
第一版实现包含：

```text

White
Smooth
Crackle

```

它和 LFO 一样先在 MatrixEngine 的 control state 中更新，再被每条 rule 映射到 amp / freq / phase / decay。

`Shape` 是非时间调制源。它把 partial 数量当成一条静态时间轴，对单个 partial 的值进行调制：

```text
axis_i = i / (N - 1)       // 默认：按 partial index
axis_i = x_i               // 可选：按 spectral x_i
m_i = Shape(axis_i + phase)
```

Shape 使用与 LFO 相同的 curve library，但没有 frequency，不随时间 tick：

```text
Asymm / Sine / Square / Tri / SampleHold
phase / rho / p_up / p_down
```

因此它适合做：

- 第 1 到第 N 根 partial 的静态 amp 梯度
- 非时间型 frequency bend / inharmonic spread
- 每隔几根 partial 的 phase offset
- 用 `x_i` 轴做 spectral-zone emphasis

### Amplitude Modulation

$$

M_i^{amp}(t)

=

1+

\alpha_k

T_k(m_k(t))

W_k(x_i,\mu_i)

$$

$$

a_i^{final}(t)

=

a_i^{src}(t)

E_i(t)

M_i^{amp}(t)

$$

### Frequency Modulation

$$

M_i^{freq}(t)

=

1+

\beta_k

T_k(m_k(t))

W_k(x_i,\mu_i)

$$

$$

f_i^{final}(t)

=

f_i^{src}(t)

M_i^{freq}(t)

$$

### Phase Offset Modulation

$$

M_i^{phase}(t)

=

\gamma_k

T_k(m_k(t))

W_k(x_i,\mu_i)

$$

$$

\Delta\phi_i(t)

=

M_i^{phase}(t)

$$

---

## 3.7 Partial Weight Function

Matrix 的关键是：

$$

W_k(x_i,\mu_i)

$$

它决定某个 modulation source 作用到哪些 partial。

例如，只影响高频位置：

$$

W_{high}(x_i)

=

\mathbf{1}_{x_i>\eta}

$$

其中：

$$

0<\eta<1

$$

也可以基于倍率 $\rho_i$：

$$

W_{ratio}(\rho_i)

=

\mathbf{1}_{\rho_i>\rho_c}

$$

区别：

- $x_i$：用于归一化控制位置。
- $\rho_i$：用于真实频率倍率条件。

不要把两者混成一个变量。

---

## 3.8 LFO Shape Library

LFO 可以作为 Matrix 的 modulation source：

$$

m_k(t)=LFO(t)

$$

基础相位：

$$

\xi(t)

=

\operatorname{frac}

\left(

f_{LFO}t+\phi_0

\right)

$$

其中：

$$

\xi(t)\in[0,1)

$$

定义非对称 LFO shape：

$$

M(t;f_{LFO},\phi_0,\rho_{lfo},p_u,p_d)

=

\begin{cases}

-1+2\left(\frac{\xi(t)}{\rho_{lfo}}\right)^{p_u},

& 0\leq \xi(t)<\rho_{lfo} 

1-2\left(\frac{\xi(t)-\rho_{lfo}}{1-\rho_{lfo}}\right)^{p_d},

& \rho_{lfo}\leq \xi(t)<1

\end{cases}

$$

其中：

- $f_{LFO}$：LFO frequency。
- $\phi_0$：initial phase。
- $\rho_{lfo}$：上升段比例。
- $p_u$：上升曲线形状。
- $p_d$：下降曲线形状。

注意：这里用 $\rho_{lfo}$，不要和 partial frequency ratio $\rho_i$ 混淆。

输出范围大约为：

$$

M(t)\in[-1,1]

$$

---

# 4. Voice / Note Runtime

这是初步产品必须补上的层。

`Voice` 是一个正在发声的音符实例。

每个 Voice 持有自己的 runtime phase、envelope state、note state。

$$

Voice

=

\left(

SourceInstance,

NoteState,

RuntimePartials,

EnvelopeState

\right)

$$

其中：

```text

noteNumber

velocity

f0

noteOnTime

noteOffTime

voiceState

phaseAccumulatorArray

currentAmpArray

currentFreqArray

```

voiceState 可以是：

```text

Active

Released

Dead

```

MIDI note 到基频：

$$

f_0

=

440\cdot 2^{\frac{m-69}{12}}

$$

其中：

- $m$：MIDI note number。
- $m=69$ 对应 A4 = 440 Hz。

## 4.1 Unison

Unison 是一层 per-voice 微偏音叠加。每个 Voice 内部会复制 `U` 份子声部，对它们做 detune / pan / phase scatter，求和后再交给 final effects chain。

```text

UnisonParams = {
  voices       : 1..kMaxUnison      // U
  detuneCents  : 0..50 cents        // 最外两个子声部之间的总宽
  widthStereo  : 0..1               // 立体声铺开程度
  phaseSpread  : 0..1               // 子声部初相散布幅度
  phaseSeed    : uint32_t           // 决定每个子声部相位散布
}

```

其中 `kMaxUnison = 16`。Unison 与 Operator / Matrix 互不冲突：它发生在 Voice render 阶段，不修改 `P_i^{src}` 或 `SpectralTimeline`。

每个子声部 $u\in[0,U)$ 有：

$$
\begin{aligned}
t_u &= \frac{u}{U-1}\cdot 2 - 1 \in [-1, +1] \\
\text{detune}_u &= 2^{\,t_u \cdot \text{detuneCents}/2400} \\
\text{pan}_u &= t_u \cdot \text{widthStereo} \\
g^L_u &= \cos\!\left(\tfrac{\pi}{4}(1+\text{pan}_u)\right) \\
g^R_u &= \sin\!\left(\tfrac{\pi}{4}(1+\text{pan}_u)\right) \\
\phi^{init}_{u,i} &=
  \begin{cases}
    \phi^{init}_{i}                                   & u = 0 \ \text{(anchor)} \\
    \phi^{init}_{i} + s_U\cdot\xi_{u,i},\ \xi_{u,i}\sim U(-\pi,\pi) & u \geq 1
  \end{cases} \\
s_U &= \begin{cases} 0 & U = 1 \\ \text{phaseSpread} & U > 1 \end{cases} \\
\theta_{u,i}[n+1] &= \theta_{u,i}[n] + 2\pi\,\frac{f_i^{final}[n]\cdot \text{detune}_u + \phi'_i[n]}{F_s}
\end{aligned}
$$

注意两个要点：

1. **`u = 0` 永远是 anchor 子声部**，使用 `PhaseInitMode` 算出的基准相位 $\phi^{init}_{i}$，不加任何散布偏移。
2. **`U = 1` 时 spread 因子被强制为 0**，所以 `phaseSpread` 旋钮在单声部模式下完全无声学效果 —— 这是 unison 的语义本来就要求的：phase spread 是子声部之间的相位差，单声部根本没有"子声部之间"可言。这两条规则共同确保 unison voice 数量调到 1 时，调 `phaseSpread` 不会改变音色。

最终 stereo voice 输出：

$$
\begin{aligned}
y^L_{voice}[n] &= \tanh\!\left(G_{voice}\cdot G_U\sum_{i=1}^{N}\sum_{u=0}^{U-1} a_i^{final}[n]\,g^L_u\,\cos(\theta_{u,i}[n] + \phi^{init}_{u,i} + \Delta\phi_i[n])\right) \\
y^R_{voice}[n] &= \tanh\!\left(G_{voice}\cdot G_U\sum_{i=1}^{N}\sum_{u=0}^{U-1} a_i^{final}[n]\,g^R_u\,\cos(\theta_{u,i}[n] + \phi^{init}_{u,i} + \Delta\phi_i[n])\right)
\end{aligned}
$$

其中 $G_U = \tfrac{1}{\sqrt{U}}$ 用于保持感知响度近似不变。

实现要点：

- Voice 内 `theta_`, `phaseInit_` 维度从 `[N]` 升到 `[U][N]`；其余 envelope / control-rate 数组保持共享。
- Unison 参数挂在 `SourceGenParams.unison` 里，与 Generator 一起 publish 给 audio thread 的 RenderSnapshot。
- 每个 control block 重新计算 detune / pan 表，允许 `voices` 在按住音符时实时修改（已经存在的 sub-voice 的相位状态保持继续累积，新加入的 sub-voice 从 `phaseInit_` 重启）。
- Unison 不会增加 partial 数，因此和 partial-count audible cap（§1.0）独立。

---

# 5. RenderState

经过 Matrix / Performance Mapping 后，得到：

$$

RenderState(t)

=

\left

\left(

f_i^{final}(t),

a_i^{final}(t),

\Delta\phi_i(t)

\right)

\right_{i=1}^{N}

$$

真正 audio-rate 渲染时，使用 runtime phase accumulator：

$$

\theta_i[n]

$$

其更新方式为：

$$

\theta_i[n+1]

=

\theta_i[n]

- 

2\pi

\frac{

f_i^{final}[n]

}{

F_s

}

$$

最终输出：

$$

y_{voice}[n]

=

G_{voice}

\sum_{i=1}^{N}

a_i^{final}[n]

\cos

\left(

\theta_i[n]

- 

\Delta\phi_i[n]

\right)

$$

所有 voices 相加：

$$

y[n]

=

\sum_{v=1}^{V}

y_{voice,v}[n]

$$

---

# 6. Final Effects Chain

Final Effects Chain 位于所有 voice mix 之后：

```text

Voices
→ mix bus
→ EQ
→ Filter
→ output

```

它不改变 `SpectralTimeline` 本体，也不改变 Matrix / Voice 的 partial 结构。
它只处理最终 stereo audio bus。

当前链路只包含：

```text

EQ
Filter

```

每个 effect 都有同一套 process mode：

```text

Normal
Linear
Nonlinear

```

- `Normal`：默认音乐化处理，保持稳定和轻微 smoothing。
- `Linear`：只做线性 gain / filter 变换，不加入 saturation。
- `Nonlinear`：在线性处理基础上加入 drive / tanh saturation。

## 6.1 EQ

EQ 是最终 bus 上的三段 tone shaper：

```text

lowGainDb
midGainDb
highGainDb
drive
mode

```

## 6.2 Filter

Filter 是最终 bus 上的 tone filter：

```text

type = LowPass / HighPass / BandPass
cutoffHz
resonance
drive
mode

```

---

# 7. Control-rate / Audio-rate 分离

复杂计算不要在 audio-rate 做。

应当分为两层：

```text

Generator / Operator / Matrix:

control-rate / spectral frame-rate

Renderer:

audio-rate

```

## 7.1 Audio-safe Render Snapshot

GUI / editor thread 可以修改 generator、operator、preset、sample analysis、matrix rule。

这些操作不能和 audio callback 共用一把 mutex。

推荐模型：

```text

GUI thread:
  Source / Generator / Operator / Preset
  → build immutable RenderSnapshot
  → atomic publish

Audio thread:
  atomic load latest RenderSnapshot
  → update Matrix control state
  → update Voice control targets
  → render audio

```

`RenderSnapshot` 至少包含：

```text

SpectralTimeline timeline
StaticSpectralFrame frame preview
GlobalAdsrParams
globalGain
LFO params
Matrix rules
EffectsChainParams
Output safety gain state

```

Audio thread 不执行：

```text

file I/O
FFT analysis
generator regeneration
operator chain rebuild
JSON preset parsing
mutex lock waiting

```

这样可以避免 GUI 调参、加载 sample、保存 preset 时造成音频卡顿。

例如每 128 samples 更新一次：

$$

P_i[k]

=

\left(

\nu_i[k],

a_i[k],

x_i,

\mu_i

\right)

$$

audio-rate 内部只做插值。

频率插值：

$$

f_i[n]

=

f_i[k]

- 

s

\left(

f_i[k+1]-f_i[k]

\right)

$$

振幅插值：

$$

a_i[n]

=

a_i[k]

- 

s

\left(

a_i[k+1]-a_i[k]

\right)

$$

phase offset 插值，如果启用：

$$

\Delta\phi_i[n]

=

\Delta\phi_i[k]

- 

s

\left(

\Delta\phi_i[k+1]-\Delta\phi_i[k]

\right)

$$

其中：

$$

0\leq s\leq 1

$$

然后 audio-rate 只做：

$$

\theta_i[n+1]

=

\theta_i[n]

- 

2\pi

\frac{

f_i[n]

}{

F_s

}

$$

$$

y[n]

=

\sum_i

a_i[n]

\cos

\left(

\theta_i[n]

- 

\Delta\phi_i[n]

\right)

$$

---

# 8. Polyphony Budget / Partial Culling

初步产品必须有计算预算。

否则 64 partial × 16 voices 会很快失控。

建议第一版参数：

```text

Max voices: 16              // kMaxVoices
Max partials per voice: 256 // kMaxPartials (上限；实际值由 audible cap 进一步裁剪)
Max unison per voice: 16    // kMaxUnison
Total active oscillators cap: V * N_active * U
Control block size: 32 samples  // kControlBlockSize

```

如果当前 active oscillator 数超过预算，按规则裁剪。实际 partial 数由 §1.0 中的 audible cap 自动收敛：

```text

active partials per voice = min(frame.partialCount, floor(20000 / f0))

```

## 8.1 Amplitude Culling

如果：

$$

a_i^{final}[n]<a_{threshold}

$$

则跳过该 partial。

例如：

```text

a_threshold = -80 dB

```

线性幅度约为：

$$

a_{threshold}=10^{-80/20}=0.0001

$$

## 8.2 Nyquist Culling

如果：

$$

f_i^{final}[n]\geq \frac{F_s}{2}

$$

则必须跳过该 partial。

为了避免硬切，可以使用高频渐隐：

$$

M_{nyq}(f)

=

1-

\operatorname{smoothstep}

\left(

0.45F_s,

0.5F_s,

f

\right)

$$

然后：

$$

a_i^{final}[n]

\leftarrow

a_i^{final}[n]M_{nyq}(f_i^{final}[n])

$$

---

# 9. Gain Staging / Normalization

多个 partial 叠加很容易爆音。

每个 voice 需要 gain strategy。

基础输出：

$$

y_{voice}[n]

=

G_{voice}

\sum_i a_i[n]\cos(\theta_i[n])

$$

可选策略 1：按总幅度归一化。

$$

G_{voice}

=

\frac{1}{\sum_i a_i+\varepsilon}

$$

可选策略 2：按 active partial 数量归一化。

$$

G_{voice}

=

\frac{1}{\sqrt{N_{active}}}

$$

可选策略 3：固定 headroom。

```text

voiceGain = 0.1 ~ 0.3

masterGain = 0.5

```

第一版建议：

```text

Use fixed headroom + soft clip / limiter.

```

Soft clip 示例：

$$

y_{out}=\tanh(g y)

$$

---

# 10. Preset / Source 数据结构

初步产品必须可以保存 preset。

一个 preset 至少保存：

```text

presetName

sourceType

generatorParams

preprocessorParams

unisonParams              // §4.1 voices / detuneCents / widthStereo / phaseSpread

partialMaxRefHz           // §1.0 用于计算 "Max N" 的参考 f0

freqMode

partialCount

nuArray

ampArray

xArray

muArray

phaseInitMode

phaseSeed

ADSRParams

matrixRules

globalGain

lfoParams

shapeSource

effects

qualityMode

metadata

```

建议 JSON 结构：

```json

{

  "presetName": "Init Spectral Patch",

  "sourceType": "DirectPartialGenerator",

  "freqMode": "RelativeRatio",

  "partialMaxRefHz": 110.0,

  "unison": {

    "voices": 1,

    "detuneCents": 12.0,

    "widthStereo": 0.7,

    "phaseSpread": 1.0,

    "phaseSeed": 17

  },

  "functionalSource": {

    "filePath": "",

    "partialCount": 64,

    "userLockRoot": false,

    "userRootHz": 261.6256,

    "minRootHz": 50.0,

    "maxRootHz": 2000.0,

    "quality": 1,

    "attackSharpness": 1.0,

    "brightnessDecay": 1.0,

    "bodyResonance": 1.0,

    "transientAmount": 0.0,

    "residualAmount": 0.0,

    "phaseSeed": 1

  },

  "partialCount": 64,

  "partials": [

    {

      "nu": 1.0,

      "amp": 1.0,

      "x": 0.0,

      "group": "low"

    }

  ],

  "phase": {

    "initMode": "zero",

    "seed": 1234

  },

  "adsr": {

    "attack": 0.01,

    "decay": 0.2,

    "sustain": 0.7,

    "release": 0.4

  },

  "matrix": [],

  "effects": {

    "eq": {

      "enabled": false,

      "mode": "Normal",

      "lowGainDb": 0.0,

      "midGainDb": 0.0,

      "highGainDb": 0.0

    },

    "filter": {

      "enabled": false,

      "mode": "Normal",

      "type": "LowPass",

      "cutoffHz": 12000.0,

      "resonance": 0.0

    }

  },

  "quality": {

    "maxVoices": 16,

    "maxPartialsPerVoice": 256,

    "maxUnisonPerVoice": 16,

    "audibleCapHz": 20000,

    "blockSize": 32

  }

}

```

---
