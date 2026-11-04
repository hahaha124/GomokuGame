#include "gomoku.h"
#include <QRandomGenerator>
#include <algorithm>
#include <climits>

template <typename Function>
static inline void forNeighbors(int r, int c, int range, Function fn)
{
    for (int dr = -range; dr <= range; ++dr)
    {
        for (int dc = -range; dc <= range; ++dc) 
        {
            if (dr == 0 && dc == 0) continue;
            int nr = r + dr, nc = c + dc;
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE)
                fn(nr, nc);
        }
    }
}

static quint64 nextZobristValue(quint64& state)
{
    state += Q_UINT64_C(0x9e3779b97f4a7c15);
    quint64 value = state;
    value = (value ^ (value >> 30)) * Q_UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * Q_UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

// ==================== 构造 / 初始化 ====================
gomoku::gomoku(QObject *parent) 
    : QObject(parent)
    , m_difficulty(NORMAL)
    , m_pieceCount(0)
    , m_aiColor(EMPTY)
    , m_enemyColor(EMPTY)
{
    memset(m_historyTable, 0, sizeof(m_historyTable));
    memset(m_killerTable, 0, sizeof(m_killerTable));
    memset(m_candFlag, 0, sizeof(m_candFlag));
    memset(m_candRow, 0, sizeof(m_candRow));
    memset(m_candIndex, 0xff, sizeof(m_candIndex));
    memset(m_ptCacheValid, 0, sizeof(m_ptCacheValid));
    for (int i = 0; i < 64; ++i) {
        m_killerTable[i][0] = QPoint(-1, -1);
        m_killerTable[i][1] = QPoint(-1, -1);
    }
    m_timeLimitMs = 2500;
    m_nodesSearched = 0;
    m_abort = false;
    m_positionKey = 0;
    m_transpositionTable.resize(1 << 18);

    quint64 seed = Q_UINT64_C(0x243f6a8885a308d3);
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            for (int color = 0; color < 2; ++color)
                m_zobrist[r][c][color] = nextZobristValue(seed);
}

void gomoku::setBoardState(const QVector<QVector<Stone>>& board,
                           const QVector<QPoint>& history,
                           Stone aiColor, Stone playerColor)
{
    m_board = board;
    m_moveHistory = history;
    m_aiColor = aiColor;
    m_enemyColor = playerColor;
    m_positionKey = 0;
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c)
            if (m_board[r][c] != EMPTY)
                m_positionKey ^= m_zobrist[r][c][m_board[r][c] == BLACK ? 0 : 1];

    memset(m_historyTable, 0, sizeof(m_historyTable));
    for (int i = 0; i < 64; ++i) {
        m_killerTable[i][0] = QPoint(-1, -1);
        m_killerTable[i][1] = QPoint(-1, -1);
    }

    rebuildCandidates();
    memset(m_ptCacheValid, 0, sizeof(m_ptCacheValid));
}

void gomoku::asyncFindBestMove()
{
    QPoint best = findBestMove();
    emit moveCalculated(best);
}

// ==================== 连子判定（修复核心） ====================

int gomoku::countConsecutive(int r, int c, int dr, int dc, Stone color) const
{
    int count = 1;
    for (int i = 1; i <= 4; ++i) {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
        if (m_board[nr][nc] == color) count++; else break;
    }
    for (int i = 1; i <= 4; ++i) {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
        if (m_board[nr][nc] == color) count++; else break;
    }
    return count;
}

// [FIX] 跨4方向取最大连子数（原来在循环里被覆盖，不是取max）
int gomoku::lineMaxConsecutive(int r, int c, Stone color) const
{
    int best = 1;
    for (int d = 0; d < 4; ++d)
        best = qMax(best, countConsecutive(r, c, DIRS[d][0], DIRS[d][1], color));
    return best;
}

// [FIX] 单点连五判定，供搜索节点使用（O(1)，不再全盘扫）
bool gomoku::isWinAt(int r, int c, Stone color) const
{
    return lineMaxConsecutive(r, c, color) >= 5;
}

// ==================== 一步杀（只在根节点调用） ====================

bool gomoku::hasWinningMove(Stone color, QPoint& move)
{
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_abort) return false;
            if (m_board[r][c] != EMPTY) continue;
            m_board[r][c] = color;
            bool win = isWinAt(r, c, color);
            m_board[r][c] = EMPTY;
            if (win) { move = QPoint(r, c); return true; }
        }
    return false;
}

// ==================== 根节点 ====================

QPoint gomoku::findBestMove()
{
    m_abort = false;  // 新的一轮搜索，重置终止标志
    m_timer.start();
    // 一步杀 / 堵杀（根节点才做全盘扫，可接受）
    QPoint winMove;
    if (hasWinningMove(m_aiColor, winMove)) return winMove;
    QPoint oppWinMove;
    if (hasWinningMove(m_enemyColor, oppWinMove)) return oppWinMove;

    if (m_difficulty == NORMAL) 
    {
        QVector<QPoint> candidates;
        getAllCandidateMoves(candidates);
        if (candidates.isEmpty()) return QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2);

        int bestScore = -1;
        QVector<QPoint> bestMoves;
        for (const QPoint& pt : candidates) {
            if (m_abort) break;
            int atk  = evaluatePoint(pt.x(), pt.y(), m_aiColor);
            int def  = evaluatePoint(pt.x(), pt.y(), m_enemyColor);
            int total = atk * 12 + def * 10;
            if (total > bestScore) { bestScore = total; bestMoves.clear(); bestMoves.append(pt); }
            else if (total == bestScore) bestMoves.append(pt);
        }
        if (bestMoves.isEmpty()) return candidates.first();
        return bestMoves[QRandomGenerator::global()->bounded(bestMoves.size())];
    }

    // ===== 困难模式：迭代加深 + 时间控制 =====

    m_timer.start();
    m_timeLimitMs = 2500;
    m_nodesSearched = 0;
    m_abort = false;

    QPoint bestMove(BOARD_SIZE / 2, BOARD_SIZE / 2);
    int maxDepth = getDynamicDepth();

    for (int d = 1; d <= maxDepth; ++d) 
    {
        if (m_abort) break;
        if (m_timer.elapsed() > m_timeLimitMs) break;   

        QPoint curBest(-1, -1);
        int alpha = INT_MIN + 1;
        int beta  = INT_MAX - 1;
        int bestScore = INT_MIN;

        QVector<QPoint> moves = generateOrderedMoves(m_aiColor, bestMove);
        if (moves.isEmpty()) return bestMove;

        bool completed = true;
        for (const QPoint& mv : moves) 
        {
            if (m_abort) { completed = false; break; }
            int r = mv.x(), c = mv.y();
            makeMove(r, c, m_aiColor);          
            
            int score = -negamax(d - 1, -beta, -alpha, m_enemyColor);
            
            unmakeMove(r, c);                     

            if (m_abort) { completed = false; break; }
            if (score > bestScore) { bestScore = score; curBest = mv; }
            alpha = qMax(alpha, score);
        }
        // [FIX] 只有完整跑完一轮才采纳，绝不返回半成品
        if (!completed || curBest.x() == -1) break;
        bestMove = curBest;

        // 兜底：本轮已找到必胜，不必再加深
        if (bestScore >= SCORE_WIN_BASE - 100000) break;
    }
    return bestMove;
}

// ==================== Negamax ====================

int gomoku::negamax(int depth, int alpha, int beta, Stone color)
{
    m_nodesSearched++;

    // Check the deadline periodically without timing every search node.
    if ((m_nodesSearched & 255) == 0 && m_timer.elapsed() > m_timeLimitMs) {
        m_abort = true;
        return 0;
    }

    // [FIX] 只判定"最后一手是否成五"，不再全盘扫、不再搞反方向
    if (!m_moveHistory.isEmpty()) {
        QPoint last = m_moveHistory.last();
        Stone lc = m_board[last.x()][last.y()];
        if (lc != EMPTY && isWinAt(last.x(), last.y(), lc)) {
            return (lc == color) ? (SCORE_WIN_BASE - (10 - depth) * 1000)
                                 : (-SCORE_WIN_BASE + (10 - depth) * 1000);
        }
    }

    if (depth <= 0)
        return quiescence(alpha, beta, 0, color);     // [FIX] 接上静止搜索

    const int originalAlpha = alpha;
    const int originalBeta = beta;
    QPoint bestMove(-1, -1);
    const quint64 key = m_positionKey ^ (color == BLACK
        ? Q_UINT64_C(0x6a09e667f3bcc909) : Q_UINT64_C(0xbb67ae8584caa73b))
        ^ (m_aiColor == BLACK
        ? Q_UINT64_C(0x3c6ef372fe94f82b) : Q_UINT64_C(0xa54ff53a5f1d36f1));
    TranspositionEntry& entry = m_transpositionTable[key & (m_transpositionTable.size() - 1)];
    QPoint preferredMove(-1, -1);
    if (entry.depth >= 0 && entry.key == key) {
        preferredMove = entry.bestMove;
        if (entry.depth >= depth) {
            if (entry.bound == 1) return entry.score;
            if (entry.bound == 2) alpha = qMax(alpha, entry.score);
            else if (entry.bound == 3) beta = qMin(beta, entry.score);
            if (alpha >= beta) return entry.score;
        }
    }

    QVector<QPoint> moves = generateOrderedMoves(color, preferredMove);
    if (moves.isEmpty())
        return evaluateBoardForColor(color);

    for (const QPoint& mv : moves) {
        int r = mv.x(), c = mv.y();
        Stone nextColor = (color == m_aiColor) ? m_enemyColor : m_aiColor;
        
        makeMove(r, c, color);                  // ← 替换直接赋值
        int score = -negamax(depth - 1, -beta, -alpha, nextColor);
        unmakeMove(r, c);                       // ← 替换直接恢复

        if (m_abort) return 0;
        if (score >= beta) {
            m_historyTable[r][c] = qMin(10000, m_historyTable[r][c] + depth * depth);
            if (depth >= 0 && depth < 64) {
                if (m_killerTable[depth][0] != mv) {
                    m_killerTable[depth][1] = m_killerTable[depth][0];
                    m_killerTable[depth][0] = mv;
                }
            }
            if (qAbs(beta) < SCORE_WIN_BASE - 100000
                && (entry.depth < depth || entry.key == key)) {
                entry.key = key;
                entry.score = beta;
                entry.depth = depth;
                entry.bound = 2;
                entry.bestMove = mv;
            }
            return beta;
        }
        if (score > alpha) { alpha = score; bestMove = mv; }
    }
    if (bestMove.x() != -1)
        m_historyTable[bestMove.x()][bestMove.y()] =
            qMin(10000, m_historyTable[bestMove.x()][bestMove.y()] + depth * depth);

    if (qAbs(alpha) < SCORE_WIN_BASE - 100000
        && (entry.depth < depth || entry.key == key)) {
        entry.key = key;
        entry.score = alpha;
        entry.depth = depth;
        entry.bound = alpha <= originalAlpha ? 3 : (alpha >= originalBeta ? 2 : 1);
        entry.bestMove = bestMove;
    }

    return alpha;
}

// ==================== 静止搜索（修复：阈值降低 + 真正被调） ====================

int gomoku::quiescence(int alpha, int beta, int qdepth, Stone color)
{
    ++m_nodesSearched;
    if ((m_nodesSearched & 255) == 0 && m_timer.elapsed() > m_timeLimitMs) {
        m_abort = true;
        return alpha;
    }

    int standPat = evaluateBoardForColor(color);
    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;
    if (qdepth >= 4) return alpha;                    // 限制静止层深度，防爆炸

    Stone opponent = (color == m_aiColor) ? m_enemyColor : m_aiColor;
    QVector<QPoint> moves = generateOrderedMoves(color);

    int searchedForcing = 0;
    int searchedQuiet = 0;
    for (const QPoint& mv : moves) {
        int r = mv.x(), c = mv.y();
        int moveScore = evaluatePoint(r, c, color);
        int opponentScore = evaluatePoint(r, c, opponent);
        const bool forcing = moveScore >= 10000 || opponentScore >= 10000;
        if (moveScore < 500 && opponentScore < 10000) continue;
        if (forcing && searchedForcing >= 8) continue;
        if (!forcing && searchedQuiet >= 4) continue;

        makeMove(r, c, color);
        if (moveScore >= SCORE_FIVE) {
            unmakeMove(r, c);
            return SCORE_WIN_BASE - qdepth * 1000;
        }
        int score = -quiescence(-beta, -alpha, qdepth + 1, opponent);
        unmakeMove(r, c);

        if (m_abort) return alpha;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;    
        if (forcing) ++searchedForcing;
        else ++searchedQuiet;
    }
    return alpha;
}

// ==================== 着法生成 ====================
QVector<QPoint> gomoku::generateOrderedMoves(Stone color, const QPoint& preferredMove)
{
    QVector<QPoint> uniq;
    collectCandidates(uniq);

    if (uniq.isEmpty()) {
        uniq.append(QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2));
        return uniq;
    }

    // 禁手过滤
    if (color == BLACK && color == m_aiColor) {
        QVector<QPoint> filtered;
        for (const QPoint& pt : uniq)
            if (!isForbiddenMove(pt.x(), pt.y(), BLACK)) filtered.append(pt);
        if (!filtered.isEmpty()) uniq = filtered;
    }

    Stone opponent = (color == m_aiColor) ? m_enemyColor : m_aiColor;
    struct MoveScore { QPoint pt; int sc; };
    QVector<MoveScore> scored;
    scored.reserve(uniq.size());

    for (const QPoint& pt : uniq) {
        int s = evaluatePoint(pt.x(), pt.y(), color)
              + (int)(evaluatePoint(pt.x(), pt.y(), opponent) * 0.9)
              + m_historyTable[pt.x()][pt.y()] * 1000;
        if (pt == preferredMove) s += 100000000;
        if (m_killerTable[0][0] == pt || m_killerTable[1][0] == pt) s += 3000;
        scored.append({pt, s});
    }

    std::sort(scored.begin(), scored.end(),
              [](const MoveScore& a, const MoveScore& b) { return a.sc > b.sc; });

    int limit = qMin(scored.size(), 10);  // 宽度从12缩到10
    QVector<QPoint> result;
    result.reserve(limit);
    for (int i = 0; i < limit; ++i) result.append(scored[i].pt);
    return result;
}

// ==================== 局面评估 ====================
int gomoku::evaluateBoardForColor(Stone color)
{
Stone enemy = (color == m_aiColor) ? m_enemyColor : m_aiColor;
    int myTotal = 0, enemyTotal = 0;

    for (const QPoint& p : m_activeCands) {
        if (!m_candFlag[p.x()][p.y()]) continue;
        int r = p.x(), c = p.y();
        if (m_board[r][c] != EMPTY) continue;
        myTotal    += evaluatePoint(r, c, color);
        enemyTotal += evaluatePoint(r, c, enemy);
    }
    return myTotal - (int)(enemyTotal * 1.1);
}

int gomoku::evaluateLine(int r, int c, int dr, int dc, Stone color)
{
    int fwd = 0, bwd = 0;
    bool fOpen = false, bOpen = false;

    for (int i = 1; i <= 4; ++i) {
        int nr = r + dr * i, nc = c + dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
        if (m_board[nr][nc] == color) fwd++;
        else if (m_board[nr][nc] == EMPTY) { fOpen = true; break; }
        else break;
    }
    for (int i = 1; i <= 4; ++i) {
        int nr = r - dr * i, nc = c - dc * i;
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
        if (m_board[nr][nc] == color) bwd++;
        else if (m_board[nr][nc] == EMPTY) { bOpen = true; break; }
        else break;
    }

    int total = 1 + fwd + bwd;
    bool both = fOpen && bOpen;
    bool one  = fOpen || bOpen;

    if (total >= 5) return SCORE_FIVE;
    if (total == 4) return both ? 100000 : (one ? 10000 : 0);
    if (total == 3) return both ? 5000   : (one ? 500   : 0);
    if (total == 2) return both ? 200    : (one ? 50    : 0);
    return both ? 10 : (one ? 2 : 0);
}

int gomoku::evaluatePoint(int r, int c, Stone color)
{
    int colorIdx = (color == m_aiColor) ? 0 : 1;
    if (m_ptCacheValid[r][c][colorIdx]) {
        return m_ptCache[r][c][colorIdx];
    }

    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) return 0;
    if (m_board[r][c] != EMPTY) return 0;

    int score = 0;
    for (int d = 0; d < 4; ++d)
        score += evaluateLine(r, c, DIRS[d][0], DIRS[d][1], color);

    int cr = BOARD_SIZE / 2, cc = BOARD_SIZE / 2;
    int dist = qMax(qAbs(r - cr), qAbs(c - cc));
    if (dist <= 3) score += (4 - dist) * 3;

    m_ptCache[r][c][colorIdx] = score;
    m_ptCacheValid[r][c][colorIdx] = true;

    return score;
}

// ==================== 动态深度 ====================

int gomoku::getDynamicDepth() const
{
    int moves = m_moveHistory.size();
    if (moves < 6)  return 3;
    if (moves < 20) return 5;
    if (moves < 40) return 6;
    return 7;
}

// ==================== 普通模式辅助 ====================

bool gomoku::wouldFormFour(int r, int c, Stone color) const
{
    for (int d = 0; d < 4; ++d)
        if (countConsecutive(r, c, DIRS[d][0], DIRS[d][1], color) >= 4)
            return true;
    return false;
}

void gomoku::getAllCandidateMoves(QVector<QPoint>& candidates)
{
    candidates.clear();
    QSet<qint64> seen;

    QVector<QPoint> urgent;
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] != EMPTY) continue;
            m_board[r][c] = m_aiColor;
            bool u = wouldFormFour(r, c, m_aiColor);
            m_board[r][c] = EMPTY;
            if (!u) {
                m_board[r][c] = m_enemyColor;
                u = wouldFormFour(r, c, m_enemyColor);
                m_board[r][c] = EMPTY;
            }
            if (u) urgent.append(QPoint(r, c));
        }

    if (!urgent.isEmpty()) { candidates = urgent; return; }

    int range = (m_moveHistory.size() < 10) ? 2 : 3;
    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] == EMPTY) continue;
            for (int dr = -range; dr <= range; ++dr)
                for (int dc = -range; dc <= range; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;
                    if (m_board[nr][nc] != EMPTY) continue;
                    qint64 key = (qint64)nr * BOARD_SIZE + nc;
                    if (seen.contains(key)) continue;
                    seen.insert(key);
                    candidates.append(QPoint(nr, nc));
                }
        }
}

// ==================== 禁手 ====================

bool gomoku::isForbiddenMove(int r, int c, Stone color) const
{
if (color != BLACK) return false;
    gomoku* self = const_cast<gomoku*>(this);
    self->makeMove(r, c, BLACK);          
    bool forbidden = checkLongConnect(r, c, BLACK)
                  || checkDoubleLiveThree(r, c, BLACK)
                  || checkDoubleFour(r, c, BLACK);
    self->unmakeMove(r, c);               
    return forbidden;
}

bool gomoku::checkLongConnect(int r, int c, Stone color) const
{
    for (int d = 0; d < 4; ++d) {
        int cnt = 1;
        for (int i = 1; i <= 5; ++i) {
            int nr = r + DIRS[d][0] * i, nc = c + DIRS[d][1] * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) cnt++; else break;
        }
        for (int i = 1; i <= 5; ++i) {
            int nr = r - DIRS[d][0] * i, nc = c - DIRS[d][1] * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) cnt++; else break;
        }
        if (cnt > 5) return true;
    }
    return false;
}

bool gomoku::checkDoubleLiveThree(int r, int c, Stone color) const
{
    int liveThree = 0;
    for (int d = 0; d < 4; ++d) {
        int dr = DIRS[d][0], dc = DIRS[d][1];
        int fwd = 0, bwd = 0;
        bool fO = false, bO = false;
        for (int i = 1; i <= 3; ++i) {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) fwd++;
            else if (m_board[nr][nc] == EMPTY) { fO = true; break; }
            else break;
        }
        for (int i = 1; i <= 3; ++i) {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) bwd++;
            else if (m_board[nr][nc] == EMPTY) { bO = true; break; }
            else break;
        }
        if (fwd + bwd >= 2 && fO && bO) liveThree++;
    }
    return liveThree >= 2;
}

bool gomoku::checkDoubleFour(int r, int c, Stone color) const
{
    int four = 0;
    for (int d = 0; d < 4; ++d) {
        int dr = DIRS[d][0], dc = DIRS[d][1];
        int fwd = 0, bwd = 0;
        bool fO = false, bO = false;
        for (int i = 1; i <= 4; ++i) {
            int nr = r + dr * i, nc = c + dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) fwd++;
            else if (m_board[nr][nc] == EMPTY) { fO = true; break; }
            else break;
        }
        for (int i = 1; i <= 4; ++i) {
            int nr = r - dr * i, nc = c - dc * i;
            if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) break;
            if (m_board[nr][nc] == color) bwd++;
            else if (m_board[nr][nc] == EMPTY) { bO = true; break; }
            else break;
        }
        if (fwd + bwd >= 3 && (fO || bO)) four++;
    }
    return four >= 2;
}

void gomoku::addCandidate(int r, int c)
{
    if (m_board[r][c] != EMPTY) return;
    if (m_candFlag[r][c]) return;
    m_candFlag[r][c] = true;
    m_candIndex[r][c] = m_activeCands.size();
    m_activeCands.append(QPoint(r, c));
}

void gomoku::removeCandidate(int r, int c)
{
    if (!m_candFlag[r][c]) return;
    int index = m_candIndex[r][c];
    QPoint last = m_activeCands.last();
    if (index != m_activeCands.size() - 1) {
        m_activeCands[index] = last;
        m_candIndex[last.x()][last.y()] = index;
    }
    m_activeCands.removeLast();
    m_candFlag[r][c] = false;
    m_candIndex[r][c] = -1;
}

void gomoku::rebuildCandidates()
{
    memset(m_candFlag, 0, sizeof(m_candFlag));
    memset(m_candRow, 0, sizeof(m_candRow));
    memset(m_candIndex, 0xff, sizeof(m_candIndex));
    m_activeCands.clear();
    m_pieceCount = 0;

    for (int r = 0; r < BOARD_SIZE; ++r)
        for (int c = 0; c < BOARD_SIZE; ++c) {
            if (m_board[r][c] == EMPTY) continue;
            m_pieceCount++;
            forNeighbors(r, c, 2, [&](int nr, int nc) {
                m_candRow[nr][nc]++;
                addCandidate(nr, nc);
            });
        }
}

void gomoku::collectCandidates(QVector<QPoint>& out) const
{
    out.clear();
    for (const QPoint& p : m_activeCands)
        if (m_candFlag[p.x()][p.y()])
            out.append(p);
    if (out.isEmpty())
        out.append(QPoint(BOARD_SIZE / 2, BOARD_SIZE / 2));
}

void gomoku::makeMove(int r, int c, Stone color)
{
    m_board[r][c] = color;
    m_positionKey ^= m_zobrist[r][c][color == BLACK ? 0 : 1];
    m_moveHistory.append(QPoint(r, c));
    m_pieceCount++;
    removeCandidate(r, c);
    forNeighbors(r, c, 2, [&](int nr, int nc) {
        m_candRow[nr][nc]++;
        addCandidate(nr, nc);
    });
    invalidateCache(r, c);
}

void gomoku::unmakeMove(int r, int c)
{
    Stone color = m_board[r][c];
    m_positionKey ^= m_zobrist[r][c][color == BLACK ? 0 : 1];
    m_board[r][c] = EMPTY;
    m_moveHistory.removeLast();
    m_pieceCount--;
    addCandidate(r, c);
    forNeighbors(r, c, 2, [&](int nr, int nc) {
        m_candRow[nr][nc]--;
        if (m_candRow[nr][nc] == 0) removeCandidate(nr, nc);
    });
    invalidateCache(r, c);
}

void gomoku::invalidateCache(int r, int c)
{
    for (int dr = -4; dr <= 4; ++dr)
        for (int dc = -4; dc <= 4; ++dc) {
            int nr = r + dr, nc = c + dc;
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                m_ptCacheValid[nr][nc][0] = false;
                m_ptCacheValid[nr][nc][1] = false;
            }
        }
}
