#ifndef CMAINWIDGET_H
#define CMAINWIDGET_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMessageBox>
#include "CBoardView.h"

class CMainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CMainWidget(QWidget *parent = nullptr);

public slots:
    void onNewGame();
    void onUndo();
    void onGameOver(QString result);
    void onTurnChanged(QString info);
    void onDifficultyChanged(int index);

private:
    CBoardView *m_board;
    QLabel *m_statusLabel;
    QLabel *m_turnLabel;
    QLabel *m_colorLabel;
    QLabel *m_diffLabel;
    QComboBox *m_diffCombo;
    QPushButton *m_newGameBtn;
    QPushButton *m_undoBtn;
};

#endif // CMAINWIDGET_H
