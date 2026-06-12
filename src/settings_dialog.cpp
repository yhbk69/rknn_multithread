#include "settings_dialog.hpp"
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(const SettingsResult& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("运行时设置"));
    setMinimumWidth(480);
    auto* layout = new QFormLayout(this);

    // 阈值
    confSpin_ = new QDoubleSpinBox(this);
    confSpin_->setRange(0.01, 0.99); confSpin_->setSingleStep(0.05);
    confSpin_->setDecimals(2); confSpin_->setValue(current.confThreshold);
    layout->addRow(QStringLiteral("置信度阈值:"), confSpin_);

    nmsSpin_ = new QDoubleSpinBox(this);
    nmsSpin_->setRange(0.01, 0.99); nmsSpin_->setSingleStep(0.05);
    nmsSpin_->setDecimals(2); nmsSpin_->setValue(current.nmsThreshold);
    layout->addRow(QStringLiteral("NMS IOU阈值:"), nmsSpin_);

    // 端口
    wsPortSpin_ = new QSpinBox(this);
    wsPortSpin_->setRange(1024, 65535); wsPortSpin_->setValue(current.websocketPort);
    layout->addRow(QStringLiteral("WebSocket端口:"), wsPortSpin_);

    alertPortSpin_ = new QSpinBox(this);
    alertPortSpin_->setRange(1024, 65535); alertPortSpin_->setValue(current.alertPort);
    layout->addRow(QStringLiteral("告警推送WebSocket端口:"), alertPortSpin_);

    streamPortSpin_ = new QSpinBox(this);
    streamPortSpin_->setRange(1024, 65535); streamPortSpin_->setValue(current.streamPort);
    layout->addRow(QStringLiteral("视频流WebSocket端口:"), streamPortSpin_);

    // 告警参数
    ackSpin_ = new QSpinBox(this);
    ackSpin_->setRange(1000, 60000); ackSpin_->setSingleStep(1000);
    ackSpin_->setValue(current.ackTimeoutMs);
    ackSpin_->setSuffix(" ms");
    layout->addRow(QStringLiteral("ACK超时:"), ackSpin_);

    cooldownSpin_ = new QSpinBox(this);
    cooldownSpin_->setRange(1000, 60000); cooldownSpin_->setSingleStep(1000);
    cooldownSpin_->setValue(current.alertCooldownMs);
    cooldownSpin_->setSuffix(" ms");
    layout->addRow(QStringLiteral("告警冷却:"), cooldownSpin_);

    ringSpin_ = new QSpinBox(this);
    ringSpin_->setRange(10, 300); ringSpin_->setValue(current.ringBufferFrames);
    layout->addRow(QStringLiteral("环形缓冲帧数:"), ringSpin_);

    // 路径
    modelEdit_ = new QLineEdit(current.modelPath, this);
    layout->addRow(QStringLiteral("模型路径:"), modelEdit_);

    outputEdit_ = new QLineEdit(current.outputDir, this);
    layout->addRow(QStringLiteral("输出目录:"), outputEdit_);

    recordEdit_ = new QLineEdit(current.recordDir, this);
    layout->addRow(QStringLiteral("录像目录:"), recordEdit_);

    // 按钮
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SettingsResult SettingsDialog::result() const {
    SettingsResult r;
    r.confThreshold = (float)confSpin_->value();
    r.nmsThreshold = (float)nmsSpin_->value();
    r.websocketPort = wsPortSpin_->value();
    r.alertPort = alertPortSpin_->value();
    r.streamPort = streamPortSpin_->value();
    r.ackTimeoutMs = ackSpin_->value();
    r.alertCooldownMs = cooldownSpin_->value();
    r.ringBufferFrames = ringSpin_->value();
    r.modelPath = modelEdit_->text();
    r.outputDir = outputEdit_->text();
    r.recordDir = recordEdit_->text();
    return r;
}
