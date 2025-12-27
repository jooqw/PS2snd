#include "ps2snd.h"
#include "ui_ps2snd.h"
#include "2sf2.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <algorithm>
#include <cstring>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    waveformWidget = new WaveformWidget(this);
    ui->waveformLayout->addWidget(waveformWidget);

    ui->treeWidget->setHeaderLabels({"Item", "Type", "Info"});
    ui->treeWidget->setColumnWidth(0, 250);
    ui->treeWidget->setColumnWidth(1, 100);

    // just in case
    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::on_treeWidget_itemSelectionChanged);
    connect(ui->chkLoop, &QCheckBox::checkStateChanged, this, &MainWindow::on_chkLoop_stateChanged);
}

MainWindow::~MainWindow() {
    if (deviceInit) ma_device_uninit(&device);
    delete ui;
}

void MainWindow::clearProperties() {
    ui->propTable->setRowCount(0);
}

void MainWindow::addProperty(const QString& key, const QString& value) {
    int r = ui->propTable->rowCount();
    ui->propTable->insertRow(r);
    ui->propTable->setItem(r, 0, new QTableWidgetItem(key));
    ui->propTable->setItem(r, 1, new QTableWidgetItem(value));
}

void MainWindow::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    MainWindow* self = (MainWindow*)pDevice->pUserData;
    if (!self || !self->isPlaying || self->playBuffer.empty()) {
        memset(pOutput, 0, frameCount * sizeof(s16));
        return;
    }
    s16* out = (s16*)pOutput;
    size_t samplesToWrite = frameCount;
    size_t totalWritten = 0;
    while (samplesToWrite > 0) {
        size_t available = self->playBuffer.size() - self->playCursor;
        if (available == 0) {
            if (self->loopPlayback) {
                self->playCursor = (self->currentSample.looping && self->currentSample.loop_end > self->currentSample.loop_start)
                ? self->currentSample.loop_start : 0;
                if (self->playBuffer.empty()) break;
                continue;
            } else break;
        }
        size_t toCopy = std::min(samplesToWrite, available);
        memcpy(out + totalWritten, self->playBuffer.data() + self->playCursor, toCopy * sizeof(s16));
        self->playCursor += toCopy;
        totalWritten += toCopy;
        samplesToWrite -= toCopy;
    }
    if (totalWritten < frameCount) {
        memset(out + totalWritten, 0, (frameCount - totalWritten) * sizeof(s16));
        if (!self->loopPlayback) self->isPlaying = false;
    }
}

void MainWindow::on_actionOpen_HD_triggered() {
    QString hdPath = QFileDialog::getOpenFileName(this, "Open HD File", "", "HD Files (*.hd)");
    if (hdPath.isEmpty()) return;

    QString bdPath = hdPath;
    if (bdPath.endsWith(".hd", Qt::CaseInsensitive)) bdPath.replace(bdPath.length() - 3, 3, ".bd");
    else bdPath += ".bd";

    if (!QFileInfo::exists(bdPath)) {
        bdPath = QFileDialog::getOpenFileName(this, "Locate matching BD File", "", "BD Files (*.bd)");
        if (bdPath.isEmpty()) return;
    }

    if (!bdParser.load(bdPath)) {
        QMessageBox::critical(this, "Error", "Failed to load BD file.");
        return;
    }
    if (!hdParser.load(hdPath, currentBank)) {
        QMessageBox::critical(this, "Error", "Failed to load HD file.");
        return;
    }

    ui->treeWidget->clear();
    clearProperties();
    waveformWidget->clear();

    for (const auto& prog : currentBank.programs) {
        QTreeWidgetItem* pItem = new QTreeWidgetItem(ui->treeWidget);
        pItem->setText(0, QString("Program %1").arg(prog->id));
        pItem->setText(1, "Instrument");
        pItem->setText(2, QString("%1 Tones").arg(prog->tones.size()));
        pItem->setData(0, Qt::UserRole, (int)prog->id);
        pItem->setData(0, Qt::UserRole + 1, -1);

        int toneIdx = 0;
        for (const auto& tone : prog->tones) {
            QTreeWidgetItem* tItem = new QTreeWidgetItem(pItem);
            tItem->setText(0, QString("Tone %1 (Key %2-%3)").arg(toneIdx).arg(tone.min_note).arg(tone.max_note));
            tItem->setText(1, "Sample");
            tItem->setText(2, QString("VAG: 0x%1").arg(tone.bd_offset, 0, 16));

            tItem->setData(0, Qt::UserRole, (int)prog->id);
            tItem->setData(0, Qt::UserRole + 1, toneIdx);

            toneIdx++;
        }
    }

    ui->statusbar->showMessage(QString("Loaded %1 programs.").arg(currentBank.programs.size()));
}

void MainWindow::on_treeWidget_itemSelectionChanged() {
    on_btnStop_clicked();
    auto items = ui->treeWidget->selectedItems();
    if (items.empty()) return;

    QTreeWidgetItem* item = items[0];
    int progId = item->data(0, Qt::UserRole).toInt();
    int toneIdx = item->data(0, Qt::UserRole + 1).toInt();

    std::shared_ptr<Program> prog = nullptr;
    for (auto& p : currentBank.programs) {
        if (p->id == (u32)progId) { prog = p; break; }
    }
    if (!prog) return;

    clearProperties();

    if (toneIdx == -1) {
        currentSample = {};
        waveformWidget->clear();

        addProperty("Program ID", QString::number(prog->id));
        addProperty("Name", QString::fromStdString(prog->name));
        addProperty("Master Volume", QString::number(prog->master_vol));
        addProperty("Master Pan", QString::number(prog->master_pan));
        addProperty("Is Layered?", prog->is_layered ? "Yes" : "No");
        addProperty("Tone Count", QString::number(prog->tones.size()));
    }
    else {
        if (toneIdx >= prog->tones.size()) return;
        const auto& tone = prog->tones[toneIdx];

        auto raw = bdParser.get_adpcm_block(tone.bd_offset);
        currentSample = BDParser::decode_adpcm(raw, tone.sample_rate);

        waveformWidget->setData(currentSample.pcm, currentSample.looping, currentSample.loop_start, currentSample.loop_end);

        addProperty("Key Range", QString("%1 - %2").arg(tone.min_note).arg(tone.max_note));
        addProperty("Root Key", QString::number(tone.root_key));
        addProperty("Pitch Fine", QString::number(tone.pitch_fine));
        addProperty("Volume", QString::number(tone.volume));
        addProperty("Pan", QString::number(tone.pan));

        addProperty("ADSR 1 (Raw)", QString("0x%1").arg(tone.adsr1, 4, 16, QChar('0')).toUpper());
        addProperty("ADSR 2 (Raw)", QString("0x%1").arg(tone.adsr2, 4, 16, QChar('0')).toUpper());

        addProperty("Attack Mode", (tone.adsr1 & 0x8000) ? "Exponential" : "Linear");
        addProperty("Sustain Level", QString::number(tone.adsr1 & 0xF));

        addProperty("VAG Offset", QString("0x%1").arg(tone.bd_offset, 8, 16, QChar('0')).toUpper());
        addProperty("Sample Rate", QString::number(tone.sample_rate) + " Hz");
        addProperty("Looping", currentSample.looping ? "Yes" : "No");
        if (currentSample.looping) {
            addProperty("Loop Start", QString::number(currentSample.loop_start));
            addProperty("Loop End", QString::number(currentSample.loop_end));
        }
        addProperty("Reverb Enabled", tone.is_reverb_enabled ? "Yes" : "No");
    }
}

void MainWindow::on_btnPlay_clicked() {
    auto items = ui->treeWidget->selectedItems();
    if (items.isEmpty()) return;

    int toneIdx = items[0]->data(0, Qt::UserRole + 1).toInt();
    if (toneIdx == -1) return;

    if (currentSample.pcm.empty()) return;

    playBuffer = currentSample.pcm;
    playCursor = 0;
    isPlaying = true;
    loopPlayback = ui->chkLoop->isChecked();

    if (!deviceInit) {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_s16;
        config.playback.channels = 1;
        config.sampleRate = currentSample.sample_rate;
        config.dataCallback = data_callback;
        config.pUserData = this;
        if (ma_device_init(NULL, &config, &device) == MA_SUCCESS) deviceInit = true;
    }

    if (device.sampleRate != currentSample.sample_rate) {
        ma_device_uninit(&device);
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_s16;
        config.playback.channels = 1;
        config.sampleRate = currentSample.sample_rate;
        config.dataCallback = data_callback;
        config.pUserData = this;
        ma_device_init(NULL, &config, &device);
    }

    if (!ma_device_is_started(&device)) ma_device_start(&device);
}

void MainWindow::on_btnStop_clicked() {
    isPlaying = false;
    playCursor = 0;
    if (deviceInit && ma_device_is_started(&device)) ma_device_stop(&device);
}

void MainWindow::on_chkLoop_stateChanged(int arg1) {
    loopPlayback = (arg1 != 0);
}

void MainWindow::on_actionExportSF2_triggered() {
    if (!currentBank.valid) return;
    QString path = QFileDialog::getSaveFileName(this, "Export SF2", "out.sf2", "SoundFont (*.sf2)");
    if (path.isEmpty()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = Sf2Exporter::exportToSf2(path, currentBank, &bdParser);
    QApplication::restoreOverrideCursor();
    if (ok) QMessageBox::information(this, "Success", "Export done.");
    else QMessageBox::critical(this, "Error", "Export failed.");
}
