#include "gomoku.h"

// 星位定义
const int GomokuBoard::STAR_POINTS[5] = {3, 7, 11};

// ======================== 构造 / 初始化 ========================

GomokuBoard::GomokuBoard(QWidget *parent)
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

    // 初始化棋盘
    m_board.resize(BOARD_SIZE);
    for (int i = 0; i < BOARD_SIZE; ++i)
        m_board[i].resize(BOARD_SIZE, EMPTY);

    // 连接 AI 落子信号（用单次定时器模拟"思考"，让UI有机会刷新）
    connect(this, &GomokuBoard::needAiMove, this, &GomokuBoard::onAiMove);
}

void GomokuBoard::newGame()
{
    // 随机分配黑白
    bool playerIsBlack = (QRandomGenerator::global()->bounded(2) == 0);
    if (playerIsBlack) {
        m_playerColor = BLACK;
        m_aiColor     = WHITE;
        m_currentTurn  = BLACK;   // 黑棋先手
        m_playerTurn   = true;    // 玩家执黑，玩家先下
    } else {
        m_playerColor = WHITE;
        m_aiColor     = BLACK;
        m_currentTurn  = BLACK;   // 黑棋先手
        m_playerTurn   = false;   // AI执黑，AI先下
    }

    // 清空棋盘
    for (int i = 0; i < BOARD_SIZE; ++i)
        for (int j = 0; j < BOARD_SIZE; ++j)
            m_board[i][j] = EMPTY;
    m_moveHistory.clear();
    m_lastMove = QPoint(-1, -1);
    m_gameOver = false;

    emit turnChanged(QString("新游戏开始！你执%1棋，%2先手")
                     .arg(playerIsBlack ? "黑" : "白")
                     .arg(playerIsBlack ? "你" : "电脑"));
    emit turnChanged(QString("当前回合：%1").arg(m_playerTurn ? "你的回合" : "电脑思考中..."));

    update();

    // 如果 AI 先手，触发 AI 落子
    if (!m_playerTurn) {
        emit needAiMove();
    }
}

void GomokuBoard::undo()
{
    if (m_gameOver) return;

    // 悔棋：撤销最后两步（AI一步 + 玩家一步），至少撤销一步
    int stepsToUndo = 0;

    if (!m_moveHistory.isEmpty()) {
        QPoint last = m_moveHistory.last();
        Stone lastColor = m_board[last.x()][last.y()];
        m_board[last.x()][last.y()] = EMPTY;
        m_moveHistory.removeLast();
        stepsToUndo = 1;

        // 如果上一步是AI的，再撤销一步玩家的
        if (!m_moveHistory.isEmpty() && lastColor == m_aiColor) {
            QPoint prev = m_moveHistory.last();
            m_board[prev.x()][prev.y()] = EMPTY;
            m_moveHistory.removeLast();
            stepsToUndo = 2;
        }
    }

    if (stepsToUndo == 0) {
        emit turnChanged("没有可悔的棋！");
        return;
    }

    // 恢复轮到玩家
    m_currentTurn = m_playerColor;
    m_playerTurn  = true;
    m_lastMove    = m_moveHistory.isEmpty() ? QPoint(-1, -1) : m_moveHistory.last();

    emit turnChanged(QString("悔棋成功，撤销了 %1 步").arg(stepsToUndo));
    update();
}

// ======================== 鼠标事件 ========================

void GomokuBoard::mousePressEvent(QMouseEvent *event)
{
    if (m_gameOver) return;
    if (!m_playerTurn) return;   // 不是玩家回合，忽略点击

    int r, c;
    if (!pixelToGrid(event->pos(), r, c)) return;
    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) return;
    if (m_board[r][c] != EMPTY) return;   // 已有棋子

    // 玩家落子
    m_board[r][c] = m_playerColor;
    m_lastMove    = QPoint(r, c);
    m_moveHistory.append(QPoint(r, c));

    update();

    // 检查胜负
    if (checkWin(r, c, m_playerColor)) {
        m_gameOver = true;
        emit gameOver(QString("恭喜！你赢了！🎉"));
        return;
    }

    if (isBoardFull()) {
        m_gameOver = true;
        emit gameOver("平局！棋盘已满。");
        return;
    }

    // 切换回合 → AI
    m_playerTurn  = false;
    m_currentTurn = m_aiColor;

    emit turnChanged("电脑思考中...");

    // 触发 AI 落子（用单次定时器让UI先刷新）
    QTimer::singleShot(100, this, &GomokuBoard::onAiMove);
}

// ======================== AI 落子 ========================

void GomokuBoard::onAiMove()
{
    if (m_gameOver) return;
    if (m_playerTurn) return;   // 不是AI回合

    QPoint best = findBestMove(m_aiColor);

    if (best.x() < 0 || best.y() < 0) {
        // 没有可下位置（理论上不会发生）
        return;
    }

    int r = best.x(), c = best.y();
    m_board[r][c] = m_aiColor;
    m_lastMove    = QPoint(r, c);
    m_moveHistory.append(QPoint(r, c));

    update();

    // 检查胜负
    if (checkWin(r, c, m_aiColor)) {
        m_gameOver = true;
        emit gameOver("电脑赢了！再接再厉 💪");
        return;
    }

    if (isBoardFull()) {
        m_gameOver = true;
        emit gameOver("平局！棋盘已满。");
        return;
    }

    // 切换回合 → 玩家
    m_playerTurn  = true;
    m_currentTurn = m_playerColor;

    emit turnChanged("你的回合");
}

// ---- AI 核心：找最佳落子点 ----
QPoint GomokuBoard::findBestMove(Stone color)
{
    QVector<QPoint> candidates;
    getAllCandidateMoves(color, candidates);

    if (candidates.isEmpty()) {
        // 棋盘为空时，下天元
        return QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2);
    }

    int bestScore = -1;
    QVector<QPoint> bestMoves;

    for (const QPoint &pt : candidates) {
        int attackScore = evaluatePoint(pt.x(), pt.y(), color);              // 进攻
        int defendScore  = evaluatePoint(pt.x(), pt.y(), color == BLACK ? WHITE : BLACK); // 防守
        // 综合评分：进攻为主，防守次之
        int totalScore = attackScore * 10 + defendScore;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestMoves.clear();
            bestMoves.append(pt);
        } else if (totalScore == bestScore) {
            bestMoves.append(pt);
        }
    }

    // 多个最优解时随机选一个
    if (bestMoves.isEmpty())
        return candidates.first();

    return bestMoves[QRandomGenerator::global()->bounded(bestMoves.size())];
}

// ---- 获取所有候选落子点（已有棋子周围2格范围） ----
void GomokuBoard::getAllCandidateMoves(Stone /*color*/, QVector<QPoint> &candidates)
{
    QSet<int> visited;

    // 第一步：检查是否有"必胜/必防"的紧急位置（四连 / 对手冲四）
    QVector<QPoint> urgentMoves;
    Stone enemy = (m_aiColor == BLACK) ? WHITE : BLACK;
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] != EMPTY) continue;

            // 检查这个位置是否能形成四连（进攻）
            bool urgent = false;
            m_board[r][c] = m_aiColor;
            if (wouldFormFour(r, c, m_aiColor)) urgent = true;
            m_board[r][c] = EMPTY;

            // 检查对手是否在这里形成四连（防守）
            if (!urgent) {
                m_board[r][c] = enemy;
                if (wouldFormFour(r, c, enemy)) urgent = true;
                m_board[r][c] = EMPTY;
            }
            if (urgent) {
                urgentMoves.append(QPoint(r, c));
            }
        }
    }

    // 如果有紧急防守/进攻点，只在那些点中选
    if (!urgentMoves.isEmpty()) {
        candidates = urgentMoves;
        return;
    }

    // 否则扫描所有已有棋子周围的空位
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] == EMPTY) continue;

            // 以该棋子为中心，扩展2格
            for (int dr = -2; dr <= 2; ++dr) {
                for (int dc = -2; dc <= 2; ++dc) {
                    int nr = r + dr, nc = c + dc;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
                        continue;
                    if (m_board[nr][nc] != EMPTY) continue;

                    int key = nr * BOARD_SIZE + nc;
                    if (visited.contains(key)) continue;
                    visited.insert(key);
                    candidates.append(QPoint(nr, nc));
                }
            }
        }
    }
}

// 检查在某个位置落子是否形成"四"（含活四和冲四）
bool GomokuBoard::wouldFormFour(int r, int c, Stone color)
{
    // 检查四个方向
    int drs[] = {0, 1, 1, 1};
    int dcs[] = {1, 0, 1, -1};

    for (int d = 0; d < 4; ++d) {
        int dr = drs[d], dc = dcs[d];

        // 数这个方向及反方向连续同色棋子
        int count = 1; // 包含自己

        // 正方向
        for (int i = 1; i <= 4; ++i) {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) count++;
            else break;
        }
        // 反方向
        for (int i = 1; i <= 4; ++i) {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) count++;
            else break;
        }

        if (count >= 4) return true;
    }
    return false;
}

// 评估以(r,c)为落子点、沿(dr,dc)方向延伸的一条线的"形态得分"
// color = 在该点落子一方的颜色
// 返回值：该方向的形态分值
int GomokuBoard::evaluateLine(int r, int c, int dr, int dc, Stone color)
{
    // 向正方向收集"同色连子数"和"端点外第一个格子是否为空（可延伸）"
    // 规则：扫描最多4格，统计连续同色；端点状态用 open 表示（一端可延伸）
    int forwardCount = 0;      // 正方向连续同色数
    bool forwardOpen = false;  // 正方向端点外是否还有空位（可延伸成五）
    for (int i = 1; i <= 4; ++i) {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
            break; // 到边界，该端封死
        if (m_board[nr][nc] == color) {
            forwardCount++;
        } else if (m_board[nr][nc] == EMPTY) {
            forwardOpen = true; // 遇到空格，仍可在此方向延伸
            break;
        } else {
            break; // 对手棋子，该端被封
        }
    }

    int backwardCount = 0;
    bool backwardOpen = false;
    for (int i = 1; i <= 4; ++i) {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE)
            break;
        if (m_board[nr][nc] == color) {
            backwardCount++;
        } else if (m_board[nr][nc] == EMPTY) {
            backwardOpen = true;
            break;
        } else {
            break;
        }
    }

    int totalCount = 1 + forwardCount + backwardCount; // 含当前落子点
    bool bothOpen = forwardOpen && backwardOpen;        // 两端都空 → 活棋
    bool oneOpen  = forwardOpen || backwardOpen;       // 仅一端空

    // 形态评分（含权重，区分活/冲）
    if (totalCount >= 5) {
        return 1000000;              // 已成五连
    } else if (totalCount == 4) {
        if (bothOpen) return 100000; // 活四：两端都空，下一手必胜
        if (oneOpen)  return 10000;  // 冲四：一端空，下一手成五
        return 0;                    // 死四（两端都封），无价值
    } else if (totalCount == 3) {
        if (bothOpen) return 5000;   // 活三：可发展成活四
        if (oneOpen)  return 500;    // 眠三：需借一端才能成四
        return 0;
    } else if (totalCount == 2) {
        if (bothOpen) return 200;    // 活二
        if (oneOpen)  return 50;     // 眠二
        return 0;
    } else { // totalCount == 1
        if (bothOpen) return 10;     // 活一
        if (oneOpen)  return 2;      // 眠一
        return 0;
    }
}

// ---- 评估某个位置的得分 ----
// 对四个方向分别评分后累加；加分项用于开局引导落子在中心区域
int GomokuBoard::evaluatePoint(int r, int c, Stone color)
{
    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) return 0;
    if (m_board[r][c] != EMPTY) return 0;

    int score = 0;
    int drs[] = {0, 1, 1, 1};
    int dcs[] = {1, 0, 1, -1};

    for (int d = 0; d < 4; ++d) {
        score += evaluateLine(r, c, drs[d], dcs[d], color);
    }

    // 中心位置偏好（开局引导，权重远小于成五类形态，避免干扰攻防决策）
    int centerDist = abs(r - BOARD_SIZE / 2) + abs(c - BOARD_SIZE / 2);
    if (centerDist <= 3) score += 5;

    return score;
}

// ======================== 胜负判定 ========================

bool GomokuBoard::checkWin(int r, int c, Stone color)
{
    int drs[] = {0, 1, 1, 1};
    int dcs[] = {1, 0, 1, -1};

    for (int d = 0; d < 4; ++d) {
        int dr = drs[d], dc = dcs[d];
        int count = 1;

        for (int i = 1; i <= 4; ++i) {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) count++;
            else break;
        }
        for (int i = 1; i <= 4; ++i) {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) count++;
            else break;
        }
        if (count >= 5) return true;
    }
    return false;
}

bool GomokuBoard::isBoardFull()
{
    for (int i = 0; i < BOARD_SIZE; ++i)
        for (int j = 0; j < BOARD_SIZE; ++j)
            if (m_board[i][j] == EMPTY) return false;
    return true;
}

// ======================== 坐标转换 ========================

QPoint GomokuBoard::gridToPixel(int r, int c) const
{
    return QPoint(MARGIN + c * CELL_SIZE, MARGIN + r * CELL_SIZE);
}

bool GomokuBoard::pixelToGrid(const QPoint &pt, int &r, int &c) const
{
    // 找到最近的网格交叉点
    int x = pt.x() - MARGIN;
    int y = pt.y() - MARGIN;

    if (x < -CELL_SIZE / 2 || y < -CELL_SIZE / 2) return false;
    if (x > CELL_SIZE * (BOARD_SIZE - 1) + CELL_SIZE / 2) return false;
    if (y > CELL_SIZE * (BOARD_SIZE - 1) + CELL_SIZE / 2) return false;

    c = qRound((double)x / CELL_SIZE);
    r = qRound((double)y / CELL_SIZE);

    return true;
}

// ======================== 绘制 ========================

void GomokuBoard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ---- 棋盘背景（木质纹理） ----
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

void GomokuBoard::drawBoard(QPainter &p)
{
    p.setPen(QPen(QColor(80, 50, 20), 1.2));

    // 横线
    for (int i = 0; i < BOARD_SIZE; ++i) {
        QPoint start = gridToPixel(i, 0);
        QPoint end   = gridToPixel(i, BOARD_SIZE - 1);
        p.drawLine(start, end);
    }
    // 竖线
    for (int j = 0; j < BOARD_SIZE; ++j) {
        QPoint start = gridToPixel(0, j);
        QPoint end   = gridToPixel(BOARD_SIZE - 1, j);
        p.drawLine(start, end);
    }

    // 外框加粗
    p.setPen(QPen(QColor(60, 40, 15), 2.5));
    QPoint tl = gridToPixel(0, 0);
    QPoint br = gridToPixel(BOARD_SIZE - 1, BOARD_SIZE - 1);
    p.drawRect(QRect(tl, br));
}

void GomokuBoard::drawStarPoints(QPainter &p)
{
    p.setBrush(QColor(60, 40, 15));
    p.setPen(Qt::NoPen);
    int starR = 5;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            // 星位：(3,3) (3,7) (3,11) (7,3) (7,7) (7,11) (11,3) (11,7) (11,11)
            int r = STAR_POINTS[i];
            int c = STAR_POINTS[j];
            // 只需 9 个星位
            if ((r == 3 || r == 7 || r == 11) && (c == 3 || c == 7 || c == 11)) {
                QPoint center = gridToPixel(r, c);
                p.drawEllipse(center, starR, starR);
            }
        }
    }
}

void GomokuBoard::drawCoordinateLabels(QPainter &p)
{
    p.setPen(QColor(120, 80, 40));
    QFont font = p.font();
    font.setPointSize(8);
    p.setFont(font);

    // 上方：1-15
    for (int c = 0; c < BOARD_SIZE; ++c) {
        QPoint pt = gridToPixel(-0.6, c);
        p.drawText(QRect(pt.x() - 12, pt.y() - 10, 24, 20),
                   Qt::AlignCenter, QString::number(c + 1));
    }
    // 左侧：A-O
    const char labels[] = "ABCDEFGHIJKLMNO";
    for (int r = 0; r < BOARD_SIZE; ++r) {
        QPoint pt = gridToPixel(r, -0.6);
        p.drawText(QRect(pt.x() - 12, pt.y() - 10, 24, 20),
                   Qt::AlignCenter, QString(labels[r]));
    }
}

void GomokuBoard::drawStones(QPainter &p)
{
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] == EMPTY) continue;

            QPoint center = gridToPixel(r, c);
            int radius = CELL_SIZE / 2 - 3;

            // 阴影
            p.setBrush(QColor(0, 0, 0, 60));
            p.setPen(Qt::NoPen);
            p.drawEllipse(center + QPoint(2, 2), radius, radius);

            // 棋子渐变
            QRadialGradient grad(center.x() - radius * 0.3, center.y() - radius * 0.3, radius * 1.5);
            if (m_board[r][c] == BLACK) {
                grad.setColorAt(0, QColor(90, 90, 90));
                grad.setColorAt(1, QColor(20, 20, 20));
            } else {
                grad.setColorAt(0, QColor(255, 255, 255));
                grad.setColorAt(1, QColor(200, 200, 200));
            }
            p.setBrush(grad);
            p.setPen(QPen(QColor(50, 50, 50), 0.5));
            p.drawEllipse(center, radius, radius);
        }
    }

    // 标记最后落子
    if (m_lastMove.x() >= 0 && m_lastMove.y() >= 0) {
        QPoint center = gridToPixel(m_lastMove.x(), m_lastMove.y());
        Stone lastColor = m_board[m_lastMove.x()][m_lastMove.y()];
        p.setPen(QPen(lastColor == BLACK ? Qt::white : Qt::black, 2));
        p.setBrush(Qt::NoBrush);
        int markR = CELL_SIZE / 2 - 6;
        p.drawRect(center.x() - markR, center.y() - markR, markR * 2, markR * 2);
    }
}

// ======================== 主窗口 ========================

GomokuWindow::GomokuWindow(QWidget *parent)
    : QWidget(parent)
{
    // 棋盘
    m_board = new GomokuBoard(this);

    // 状态标签
    m_statusLabel = new QLabel("点击「新游戏」开始", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(14);
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);

    // 回合信息
    m_turnLabel = new QLabel("等待开始", this);
    m_turnLabel->setAlignment(Qt::AlignCenter);
    QFont turnFont = m_turnLabel->font();
    turnFont.setPointSize(11);
    m_turnLabel->setFont(turnFont);

    // 执棋颜色
    m_colorLabel = new QLabel("", this);
    m_colorLabel->setAlignment(Qt::AlignCenter);

    // 按钮
    m_newGameBtn = new QPushButton("🆕 新游戏", this);
    m_undoBtn    = new QPushButton("↩️ 悔棋", this);

    QFont btnFont = m_newGameBtn->font();
    btnFont.setPointSize(12);
    m_newGameBtn->setFont(btnFont);
    m_undoBtn->setFont(btnFont);
    m_newGameBtn->setMinimumHeight(40);
    m_undoBtn->setMinimumHeight(40);

    // 布局
    QVBoxLayout *sideLayout = new QVBoxLayout;
    sideLayout->addWidget(m_statusLabel);
    sideLayout->addSpacing(20);
    sideLayout->addWidget(m_colorLabel);
    sideLayout->addSpacing(10);
    sideLayout->addWidget(m_turnLabel);
    sideLayout->addSpacing(30);
    sideLayout->addWidget(m_newGameBtn);
    sideLayout->addWidget(m_undoBtn);
    sideLayout->addStretch();

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_board, 1);
    mainLayout->addLayout(sideLayout);

    // 连接信号
    connect(m_newGameBtn, &QPushButton::clicked, this, &GomokuWindow::onNewGame);
    connect(m_undoBtn,    &QPushButton::clicked, this, &GomokuWindow::onUndo);
    connect(m_board, &GomokuBoard::gameOver,     this, &GomokuWindow::onGameOver);
    connect(m_board, &GomokuBoard::turnChanged,  this, &GomokuWindow::onTurnChanged);
}

void GomokuWindow::onNewGame()
{
    m_board->newGame();

    Stone pc = m_board->playerColor();
    QString colorText = (pc == BLACK)
        ? "<span style='color:#222;font-size:18px;'>⚫ 你执 <b>黑棋</b>（先手）</span>"
        : "<span style='color:#555;font-size:18px;'>⚪ 你执 <b>白棋</b>（后手）</span>";
    m_colorLabel->setText(colorText);
    m_colorLabel->setTextFormat(Qt::RichText);

    m_statusLabel->setText("游戏进行中");
    m_turnLabel->setText(m_board->isPlayerTurn() ? "你的回合" : "电脑思考中...");
}

void GomokuWindow::onUndo()
{
    m_board->undo();
    m_turnLabel->setText("已悔棋");
}

void GomokuWindow::onGameOver(QString result)
{
    m_statusLabel->setText("游戏结束");
    m_turnLabel->setText(result);
    QMessageBox::information(this, "游戏结束", result);
}

void GomokuWindow::onTurnChanged(QString info)
{
    m_turnLabel->setText(info);
}
