#include "CBoardView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QLinearGradient>
#include <QRadialGradient>

CBoardView::CBoardView(QWidget *parent)
    : QWidget(parent)
    , m_playerColor(EMPTY)
    , m_aiColor(EMPTY)
    , m_currentTurn(BLACK)
    , m_playerTurn(true)
    , m_gameOver(false)
    , m_lastMove(-1, -1)
{
    setMinimumSize(MARGIN * 2 + CELL_SIZE * (BOARD_SIZE - 1) + 60,
                   MARGIN * 2 + CELL_SIZE * (BOARD_SIZE - 1) + 60);
    setMouseTracking(true);

    m_board.resize(BOARD_SIZE);
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        m_board[i].resize(BOARD_SIZE, EMPTY);
    }

    m_ai = new gomoku(this);
    connect(this, &CBoardView::needAiMove, this, &CBoardView::onAiMove);
}

void CBoardView::newGame()
{
    bool playerIsBlack = (QRandomGenerator::global()->bounded(2) == 0);
    if (playerIsBlack)
    {
        m_playerColor = BLACK; m_aiColor = WHITE;
        m_currentTurn = BLACK; m_playerTurn = true;
    } 
    else 
    {
        m_playerColor = WHITE; m_aiColor = BLACK;
        m_currentTurn = BLACK; m_playerTurn = false;
    }

    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        for (int j = 0; j < BOARD_SIZE; ++j)
        {
            m_board[i][j] = EMPTY;
        }
    }
    m_moveHistory.clear();
    m_lastMove = QPoint(-1, -1);
    m_gameOver = false;

    emit turnChanged(QString("新游戏开始！你执%1棋，%2先手")
                     .arg(playerIsBlack ? "黑" : "白")
                     .arg(playerIsBlack ? "你" : "电脑"));
    emit turnChanged(QString("当前回合：%1").arg(m_playerTurn ? "你的回合" : "电脑思考中..."));
    update();

    if (!m_playerTurn) 
    {
        emit needAiMove();
    }
}

void CBoardView::undo()
{
    if (m_gameOver) return;
    int stepsToUndo = 0;
    if (!m_moveHistory.isEmpty()) 
    {
        QPoint last = m_moveHistory.last();
        Stone lastColor = m_board[last.x()][last.y()];
        m_board[last.x()][last.y()] = EMPTY;
        m_moveHistory.removeLast();
        stepsToUndo = 1;
        if (!m_moveHistory.isEmpty() && lastColor == m_aiColor) 
        {
            QPoint prev = m_moveHistory.last();
            m_board[prev.x()][prev.y()] = EMPTY;
            m_moveHistory.removeLast();
            stepsToUndo = 2;
        }
    }
    if (stepsToUndo == 0) 
    { 
        emit turnChanged("没有可悔的棋！"); 
        return; 
    }
    m_currentTurn = m_playerColor;
    m_playerTurn = true;
    m_lastMove = m_moveHistory.isEmpty() ? QPoint(-1, -1) : m_moveHistory.last();
    emit turnChanged(QString("悔棋成功，撤销了 %1 步").arg(stepsToUndo));
    update();
}

void CBoardView::mousePressEvent(QMouseEvent *event)
{
    if (m_gameOver || !m_playerTurn) 
        return;
    int r, c;
    if (!pixelToGrid(event->pos(), r, c)) 
        return; 

    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) 
        return;

    if (m_board[r][c] != EMPTY) 
        return;

    m_board[r][c] = m_playerColor;
    m_lastMove = QPoint(r, c);
    m_moveHistory.append(QPoint(r, c));
    update();

    if (checkWin(r, c, m_playerColor)) 
    {
        m_gameOver = true;
        emit gameOver(QString("恭喜！你赢了！🎉"));
        return;
    }
    if (isBoardFull())
     {
        m_gameOver = true;
        emit gameOver("平局！棋盘已满。");
        return;
    }

    m_playerTurn = false;
    m_currentTurn = m_aiColor;
    emit turnChanged("电脑思考中...");
    emit needAiMove();
}

void CBoardView::onAiMove()
{
    if (m_gameOver || m_playerTurn) return;

    m_ai->setBoardState(m_board, m_moveHistory, m_aiColor, m_playerColor);
    QPoint best = m_ai->findBestMove();

    if (best.x() < 0 || best.y() < 0) return;

    int r = best.x(), c = best.y();
    m_board[r][c] = m_aiColor;
    m_lastMove = QPoint(r, c);
    m_moveHistory.append(QPoint(r, c));
    update();

    if (checkWin(r, c, m_aiColor)) 
    {
        m_gameOver = true;
        emit gameOver("电脑赢了！再接再厉 💪");
        return;
    }

    if (isBoardFull()) 
    {
        m_gameOver = true;
        emit gameOver("平局！棋盘已满。");
        return;
    }

    m_playerTurn = true;
    m_currentTurn = m_playerColor;
    emit turnChanged("你的回合");
}

bool CBoardView::checkWin(int r, int c, Stone color) const
{
    for (int d = 0; d < 4; ++d)
    {
        if (countConsecutive(r, c, DIRS[d][0], DIRS[d][1], color) >= 5)
        {
            return true;
        }
    }
    return false;
}

bool CBoardView::isBoardFull() const
{
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        for (int j = 0; j < BOARD_SIZE; ++j)
        {
            if (m_board[i][j] == EMPTY) return false;
        }
    }
    return true;
}

int CBoardView::countConsecutive(int r, int c, int dr, int dc, Stone color) const
{
    int count = 1;
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
            break;
        if (m_board[nr][nc] == color) 
        {
            count++;
        }
        else
        {
            break;
        }
    }
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
            break;
        if (m_board[nr][nc] == color) 
            count++;
        else
            break;
    }
    return count;
}

QPoint CBoardView::gridToPixel(int r, int c) const
{
    return QPoint(MARGIN + c * CELL_SIZE, MARGIN + r * CELL_SIZE);
}

bool CBoardView::pixelToGrid(const QPoint &pt, int &r, int &c) const
{
    int x = pt.x() - MARGIN, y = pt.y() - MARGIN;

    if (x < -CELL_SIZE/2 || y < -CELL_SIZE/2) 
        return false;

    if (x > CELL_SIZE*(BOARD_SIZE-1)+CELL_SIZE/2)
       return false;

    if (y > CELL_SIZE*(BOARD_SIZE-1)+CELL_SIZE/2)
       return false;

    c = qRound((double)x / CELL_SIZE);
    r = qRound((double)y / CELL_SIZE);

    return true;
}

void CBoardView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient bgGrad(0, 0, width(), height());
    bgGrad.setColorAt(0, QColor(222, 179, 125));
    bgGrad.setColorAt(0.5, QColor(240, 205, 155));
    bgGrad.setColorAt(1, QColor(210, 168, 110));

    p.fillRect(rect(), bgGrad);

    drawBoard(p);
    drawStarPoints(p);
    drawCoordinateLabels(p);
    drawStones(p);
}

void CBoardView::drawBoard(QPainter &p)
{
    p.setPen(QPen(QColor(80, 50, 20), 1.2));

    for (int i = 0; i < BOARD_SIZE; ++i) 
    {
        p.drawLine(gridToPixel(i, 0), gridToPixel(i, BOARD_SIZE - 1));
        p.drawLine(gridToPixel(0, i), gridToPixel(BOARD_SIZE - 1, i));
    }

    p.setPen(QPen(QColor(60, 40, 15), 2.5));
    p.drawRect(QRect(gridToPixel(0, 0), gridToPixel(BOARD_SIZE - 1, BOARD_SIZE - 1)));
}

void CBoardView::drawStarPoints(QPainter &p)
{
    p.setBrush(QColor(60, 40, 15)); p.setPen(Qt::NoPen);
    for (int r : STAR_POINTS)
    {
        for (int c : STAR_POINTS)
        {
            p.drawEllipse(gridToPixel(r, c), 5, 5);
        }
    }
}

void CBoardView::drawCoordinateLabels(QPainter &p)
{
    p.setPen(QColor(120, 80, 40));
    QFont font = p.font(); font.setPointSize(8); p.setFont(font);
    for (int c = 0; c < BOARD_SIZE; ++c) 
    {
        QPoint pt = gridToPixel(-0.6, c);
        p.drawText(QRect(pt.x()-12, pt.y()-10, 24, 20), Qt::AlignCenter, QString::number(c+1));
    }
    const char labels[] = "ABCDEFGHIJKLMNO";
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        QPoint pt = gridToPixel(r, -0.6);
        p.drawText(QRect(pt.x()-12, pt.y()-10, 24, 20), Qt::AlignCenter, QString(labels[r]));
    }
}

void CBoardView::drawStones(QPainter &p)
{
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] == EMPTY) 
                continue;
            QPoint center = gridToPixel(r, c);
            int radius = CELL_SIZE / 2 - 3;
            p.setBrush(QColor(0, 0, 0, 60)); p.setPen(Qt::NoPen);
            p.drawEllipse(center + QPoint(2, 2), radius, radius);
            QRadialGradient grad(center.x()-radius*0.3, center.y()-radius*0.3, radius*1.5);
            if (m_board[r][c] == BLACK) 
            {
                grad.setColorAt(0, QColor(90, 90, 90)); grad.setColorAt(1, QColor(20, 20, 20));
            } 
            else 
            {
                grad.setColorAt(0, QColor(255, 255, 255)); grad.setColorAt(1, QColor(200, 200, 200));
            }
            p.setBrush(grad); p.setPen(QPen(QColor(50, 50, 50), 0.5));
            p.drawEllipse(center, radius, radius);
        }
    }
    if (m_lastMove.x() >= 0 && m_lastMove.y() >= 0) 
    {
        QPoint center = gridToPixel(m_lastMove.x(), m_lastMove.y());
        Stone lastColor = m_board[m_lastMove.x()][m_lastMove.y()];
        p.setPen(QPen(lastColor == BLACK ? Qt::white : Qt::black, 2));
        p.setBrush(Qt::NoBrush);
        int markR = CELL_SIZE / 2 - 6;
        p.drawRect(center.x()-markR, center.y()-markR, markR*2, markR*2);
    }
}
