#ifndef PS2SND_H
#define PS2SND_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include "main.h"
#include "hd.h"
#include "bd.h"
#include "miniaudio.h"
#include "waveform.h"

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionOpen_HD_triggered();
    void on_actionExportSF2_triggered();
    void on_treeWidget_itemSelectionChanged();
    void on_btnPlay_clicked();
    void on_btnStop_clicked();
    void on_chkLoop_stateChanged(int arg1);

private:
    void addProperty(const QString& key, const QString& value);
    void clearProperties();

    Ui::MainWindow *ui;
    WaveformWidget *waveformWidget;

    HDParser hdParser;
    BDParser bdParser;
    Bank currentBank;
    DecodedSample currentSample;

    ma_device device;
    bool deviceInit = false;

    std::vector<s16> playBuffer;
    size_t playCursor = 0;
    bool isPlaying = false;
    bool loopPlayback = false;

    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};

#endif // PS2SND_H
