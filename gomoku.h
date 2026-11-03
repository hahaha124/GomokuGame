#ifndef GOMOKU_H
#define GOMOKU_H
#include <QObject>
#include <QVector>
#include <QPoint>
#include <QSet>
#include <array>

// ============ 公共枚举与常量 ============
enum Stone
{
    EMPTY = 0,
    BLACK = 1,
    WHITE = -1
};
enum Difficulty
{
    NORMAL,
    HARD
};
const int BOARD_SIZE = 15;
const int DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

// 棋型枚举
enum PatternType {
    NONE = 0,
    LIVE_ONE = 1,
    SLEEP_ONE = 2,
    LIVE_TWO = 3,
    SLEEP_TWO = 4,
    LIVE_THREE = 5,
    SLEEP_THREE = 6,
    LIVE_FOUR = 7,
    SLEEP_FOUR = 8,
    FIVE = 9,
    DOUBLE_LIVE_THREE = 10,
    DOUBLE_FOUR = 11
};

// 棋型评分
const int PATTERN_SCORES[] = {
    0,      // NONE
    10,     // LIVE_ONE
    2,      // SLEEP_ONE
    100,    // LIVE_TWO
    20,     // SLEEP_TWO
    1000,   // LIVE_THREE
    100,    // SLEEP_THREE
    10000,  // LIVE_FOUR
    1000,   // SLEEP_FOUR
    1000000, // FIVE
    5000,   // DOUBLE_LIVE_THREE
    10000   // DOUBLE_FOUR
};

// 搜索节点
struct SearchNode {
    int depth;
    int alpha;
    int beta;
    int score;
    QPoint bestMove;
};

struct MoveScore {
    QPoint point;
    int score;
};

class gomoku : public QObject
{
    Q_OBJECT
public:
    explicit gomoku(QObject *parent = nullptr);

    // 设置难度
    void setDifficulty(Difficulty diff) { m_difficulty = diff; }

    // 设置局面
    void setBoardState(const QVector<QVector<Stone>>& board,
                       const QVector<QPoint>& history,
                       Stone aiColor, Stone playerColor);

    // 获取最佳落子
    QPoint findBestMove();

    // ==================== 禁手检测 ====================
    bool isForbiddenMove(int r, int c, Stone color) const;
    bool checkDoubleLiveThree(int r, int c, Stone color) const;
    bool checkDoubleFour(int r, int c, Stone color) const;
    bool checkLongConnect(int r, int c, Stone color) const;

private:
    // ==================== 普通模式 ====================
    void getAllCandidateMoves(QVector<QPoint>& candidates);
    bool wouldFormFour(int r, int c, Stone color) const;
    int evaluatePoint(int r, int c, Stone color);
    int evaluateLine(int r, int c, int dr, int dc, Stone color);
    int countConsecutive(int r, int c, int dr, int dc, Stone color) const;

    // ==================== 困难模式：Minimax + AB剪枝 ====================
    int negamax(int depth, int alpha, int beta, Stone color);
    int quiescenceNegamax(int alpha, int beta, int depth, Stone color);
    int evaluateBoardForColor(Stone color);
    QVector<QPoint> generateOrderedMoves(Stone color);
    int evaluateBoard(Stone perspective);

    // 棋型模式匹配
    PatternType detectPattern(int r, int c, int dr, int dc, Stone color) const;
    int countPattern(int r, int c, Stone color, PatternType pattern) const;

    // 移动排序
    void updateHistoryTable(const QPoint& move, int depth);
    QVector<QPoint> getKillerMoves(int depth) const;

    // 动态深度
    int getDynamicDepth() const;

    // 一步杀检测
    bool hasWinningMove(Stone color, QPoint& winningMove);

private:
    QVector<QVector<Stone>> m_board;
    QVector<QPoint> m_moveHistory;
    Stone m_aiColor;
    Stone m_enemyColor;
    Difficulty m_difficulty;

    // 历史启发表 (15x15)
    int m_historyTable[BOARD_SIZE][BOARD_SIZE];
    // 杀手走法表 [depth][2]
    QPoint m_killerTable[64][2];
    int m_nodesSearched;
};


#endif // GOMOKU_H
