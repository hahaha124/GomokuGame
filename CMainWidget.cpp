#include "CMainWidget.h"

CMainWidget::CMainWidget(QWidget *parent) : QWidget(parent)
{
    m_board = new CBoardView(this);

    m_statusLabel = new QLabel("点击「新游戏」开始", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(14); statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);

    m_turnLabel = new QLabel("等待开始", this);
    m_turnLabel->setAlignment(Qt::AlignCenter);
    QFont turnFont = m_turnLabel->font();
    turnFont.setPointSize(11); m_turnLabel->setFont(turnFont);

    m_colorLabel = new QLabel("", this);
    m_colorLabel->setAlignment(Qt::AlignCenter);

    m_diffLabel = new QLabel("难度：", this);
    m_diffCombo = new QComboBox(this);
    m_diffCombo->addItem("🟢 普通", NORMAL);
    m_diffCombo->addItem("🔴 困难", HARD);
    m_diffCombo->setCurrentIndex(0);
    m_board->setDifficulty(NORMAL);

    m_newGameBtn = new QPushButton("🆕 新游戏", this);
    m_undoBtn = new QPushButton("↩️ 悔棋", this);
    QFont btnFont = m_newGameBtn->font();
    btnFont.setPointSize(12);
    m_newGameBtn->setFont(btnFont); m_undoBtn->setFont(btnFont);
    m_newGameBtn->setMinimumHeight(40); m_undoBtn->setMinimumHeight(40);

    QHBoxLayout *diffLayout = new QHBoxLayout;
    diffLayout->addWidget(m_diffLabel);
    diffLayout->addWidget(m_diffCombo);
    diffLayout->addStretch();

    QVBoxLayout *sideLayout = new QVBoxLayout;
    sideLayout->addWidget(m_statusLabel);
    sideLayout->addSpacing(10);
    sideLayout->addLayout(diffLayout);
    sideLayout->addSpacing(10);
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

    connect(m_newGameBtn, &QPushButton::clicked, this, &CMainWidget::onNewGame);
    connect(m_undoBtn, &QPushButton::clicked, this, &CMainWidget::onUndo);
    connect(m_diffCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CMainWidget::onDifficultyChanged);
    connect(m_board, &CBoardView::gameOver, this, &CMainWidget::onGameOver);
    connect(m_board, &CBoardView::turnChanged, this, &CMainWidget::onTurnChanged);
}

void CMainWidget::onNewGame()
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

void CMainWidget::onUndo()
{
    m_board->undo();
    m_turnLabel->setText("已悔棋");
}

void CMainWidget::onGameOver(QString result)
{
    m_statusLabel->setText("游戏结束");
    m_turnLabel->setText(result);
    QMessageBox::information(this, "游戏结束", result);
}

void CMainWidget::onTurnChanged(QString info)
{
    m_turnLabel->setText(info);
}

void CMainWidget::onDifficultyChanged(int index)
{
    Difficulty diff = (Difficulty)m_diffCombo->itemData(index).toInt();
    m_board->setDifficulty(diff);
}
