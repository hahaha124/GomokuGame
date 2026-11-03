# 五子棋 - 人机对弈 (Qt 6.2.4)

## 功能说明

- **棋盘**：标准 15×15 五子棋棋盘
- **随机分配**：每次开局随机决定玩家执黑或执白，AI 自动补位
- **AI 算法**：启发式评分 + 攻防结合策略
- **悔棋**：可撤销玩家和AI各一步
- **视觉**：渐变棋子、最后落子标记、木色棋盘、星位标记

## 编译运行

### CMake

```bash
mkdir build && cd build
cmake ..
cmake --build .
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
2. 系统随机分配黑白棋（黑棋先手）
3. 在棋盘交叉点点击落子
4. 支持悔棋
