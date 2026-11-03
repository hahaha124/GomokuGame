#ifndef GOMOKU_H
#define GOMOKU_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVector>
#include <QPoint>
#include <QMessageBox>
#include <QRandomGenerator>
#include <algorithm>
#include <QTimer>

// 棋子颜色：EMPTY=空  BLACK=黑  WHITE=白
enum Stone { EMPTY, BLACK, WHITE };

class GomokuBoard : public QWidget
{
    Q_OBJECT
public:
    explicit GomokuBoard(QWidget *parent = nullptr);

    // 对外接口
    void newGame();          // 新游戏（随机分配黑白）
    void undo();             // 悔棋
    bool isPlayerTurn() const { return m_playerTurn; }
    Stone playerColor() const { return m_playerColor; }
    Stone aiColor() const    { return m_aiColor; }
    Stone currentTurn() const{ return m_currentTurn; }

signals:
    void gameOver(QString result);   // 游戏结束信号
    void turnChanged(QString info);  // 回合变化信号
    void needAiMove();               // 请求AI落子

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onAiMove();                 // AI落子槽

private:
    // ---- 常量 ----
    static const int BOARD_SIZE = 15;     // 15x15 棋盘
    static const int MARGIN     = 40;     // 边距
    static const int CELL_SIZE  = 40;     // 格子大小
    static const int STAR_POINTS[5];      // 星位

    // ---- 数据 ----
    QVector<QVector<Stone>> m_board;   // 棋盘二维数组
    Stone m_playerColor;               // 玩家执棋颜色
    Stone m_aiColor;                  // AI 执棋颜色
    Stone m_currentTurn;              // 当前该谁下
    bool  m_playerTurn;               // 是否轮到玩家
    bool  m_gameOver;                 // 游戏是否结束
    QPoint m_lastMove;                // 最后落子位置
    QVector<QPoint> m_moveHistory;    // 落子历史（用于悔棋）

    // ---- AI ----
    QPoint findBestMove(Stone color);                      // 找最佳落子点
    int    evaluatePoint(int r, int c, Stone color);       // 评估某个位置得分
    void   getAllCandidateMoves(Stone color, QVector<QPoint>& candidates); // 获取候选点
    bool   checkWin(int r, int c, Stone color);            // 检查是否获胜
    bool   isBoardFull();                                  // 棋盘是否已满
    bool   wouldFormFour(int r, int c, Stone color);       // 是否形成四连

    // ---- 绘制 ----
    void drawBoard(QPainter &p);
    void drawStones(QPainter &p);
    void drawStarPoints(QPainter &p);
    void drawCoordinateLabels(QPainter &p);
    QPoint gridToPixel(int r, int c) const;
    bool pixelToGrid(const QPoint &pt, int &r, int &c) const;
    int evaluateLine(int r, int c, int dr, int dc, Stone color);
};

// ============ 主窗口 ============
class GomokuWindow : public QWidget
{
    Q_OBJECT
public:
    explicit GomokuWindow(QWidget *parent = nullptr);

private slots:
    void onNewGame();
    void onUndo();
    void onGameOver(QString result);
    void onTurnChanged(QString info);

private:
    GomokuBoard *m_board;
    QLabel      *m_statusLabel;
    QLabel      *m_turnLabel;
    QLabel      *m_colorLabel;
    QPushButton *m_newGameBtn;
    QPushButton *m_undoBtn;
};

#endif // GOMOKU_H
