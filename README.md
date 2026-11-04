# 五子棋 - 人机对弈 (Qt 6.2.4)

## 功能说明

- **棋盘**：标准 15×15 五子棋棋盘
- **随机分配**：每次开局随机决定玩家执黑或执白，AI 自动补位
- **AI 算法**：启发式评分 + Negamax + Alpha-Beta 剪枝 + 攻防结合策略 + 置换表 + 静止搜索
- **难度选择**：
  - **普通模式**：静态评估 + 贪心策略，响应极快，适合休闲
  - **困难模式**：迭代加深 + Negamax + Alpha-Beta 剪枝 + 置换表 + 历史/杀手启发 + 静止搜索 + 一步杀检测，攻防严密
- **悔棋**：可撤销玩家和 AI 各一步
- **视觉**：渐变棋子、最后落子标记、木色棋盘、星位标记

## 编译运行

### CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .
(或者 make -j$(nproc))
./GomokuGame
```

### qmake

```bash
qmake GomokuGame.pro
make
./GomokuGame
```

## 使用

1. 启动后点击「新游戏」
2. 选择难度（普通 / 困难）
3. 系统随机分配黑白棋（黑棋先手）
4. 在棋盘交叉点点击落子
5. 支持悔棋（撤销双方各一步）

---

## 算法架构总览

```
┌──────────────────────────────────────────────────────────────────┐
│                         findBestMove()                           │
│                     主入口调度 + 迭代加深 + 时间控制(2.5s)         │
├────────────────────┬─────────────────────────────────────────────┤
│   普通模式 NORMAL   │           困难模式 HARD                      │
│                    │                                             │
│  静态评估 + 贪心    │  一步杀检测（最高优先级）                      │
│  12:10 攻防权重    │       ↓                                     │
│  单点最优解        │  迭代加深 (深度 3→5→6→7) + 超时保护            │
│                    │       ↓                                     │
│                    │  Negamax + Alpha-Beta 剪枝                   │
│                    │       ↓                                     │
│                    │  置换表查询（Zobrist 哈希，1<<18 槽位）       │
│                    │       ↓                                     │
│                    │  走法生成（攻防排序 + 历史启发 + 杀手启发）    │
│                    │  限制宽度 top 10                              │
│                    │       ↓                                     │
│                    │  叶子节点 → 静止搜索（防水平线效应，深度≤4）   │
│                    │       ↓                                     │
│                    │  终局剪枝（成五极值返回，基于最后一手 O(1)）   │
└────────────────────┴─────────────────────────────────────────────┘
```

---

## 一、算法层面：进攻与防守的权重平衡

### 1. 普通模式（NORMAL）：静态贪心

**核心公式**：

```cpp
int attackScore = evaluatePoint(pt, m_aiColor);     // AI 落这点的进攻分
int defendScore = evaluatePoint(pt, m_enemyColor);  // 对手落这点的威胁分
int totalScore = attackScore * 12 + defendScore * 10;
```

| 项目 | 权重 | 含义 |
|------|------|------|
| 进攻 | ×12 | 自己落子的棋型威胁值 |
| 防守 | ×10 | 对手在此落子的威胁值 |
| 攻防比 | **1.2 : 1** | 进攻略重于防守 |

**设计哲学**：

- 不做搜索，只看"当前这一步"的即时价值
- 进攻权重稍高（12 vs 10），让 AI 倾向于主动进攻而非纯龟缩
- 同一套 `evaluatePoint` 评分体系，只是 `color` 参数不同

**局限性**：

- 没有组合棋型概念（双活三、四三杀），只看单点
- 不搜索后续变化，可能忽略"分数低但有后续杀招"的点
- 权重固定，不会根据局面动态调整

---

### 2. 困难模式（HARD）：迭代加深 + Negamax 多层平衡

困难模式的平衡是**分层嵌套**实现的：

#### 第零层：迭代加深与时间控制

```cpp
m_timeLimitMs = 2500;
for (int d = 1; d <= maxDepth; ++d) {
    if (m_timer.elapsed() > m_timeLimitMs) break;
    // 每一层完整搜索，超时或找到必胜则提前终止
}
```

#### 第一层：一步杀检测（绝对优先级）

```cpp
if (hasWinningMove(m_aiColor, winningMove))  return winningMove;    // 自己能赢 → 直接赢
if (hasWinningMove(m_enemyColor, enemyMove)) return enemyMove;      // 对手能赢 → 直接堵
```

#### 第二层：置换表剪枝（Transposition Table）

```cpp
TranspositionEntry& entry = m_transpositionTable[key & (size - 1)];
if (entry.depth >= depth) {
    if (entry.bound == 1) return entry.score;  // 精确值
    if (entry.bound == 2) alpha = qMax(alpha, entry.score);  // 下界
    if (entry.bound == 3) beta = qMin(beta, entry.score);    // 上界
}
```

使用 Zobrist 哈希（`m_zobrist` + `m_positionKey`）快速定位局面，避免重复搜索。

#### 第三层：走法排序中的攻防兼顾 + 启发

```cpp
int s = evaluatePoint(pt, color)                  // 进攻分 ×1.0
      + evaluatePoint(pt, opponent) * 0.9         // 防守分 ×0.9
      + m_historyTable[pt] * 1000                 // 历史启发
      + (pt == preferredMove ? 100000000 : 0)     // 置换表/上轮最佳走法
      + (pt == killerMove ? 3000 : 0);            // 杀手启发
```

| 项目 | 权重 | 说明 |
|------|------|------|
| 进攻 | ×1.0 | 当前走棋者在此点的棋型威胁 |
| 防守 | ×0.9 | 对手在此点的威胁（略低） |
| 历史启发 | ×depth² | 搜索中产生好结果的点优先尝试 |
| 杀手启发 | +3000 | 同层搜索中曾导致剪枝的走法 |
| 上一轮最佳 | +1亿 | 迭代加深中上一深度找到的最佳走法 |

走法宽度限制为 **top 10**（`limit = qMin(scored.size(), 10)`），减少分支爆炸。

#### 第四层：叶子节点与静止搜索（Quiescence Search）

```cpp
if (depth <= 0)
    return quiescence(alpha, beta, 0, color);  // 静止搜索替代直接评估
```

静止搜索只扩展有威胁的走法（己方评分≥500 或对手威胁≥10000），最多延伸 **4 层**，防止水平线效应。

#### 第五层：终局剪枝（O(1) 判定）

```cpp
// 只检查最后一手的四个方向，不再全盘扫描
if (isWinAt(last.x(), last.y(), lc)) {
    return (lc == color) 
        ? (SCORE_WIN_BASE - (10 - depth) * 1000)   // 自己赢：越浅越好
        : (-SCORE_WIN_BASE + (10 - depth) * 1000); // 对手赢：越深越晚输越好
}
```

#### 第六层：禁手过滤（黑棋专属）

```cpp
if (color == BLACK && color == m_aiColor) {
    if (!isForbiddenMove(pt.x(), pt.y(), BLACK)) filtered.append(pt);
}
```

---

### 3. 两种模式对比总结

| 维度 | 普通模式 | 困难模式 |
|------|----------|----------|
| 平衡方式 | 单点加权求和（12:10） | 多层嵌套（迭代加深 → 置换表 → 排序 → 静止搜索 → 终局 → 禁手） |
| 搜索深度 | 0（只看当前） | 动态 3~7 层 Negamax + 静止搜索 4 层 |
| 防守触发 | 被动响应（分数够高才堵） | 主动预判（搜索树 + 静止搜索自然发现威胁） |
| 组合棋型 | 无法识别 | 通过搜索发现双活三、四三等杀型 |
| 置换表 | 无 | Zobrist 哈希 + 262144 槽位缓存 |
| 时间控制 | 无 | 2.5 秒硬性超时 + 迭代加深 |
| 响应速度 | <5ms | 50~500ms（迭代加深 + 置换表加速） |

---

### 4. 攻防哲学一句话总结

> **普通模式靠"人工调权重"硬平衡，困难模式靠"迭代加深 + 搜索深度 + 轻量权重引导 + 置换表"让 AI 自己学会攻防转换。**

---

## 二、棋型层面：定义、判别与评分

### 核心判别函数：`evaluateLine`

```cpp
int evaluateLine(int r, int c, int dr, int dc, Stone color)
```

统计四个变量：`fwd`（前方连续同色）、`bwd`（后方连续同色）、`fOpen`（前方空）、`bOpen`（后方空）。

### 完整棋型评分表

| 棋型 | 连子数 | 开口条件 | 评分 | 威胁等级 |
|------|--------|----------|------|----------|
| **五连** | ≥5 | 任意 | 1,000,000 | 胜负已定 |
| **活四** | 4 | 两端空 | 100,000 | ⭐⭐⭐⭐⭐ 必胜 |
| **冲四/眠四** | 4 | 一端空 | 10,000 | ⭐⭐⭐⭐⭐ 必须应 |
| **死四** | 4 | 两端堵 | 0 | 无威胁 |
| **活三** | 3 | 两端空 | 5,000 | ⭐⭐⭐⭐ 核心进攻 |
| **眠三** | 3 | 一端空 | 500 | ⭐⭐⭐ 可发展冲四 |
| **活二** | 2 | 两端空 | 200 | ⭐⭐ 发展基础 |
| **眠二** | 2 | 一端空 | 50 | ⭐ 低价值 |
| **活一** | 1 | 两端空 | 10 | 开局占位 |
| **眠一** | 1 | 一端空 | 2 | 几乎无价值 |

### 多点评分：`evaluatePoint` 与缓存

```cpp
int evaluatePoint(int r, int c, Stone color)
{
    if (m_ptCacheValid[r][c][colorIdx]) return m_ptCache[r][c][colorIdx];  // 缓存命中
    // ... 四方向累加 ...
    m_ptCache[r][c][colorIdx] = score;   // 写入缓存
    return score;
}
```

- **缓存机制**：`m_ptCacheValid` 标记 + `invalidateCache(r,c)` 只清除落子周围 4 格内的缓存，大幅减少重复计算。
- **中心偏好**：天元附近每近一格 +3 分。

### 局面评估：`evaluateBoardForColor`

```cpp
for (const QPoint& p : m_activeCands) {   // 只遍历活跃候选点，而非全盘
    myTotal    += evaluatePoint(r, c, color);
    enemyTotal += evaluatePoint(r, c, enemy);
}
return myTotal - (int)(enemyTotal * 1.1);  // 防守权重 1.1
```

### 组合棋型判别（禁手专用）

- **双活三**：四个方向中能形成活三的方向数 ≥ 2
- **双四**：能形成四连（含冲四、活四）的方向数 ≥ 2
- **长连**：任何方向连续同色棋子数 > 5

---

## 三、算法优化手段

### 1. 候选点数组化管理

```cpp
bool m_candFlag[BOARD_SIZE][BOARD_SIZE];      // O(1) 查重
QVector<QPoint> m_activeCands;                // 紧凑存储
int m_candIndex[BOARD_SIZE][BOARD_SIZE];      // O(1) 随机删除
```

替代 `QSet<qint64>`，消除哈希开销，`addCandidate` / `removeCandidate` 均为 O(1)。

### 2. 走法排序（Move Ordering）+ 宽度限制

高分走法优先，限制只搜索 **top 10** 候选，Alpha-Beta 剪枝效率最大化。

### 3. 历史启发（History Heuristic）

```cpp
m_historyTable[r][c] += depth * depth;   // 产生 Beta 剪枝的走法加分
```

### 4. 杀手启发（Killer Heuristic）

```cpp
m_killerTable[depth][0] = mv;   // 同层搜索中导致剪枝的走法优先尝试
```

### 5. 置换表（Transposition Table）+ Zobrist 哈希

```cpp
m_positionKey ^= m_zobrist[r][c][color];  // 落子/悔棋时增量更新
m_transpositionTable.resize(1 << 18);      // 262144 槽位
```

避免重复搜索相同局面，迭代加深中效果显著。

### 6. 评估分数缓存 + 增量失效

`invalidateCache(r,c)` 只清除 `(r±4, c±4)` 范围内的缓存，避免全盘失效。

### 7. 动态搜索深度

```cpp
int getDynamicDepth() const {
    if (moves < 6)  return 3;   // 开局
    if (moves < 20) return 5;   // 中局
    if (moves < 40) return 6;   // 中后局
    return 7;                   // 终局
}
```

### 8. 静止搜索（Quiescence Search）

深度耗尽后，继续搜索有威胁的走法（评分≥500 或对手威胁≥10000），最多延伸 4 层，消除水平线效应。

### 9. 迭代加深 + 时间控制

```cpp
m_timeLimitMs = 2500;
for (int d = 1; d <= maxDepth; ++d) {
    if (m_timer.elapsed() > m_timeLimitMs) break;
    // 搜索深度 d
}
```

保证在限定时间内返回最佳走法，绝不返回半成品。

### 10. 终局 O(1) 判定

只检查 `m_moveHistory.last()` 的四方向，不再全盘扫描。

---

## 四、数据流全链路

```
玩家点击落子
     │
     ▼
setBoardState(board, history, aiColor, playerColor)
     │ → rebuildCandidates() + 清除缓存
     ▼
findBestMove()
     │
     ├─【一步杀】hasWinningMove(aiColor) → 能赢？直接返回
     ├─【一步杀】hasWinningMove(enemyColor) → 对手能赢？直接堵
     │
     ├─ NORMAL 模式 ─────────────────────────────┐
     │   getAllCandidateMoves()                    │
     │   total = attack*12 + defend*10             │
     │   返回最高分点（随机选同分点）                │
     │                                            │
     ├─ HARD 模式（迭代加深 d=1..maxDepth）────────┤
     │   │                                        │
     │   ├─ 超时检查 → break                      │
     │   │                                        │
     │   └─ for each move (top 10):               │
     │        makeMove() → -negamax(d-1, -β, -α)  │
     │              │                             │
     │              ├─ 时间检查 → m_abort          │
     │              ├─ 置换表命中？返回缓存值       │
     │              ├─ 最后一手成五？返回极值       │
     │              ├─ depth<=0？quiescence()     │
     │              ├─ generateOrderedMoves()     │
     │              │   ├─ 禁手过滤               │
     │              │   ├─ 评分 + 历史 + 杀手      │
     │              │   └─ top 10                 │
     │              └─ 递归 + Alpha-Beta 剪枝     │
     │                                           │
     │        unmakeMove()                        │
     │        更新历史表/杀手表 → 写入置换表       │
     │  返回本轮最佳走法（完整跑完才采纳）          │
     │                                            │
     └────────────────────────────────────────────┘
```

---

## 五、设计亮点与不足

### ✅ 亮点

| 设计 | 优势 |
|------|------|
| 迭代加深 + 时间控制 | 保证响应速度，2.5 秒内返回最佳结果，绝不返回半成品 |
| 置换表 + Zobrist | 避免重复搜索，迭代加深效率倍增 |
| 静止搜索 | 消除水平线效应，防止 AI 忽略即将发生的杀招 |
| 候选点数组化管理 | O(1) 增删查，比 QSet 快数倍 |
| Negamax 统一视角 | 代码简洁，递归取负即可切换攻防 |
| 多层攻防平衡 | 不同层级用不同权重策略，既有引导又不失灵活 |
| 一步杀检测 | O(n²) 扫描解决"眼前杀招"，避免搜索浪费 |
| 历史/杀手启发 | 越搜越准，剪枝效率指数级提升 |
| 禁手前置过滤 | 不在搜索树中浪费时间评估非法点 |
| 评估缓存增量失效 | 只清周围 4 格，大幅减少重复评估 |

### ⚠️ 不足与可改进方向

| 不足 | 改进方向 |
|------|----------|
| 棋型评分未考虑间隔（跳活三 `○●○●○`） | 增加对跳活三、跳冲四的识别 |
| 禁手检测频繁临时落子/悔棋 | 改增量维护，每次只更新受影响方向 |
| 无 VC（Victory of Continuous）搜索 | 专门找杀型，提升进攻锐度 |
| 静止搜索阈值固定 | 可根据深度动态调整威胁阈值 |
| 走法排序未考虑空步（Null Move） | 引入空步剪枝进一步加深搜索 |

---

## 六、总结

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  「算法层面」                                                      │
│                                                                  │
│  普通模式 = 静态权重硬编码（12:10），快但"短视"                    │
│  困难模式 = 迭代加深 + 置换表 + 搜索树 + 静止搜索 + 轻量引导        │
│                                                                  │
│  核心公式：                                                        │
│    攻防平衡 = f(静态权重, 迭代加深, 置换表, 走法排序, 历史/杀手)    │
│                                                                  │
│  ──────────────────────────────────────────────────────────────  │
│                                                                  │
│  「棋型层面」                                                      │
│                                                                  │
│  一切决策的基石 = evaluateLine() 的四方向扫描 + 评估缓存             │
│                                                                  │
│  棋型价值阶梯：                                                    │
│    五连(100万) >> 活四(10万) > 冲四(1万) > 活三(5千)             │
│    > 眠三(500) > 活二(200) > 眠二(50)                            │
│                                                                  │
│  ──────────────────────────────────────────────────────────────  │
│                                                                  │
│  「一句话总结」                                                    │
│                                                                  │
│  五子棋 AI 的本质 = 在正确的时机，找到能形成最高价值棋型的空点      │
│                    + 阻止对手做同样的事                             │
│                                                                  │
│  普通模式用"权重"近似这个目标，困难模式用"搜索+置换表"逼近它。     │
│  棋型评分体系是两种模式共享的"语言"——                              │
│  它把棋盘上的黑白排列翻译成数字，让机器能"理解"威胁。               │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 项目结构

```
GomokuGame/
├── CMakeLists.txt
├── GomokuGame.pro
├── README.md
├── gomoku.h          // AI 引擎头文件（枚举、常量、类声明）
├── gomoku.cpp        // AI 引擎实现（Negamax、置换表、静止搜索、评估、棋型判别、禁手）
├── mainwindow.h      // 主窗口头文件
├── mainwindow.cpp    // 主窗口实现（UI 交互、棋盘绘制、游戏流程）
├── main.cpp          // 程序入口
└── resources/        // 资源文件（图标等）
```

## 依赖

- Qt 6.2.4+（Core, Widgets, Gui）
- C++17 编译器
