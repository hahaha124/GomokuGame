#ifndef GOMOKU_H
#define GOMOKU_H

#include <QObject>
#include <QVector>
#include <QPoint>
#include <QHash>
#include <QSet>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <algorithm>
#include <climits>

enum Stone { EMPTY = 0, BLACK = 1, WHITE = -1 };
enum Difficulty { NORMAL, HARD };
const int BOARD_SIZE = 15;
const int DIRS[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};

const int SCORE_FIVE      = 1000000;
const int SCORE_WIN_BASE  = 1000000;

class gomoku : public QObject
{
    Q_OBJECT
public:
    explicit gomoku(QObject *parent = nullptr);
    void setDifficulty(Difficulty diff) { m_difficulty = diff; }
    void setBoardState(const QVector<QVector<Stone>>& board,
                       const QVector<QPoint>& history,
                       Stone aiColor, Stone playerColor);
    QPoint findBestMove();
    void abortCalculation() { m_abort = true; }

signals:
    void moveCalculated(const QPoint& move);

public slots:
    void asyncFindBestMove(); 

private:
    struct TranspositionEntry {
        quint64 key;
        int score;
        int depth;
        char bound;
        QPoint bestMove;

        TranspositionEntry()
            : key(0), score(0), depth(-1), bound(0), bestMove(-1, -1) {}
    };

    // 搜索
    int  negamax(int depth, int alpha, int beta, Stone color);
    int  quiescence(int alpha, int beta, int qdepth, Stone color);
    int  getDynamicDepth() const;
    bool hasWinningMove(Stone color, QPoint& move);

    // 着法生成
    QVector<QPoint> generateOrderedMoves(Stone color, const QPoint& preferredMove = QPoint(-1, -1));
    void getAllCandidateMoves(QVector<QPoint>& candidates);

    // 评估
    int  evaluateBoardForColor(Stone color);
    int  evaluatePoint(int r, int c, Stone color);
    int  evaluateLine(int r, int c, int dr, int dc, Stone color);
    int  countConsecutive(int r, int c, int dr, int dc, Stone color) const;
    int  lineMaxConsecutive(int r, int c, Stone color) const;   // [新增] 跨4方向取max
    bool isWinAt(int r, int c, Stone color) const;              // [新增] 单点连五判定

    // 普通模式
    bool wouldFormFour(int r, int c, Stone color) const;

    // 禁手
    bool isForbiddenMove(int r, int c, Stone color) const;
    bool checkLongConnect(int r, int c, Stone color) const;
    bool checkDoubleLiveThree(int r, int c, Stone color) const;
    bool checkDoubleFour(int r, int c, Stone color) const;

    // 增量候选点管理
    void makeMove(int r, int c, Stone color);
    void unmakeMove(int r, int c);
    void addCandidate(int r, int c);
    void removeCandidate(int r, int c);
    void rebuildCandidates();
    void collectCandidates(QVector<QPoint>& out) const;
    void invalidateCache(int r, int c);
private:
    QVector<QVector<Stone>> m_board;
    QVector<QPoint> m_moveHistory;
    Stone m_aiColor;
    Stone m_enemyColor;
    Difficulty m_difficulty;

    int m_historyTable[BOARD_SIZE][BOARD_SIZE];
    QPoint m_killerTable[64][2];
    QVector<TranspositionEntry> m_transpositionTable;
    quint64 m_zobrist[BOARD_SIZE][BOARD_SIZE][2];
    quint64 m_positionKey;

    QElapsedTimer m_timer;
    int m_timeLimitMs;
    qint64 m_nodesSearched;
    std::atomic<bool> m_abort;                     // [新增] 超时中断标志

    // ===== 增量候选点数据 =====
    bool  m_candFlag[BOARD_SIZE][BOARD_SIZE];
    int   m_candRow[BOARD_SIZE][BOARD_SIZE];
    int   m_candIndex[BOARD_SIZE][BOARD_SIZE];
    QVector<QPoint> m_activeCands;
    int   m_pieceCount;

    // ===== 评估点缓存 =====
    int  m_ptCache[BOARD_SIZE][BOARD_SIZE][2];
    bool m_ptCacheValid[BOARD_SIZE][BOARD_SIZE][2];
};

#endif // GOMOKU_H
