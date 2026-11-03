#ifndef CBOARDVIEW_H
#define CBOARDVIEW_H

#include <QWidget>
#include <QObject>
#include <QVector>
#include <QPoint>
#include "gomoku.h"

const int MARGIN    = 30;
const int CELL_SIZE = 40;
const int STAR_POINTS[] = {3, 7, 11};

class CBoardView : public QWidget
{
    Q_OBJECT
public:
    explicit CBoardView(QWidget *parent = nullptr);

    void newGame();
    void undo();
    bool isPlayerTurn() const { return m_playerTurn; }
    Stone playerColor() const { return m_playerColor; }
    void setDifficulty(Difficulty diff) { m_ai->setDifficulty(diff); }

signals:
    void gameOver(QString result);
    void turnChanged(QString info);
    void needAiMove();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawBoard(QPainter &p);
    void drawStarPoints(QPainter &p);
    void drawCoordinateLabels(QPainter &p);
    void drawStones(QPainter &p);
    QPoint gridToPixel(int r, int c) const;
    bool pixelToGrid(const QPoint &pt, int &r, int &c) const;
    bool checkWin(int r, int c, Stone color) const;
    bool isBoardFull() const;
    int countConsecutive(int r, int c, int dr, int dc, Stone color) const;
    void onAiMove();

    gomoku *m_ai;
    QVector<QVector<Stone>> m_board;
    QVector<QPoint> m_moveHistory;
    Stone m_playerColor;
    Stone m_aiColor;
    Stone m_currentTurn;
    bool  m_playerTurn;
    bool  m_gameOver;
    QPoint m_lastMove;
};

#endif // CBOARDVIEW_H
