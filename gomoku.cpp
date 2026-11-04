#include "gomoku.h"
#include <QRandomGenerator>
#include <algorithm>
#include <climits>

gomoku::gomoku(QObject *parent) : QObject(parent), m_difficulty(NORMAL)
{
    memset(m_historyTable, 0, sizeof(m_historyTable));
    for (int i = 0; i < 64; ++i) 
    {
        m_killerTable[i][0] = QPoint(-1, -1);
        m_killerTable[i][1] = QPoint(-1, -1);
    }
}

void gomoku::setBoardState(const QVector<QVector<Stone>>& board, const QVector<QPoint>& history, Stone aiColor, Stone playerColor)
{
    m_board = board;
    m_moveHistory = history;
    m_aiColor = aiColor;
    m_enemyColor = playerColor;
}

QPoint gomoku::findBestMove()
{
    // ===== 一步杀检测（根节点，AI自己能赢直接返回）=====
    QPoint winningMove;
    if (hasWinningMove(m_aiColor, winningMove))
    {
        return winningMove;
    }
    
    // ===== 检查玩家是否一步杀，如果是则堵（紧急防守）=====
    QPoint playerWinMove;
    if (hasWinningMove(m_enemyColor, playerWinMove)) 
    {
        return playerWinMove; // 不假思索直接堵
    }

    if (m_difficulty == NORMAL) 
    {
        // ===== 普通模式: 静态贪心 =====
        QVector<QPoint> candidates;
        getAllCandidateMoves(candidates);

        if (candidates.isEmpty())
        {
            return QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2);
        }

        int bestScore = -1;
        QVector<QPoint> bestMoves;

        for (const QPoint &pt : candidates) 
        {
            int attackScore = evaluatePoint(pt.x(), pt.y(), m_aiColor);
            int defendScore = evaluatePoint(pt.x(), pt.y(), m_enemyColor);
            int totalScore = attackScore * 12 + defendScore * 10;

            if (totalScore > bestScore) 
            {
                bestScore = totalScore;
                bestMoves.clear();
                bestMoves.append(pt);
            } 
            else if (totalScore == bestScore) 
            {
                bestMoves.append(pt);
            }
        }

        if (bestMoves.isEmpty())
            return candidates.first();

        return bestMoves[QRandomGenerator::global()->bounded(bestMoves.size())];
    }

    // ===== 困难模式：Negamax + Alpha-Beta =====
    m_nodesSearched = 0;
    int depth = getDynamicDepth();

    int alpha = INT_MIN + 1;
    int beta = INT_MAX - 1;
    int bestScore = INT_MIN;
    QPoint bestMove(-1, -1);

    QVector<QPoint> orderedMoves = generateOrderedMoves(m_aiColor);

    if (orderedMoves.size() > 15)
        orderedMoves.resize(15);

    for (const QPoint& move : orderedMoves) 
    {
        int r = move.x(), c = move.y();
        m_board[r][c] = m_aiColor;
        m_moveHistory.append(move);

        // negamax：下一层是对手，返回的分数取负
        int score = -negamax(depth - 1, -beta, -alpha, m_enemyColor);

        m_board[r][c] = EMPTY;
        m_moveHistory.removeLast();

        if (score > bestScore) 
        {
            bestScore = score;
            bestMove = move;
        }
        alpha = qMax(alpha, score);
    }

    // 如果搜索失败（理论上不会），退回普通模式逻辑
    if (bestMove.x() == -1) 
    {
        return QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2);
    }

    return bestMove;
}

int gomoku::negamax(int depth, int alpha, int beta, Stone color)
{
    m_nodesSearched++;

    QPoint winMove;
    if (hasWinningMove(color, winMove)) 
    {
        // 当前走棋者能一步赢 → 返回极大值（depth 越大越优先）
        return 1000000 - (10 - depth) * 1000;
    }

    // 终局快剪枝：如果当前局面已经有一方连成五子，则返回极值
    if (!m_moveHistory.isEmpty()) 
    {
        QPoint last = m_moveHistory.last();
        Stone lastColor = m_board[last.x()][last.y()];
        int consecutive = 0;
        for (int d = 0; d < 4; ++d) 
        {
            int cnt = 1;
            for (int i = 1; i <= 4; ++i) 
            {
                int nr = last.x() + DIRS[d][0] * i, nc = last.y() + DIRS[d][1] * i;
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                    break;
                if (m_board[nr][nc] == lastColor) 
                {
                    cnt++;
                }
                else break;
            }
            for (int i = 1; i <= 4; ++i) 
            {
                int nr = last.x() - DIRS[d][0] * i, nc = last.y() - DIRS[d][1] * i;
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                    break;
                if (m_board[nr][nc] == lastColor) 
                {
                    cnt++;
                }
                else break;
            }
            consecutive = qMax(consecutive, cnt);
        }
        if (consecutive >= 5) 
        {
            return (lastColor == color) ? (1000000 - (10 - depth) * 1000)
                                        : (-1000000 + (10 - depth) * 1000);
        }
    }

    if (depth <= 0) 
    {
        return evaluateBoardForColor(color);
    }

    QVector<QPoint> moves = generateOrderedMoves(color);

    if (moves.isEmpty()) 
    {
        return evaluateBoardForColor(color);
    }

    QPoint bestMove(-1, -1);
    int originalAlpha = alpha;

    for (const QPoint& move : moves) 
    {
        int r = move.x(), c = move.y();
        m_board[r][c] = color;
        m_moveHistory.append(move);

        int score = -negamax(depth - 1, -beta, -alpha,
                             (color == m_aiColor) ? m_enemyColor : m_aiColor);

        m_board[r][c] = EMPTY;
        m_moveHistory.removeLast();

        if (score >= beta) 
        {
            // 更新历史表：好的走法加分
            m_historyTable[r][c] += depth * depth;
            return beta;
        }
        if (score > alpha) 
        {
            alpha = score;
            bestMove = move;
        }
    }

    // 更新历史表：最佳走法加分
    if (bestMove.x() != -1) 
    {
        m_historyTable[bestMove.x()][bestMove.y()] += depth * depth;
    }

    return alpha;
}

int gomoku::evaluateBoardForColor(Stone color)
{
    Stone enemy = (color == m_aiColor) ? m_enemyColor : m_aiColor;

    int myScore = 0;
    int enemyScore = 0;

    QSet<qint64> evaluated;
    int myTotal = 0, enemyTotal = 0;

    for (int i = qMax(0, m_moveHistory.size() - 4); i < m_moveHistory.size(); ++i) 
    {
        QPoint p = m_moveHistory[i];
        for (int dr = -2; dr <= 2; ++dr) 
        {
            for (int dc = -2; dc <= 2; ++dc) 
            {
                int nr = p.x() + dr, nc = p.y() + dc;
                if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                    continue;
                if (m_board[nr][nc] != EMPTY) 
                    continue;
                qint64 key = (qint64)nr * BOARD_SIZE + nc;
                if (evaluated.contains(key)) 
                    continue;
                evaluated.insert(key);

                myTotal += evaluatePoint(nr, nc, color);
                enemyTotal += evaluatePoint(nr, nc, enemy);
            }
        }
    }

    int score = (color == m_aiColor) ? (myTotal - enemyTotal * 1.1) 
                                     : (enemyTotal * 1.1 - myTotal);

    return score;
}

// ==================== 静止搜索 ====================
int gomoku::quiescenceNegamax(int alpha, int beta, int depth, Stone color)
{
    int standPat = evaluateBoardForColor(color);

    if (standPat >= beta)
        return beta;

    if (standPat > alpha)
        alpha = standPat;

    if (depth > 2) 
        return standPat;

    Stone opponent = (color == m_aiColor) ? m_enemyColor : m_aiColor;
    QVector<QPoint> moves = generateOrderedMoves(color);

    for (const QPoint& move : moves) 
    {
        int r = move.x(), c = move.y();
        int threatScore = evaluatePoint(r, c, color);
        if (threatScore < 500) continue; // 只搜索有威胁的走法

        m_board[r][c] = color;
        m_moveHistory.append(move);

        int score = -quiescenceNegamax(-beta, -alpha, depth + 1, opponent);

        m_board[r][c] = EMPTY;
        m_moveHistory.removeLast();

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

// ==================== 有序移动生成 ====================
QVector<QPoint> gomoku::generateOrderedMoves(Stone color)
{
    QVector<QPoint> candidates;
    bool visited[BOARD_SIZE][BOARD_SIZE] = {false};  

    int searchRange = (m_moveHistory.size() < 10) ? 2 : 3;

    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] != EMPTY) 
                continue;
            if (visited[r][c]) 
                continue;           

            bool nearPiece = false;
            for (int dr = -searchRange; dr <= searchRange; ++dr) 
            {
                for (int dc = -searchRange; dc <= searchRange; ++dc) 
                {
                    if (dr == 0 && dc == 0) 
                        continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) 
                    {
                        if (m_board[nr][nc] != EMPTY) 
                        {
                            nearPiece = true;
                            goto found;
                        }
                    }
                }
            }
        found:
            if (!nearPiece && m_moveHistory.size() > 0) continue;

            visited[r][c] = true;                  
            candidates.append(QPoint(r, c));
        }
    }

    // 禁手过滤
    if (color == BLACK && color == m_aiColor) 
    {
        QVector<QPoint> filtered;
        for (const QPoint& pt : candidates) 
        {
            if (!isForbiddenMove(pt.x(), pt.y(), BLACK)) 
            {
                filtered.append(pt);
            }
        }
        if (!filtered.isEmpty()) 
        {
            candidates = filtered;
        }
    }

    // 评分 + 排序（每个点只算一次）
    Stone opponent = (color == m_aiColor) ? m_enemyColor : m_aiColor;
    struct MoveScore 
    { 
        QPoint point; 
        int score; 
    };
    QVector<MoveScore> scored;
    scored.reserve(candidates.size());

    for (const QPoint& pt : candidates) 
    {
        int s = evaluatePoint(pt.x(), pt.y(), color)
                + evaluatePoint(pt.x(), pt.y(), opponent) * 0.9
                + m_historyTable[pt.x()][pt.y()] * 1000;
        scored.append({pt, s});
    }

    std::sort(scored.begin(), scored.end(),
              [](const MoveScore& a, const MoveScore& b) 
              {
                  return a.score > b.score;
              });

    QVector<QPoint> result;
    int limit = qMin(scored.size(), 12);
    result.reserve(limit);
    for (int i = 0; i < limit; ++i)
    {
        result.append(scored[i].point);
    }

    return result;
}

// ==================== 局面评估 ====================
int gomoku::evaluateBoard(Stone perspective)
{
    Stone myColor = m_aiColor;
    Stone enemyColor = m_enemyColor;

    if (perspective == m_enemyColor) 
    {
        myColor = m_enemyColor;
        enemyColor = m_aiColor;
    }

    int myScore = 0;
    int enemyScore = 0;

    // 评估所有棋子
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] == myColor) 
            {
                myScore += evaluatePoint(r, c, myColor);
            } else if (m_board[r][c] == enemyColor) 
            {
                enemyScore += evaluatePoint(r, c, enemyColor);
            }
        }
    }

    // 检查禁手（黑棋）
    if (m_aiColor == BLACK && !m_moveHistory.isEmpty()) 
    {
        QPoint last = m_moveHistory.last();
        if (m_board[last.x()][last.y()] == BLACK) 
        {
            if (isForbiddenMove(last.x(), last.y(), BLACK)) 
            {
                return INT_MIN / 2; // 禁手判负
            }
        }
    }

    return myScore - enemyScore * 1.1; // 防守略重于进攻
}

// ==================== 禁手检测 ====================
bool gomoku::isForbiddenMove(int r, int c, Stone color) const
{
    if (color != BLACK) 
        return false; // 只有黑棋有禁手

    // 临时落子
    gomoku* self = const_cast<gomoku*>(this);
    self->m_board[r][c] = BLACK;

    bool forbidden = false;

    // 检查长连（超过5连）
    if (checkLongConnect(r, c, BLACK)) 
    {
        forbidden = true;
    }
    // 检查双活三
    else if (checkDoubleLiveThree(r, c, BLACK)) 
    {
        forbidden = true;
    }
    // 检查双四
    else if (checkDoubleFour(r, c, BLACK)) 
    {
        forbidden = true;
    }

    self->m_board[r][c] = EMPTY;
    return forbidden;
}

bool gomoku::checkLongConnect(int r, int c, Stone color) const
{
    for (int d = 0; d < 4; ++d) 
    {
        int count = 1;
        for (int i = 1; i <= 5; ++i) 
        {
            int nr = r + DIRS[d][0] * i, nc = c + DIRS[d][1] * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                break;
            if (m_board[nr][nc] == color) 
                count++;
            else break;
        }
        for (int i = 1; i <= 5; ++i) 
        {
            int nr = r - DIRS[d][0] * i, nc = c - DIRS[d][1] * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
            {
                break;
            }
            if (m_board[nr][nc] == color) 
            {
                count++;
            }
            else break;
        }
        if (count > 5) return true;
    }
    return false;
}

bool gomoku::checkDoubleLiveThree(int r, int c, Stone color) const
{
    int liveThreeCount = 0;

    for (int d = 0; d < 4; ++d) 
    {
        int dr = DIRS[d][0], dc = DIRS[d][1];

        // 检查这个方向是否形成活三
        int forward = 0, backward = 0;
        bool fOpen = false, bOpen = false;

        for (int i = 1; i <= 3; ++i) 
        {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                break;
            if (m_board[nr][nc] == color) forward++;
            else if (m_board[nr][nc] == EMPTY) 
            { 
                fOpen = true; break; 
            }
            else break;
        }
        for (int i = 1; i <= 3; ++i) 
        {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                break;
            if (m_board[nr][nc] == color)  
                backward++;
            else if (m_board[nr][nc] == EMPTY) 
            { 
                bOpen = true; break; 
            }
            else break;
        }

        if ((forward + backward >= 2) && fOpen && bOpen)
            liveThreeCount++;
    }

    return liveThreeCount >= 2;
}

bool gomoku::checkDoubleFour(int r, int c, Stone color) const
{
    int fourCount = 0;

    for (int d = 0; d < 4; ++d) 
    {
        int dr = DIRS[d][0], dc = DIRS[d][1];
        int forward = 0, backward = 0;
        bool fOpen = false, bOpen = false;

        for (int i = 1; i <= 4; ++i) 
        {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                break;
            if (m_board[nr][nc] == color) 
            {
                forward++;
            }
            else if (m_board[nr][nc] == EMPTY) 
            { 
                fOpen = true; break; 
            }
            else break;
        }
        for (int i = 1; i <= 4; ++i) 
        {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                break;
            if (m_board[nr][nc] == color) 
            {
                backward++;
            }
            else if (m_board[nr][nc] == EMPTY) 
            { 
                bOpen = true; break; 
            }
            else break;
        }

        if ((forward + backward >= 3) && (fOpen || bOpen))
        {
            fourCount++;
        }
    }

    return fourCount >= 2;
}

// ==================== 动态深度 ====================
int gomoku::getDynamicDepth() const
{
    int moves = m_moveHistory.size();

    if (moves < 6)  return 2;   // 开局
    if (moves < 20) return 3;   // 中局
    if (moves < 40) return 4;   // 中后局
    return 5;                   // 终局
}

bool gomoku::hasWinningMove(Stone color, QPoint& winningMove)
{
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] != EMPTY) continue;

            // 临时落子
            m_board[r][c] = color;
            bool win = false;

            for (int d = 0; d < 4; ++d) 
            {
                int cnt = 1;
                for (int i = 1; i <= 4; ++i) 
                {
                    int nr = r + DIRS[d][0] * i, nc = c + DIRS[d][1] * i;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                        break;
                    if (m_board[nr][nc] == color) 
                        cnt++;
                    else break;
                }
                for (int i = 1; i <= 4; ++i) 
                {
                    int nr = r - DIRS[d][0] * i, nc = c - DIRS[d][1] * i;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                        break;
                    if (m_board[nr][nc] == color) 
                    {
                        cnt++;
                    }
                    else break;
                }
                if (cnt >= 5) 
                { 
                    win = true; break; 
                }
            }

            m_board[r][c] = EMPTY;
            if (win) 
            {
                winningMove = QPoint(r, c);
                return true;
            }
        }
    }
    return false;
}

// ==================== 普通模式函数（保留） ====================
void gomoku::getAllCandidateMoves(QVector<QPoint>& candidates)
{
    candidates.clear();
    bool visited[BOARD_SIZE][BOARD_SIZE] = {false}; 

    // 紧急点检查
    QVector<QPoint> urgentMoves;
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] != EMPTY) 
                continue;

            bool urgent = false;

            m_board[r][c] = m_aiColor;
            if (wouldFormFour(r, c, m_aiColor)) 
            {
                urgent = true;
            }
            m_board[r][c] = EMPTY;

            if (!urgent) 
            {
                m_board[r][c] = m_enemyColor;
                if (wouldFormFour(r, c, m_enemyColor)) 
                {
                    urgent = true;
                }
                m_board[r][c] = EMPTY;
            }
            if (urgent) urgentMoves.append(QPoint(r, c));
        }
    }

    if (!urgentMoves.isEmpty()) 
    {
        candidates = urgentMoves;
        return;
    }

    int searchRange = (m_moveHistory.size() < 10) ? 2 : 3;
    for (int r = 0; r < BOARD_SIZE; ++r) 
    {
        for (int c = 0; c < BOARD_SIZE; ++c) 
        {
            if (m_board[r][c] == EMPTY) continue;
            for (int dr = -searchRange; dr <= searchRange; ++dr) 
            {
                for (int dc = -searchRange; dc <= searchRange; ++dc) 
                {
                    if (dr == 0 && dc == 0) 
                    {
                        continue;
                    }
                    int nr = r + dr, nc = c + dc;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
                    {
                        continue;
                    }
                    if (m_board[nr][nc] != EMPTY) 
                    {
                        continue;
                    }
                    if (visited[nr][nc]) 
                    {
                        continue;   
                    } 
                    visited[nr][nc] = true;        
                    candidates.append(QPoint(nr, nc));
                }
            }
        }
    }
}

bool gomoku::wouldFormFour(int r, int c, Stone color) const
{
    for (int d = 0; d < 4; ++d) 
    {
        if (countConsecutive(r, c, DIRS[d][0], DIRS[d][1], color) >= 4)
            return true;
    }
    return false;
}

int gomoku::countConsecutive(int r, int c, int dr, int dc, Stone color) const
{
    int count = 1;
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
        {
            break;
        }
        if (m_board[nr][nc] == color) 
        {
            count++;
        }
        else break;
    }
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
        {
            break;
        }
        if (m_board[nr][nc] == color) 
        {
            count++;
        }
        else break;
    }
    return count;
}

int gomoku::evaluateLine(int r, int c, int dr, int dc, Stone color)
{
    int forwardCount = 0;
    bool forwardOpen = false;
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
        {
            break;
        }
        if (m_board[nr][nc] == color) 
        {
            forwardCount++;
        }
        else if (m_board[nr][nc] == EMPTY) 
        { 
            forwardOpen = true; 
            break; 
        }
        else break;
    }

    int backwardCount = 0;
    bool backwardOpen = false;
    for (int i = 1; i <= 4; ++i) 
    {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) 
        {
            break;
        }
        if (m_board[nr][nc] == color) 
        {
            backwardCount++;
        }
        else if (m_board[nr][nc] == EMPTY) 
        { 
            backwardOpen = true; 
            break; 
        }
        else break;
    }

    int totalCount = 1 + forwardCount + backwardCount;
    bool bothOpen = forwardOpen && backwardOpen;
    bool oneOpen = forwardOpen || backwardOpen;

    if (totalCount >= 5) 
    {
        return 1000000;
    }
    else if (totalCount == 4) 
    {
        if (bothOpen) 
            return 100000;
        if (oneOpen) 
            return 10000;
        return 0;
    } 
    else if (totalCount == 3) 
    {
        if (bothOpen) 
            return 5000;
        if (oneOpen) 
            return 500;
        return 0;
    } 
    else if (totalCount == 2) 
    {
        if (bothOpen) 
            return 200;
        if (oneOpen) 
            return 50;
        return 0;
    } 
    else 
    {
        if (bothOpen) 
            return 10;
        if (oneOpen) 
            return 2;
        return 0;
    }
}

int gomoku::evaluatePoint(int r, int c, Stone color)
{
    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) 
        return 0;
    if (m_board[r][c] != EMPTY) 
        return 0;

    int score = 0;
    for (int d = 0; d < 4; ++d) 
    {
        score += evaluateLine(r, c, DIRS[d][0], DIRS[d][1], color);
    }

    // 中心偏好
    int centerR = BOARD_SIZE / 2, centerC = BOARD_SIZE / 2;
    int maxDist = qMax(qAbs(r - centerR), qAbs(c - centerC));
    if (maxDist <= 3) score += (4 - maxDist) * 3;

    return score;
}
