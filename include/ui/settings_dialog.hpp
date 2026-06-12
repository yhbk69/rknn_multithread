#ifndef SETTINGS_DIALOG_HPP
#define SETTINGS_DIALOG_HPP

#include <QDialog>
#include <QString>

class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;

struct SettingsResult {
    float confThreshold{};
    float nmsThreshold{};
    int websocketPort{};
    int alertPort{};
    int streamPort{};
    int ackTimeoutMs{};
    int alertCooldownMs{};
    int ringBufferFrames{};
    QString modelPath;
    QString outputDir;
    QString recordDir;
};

class SettingsDialog : public QDialog {
public:
    explicit SettingsDialog(const SettingsResult& current, QWidget* parent = nullptr);

    SettingsResult result() const;

private:
    QDoubleSpinBox* confSpin_;
    QDoubleSpinBox* nmsSpin_;
    QSpinBox* wsPortSpin_;
    QSpinBox* alertPortSpin_;
    QSpinBox* streamPortSpin_;
    QSpinBox* ackSpin_;
    QSpinBox* cooldownSpin_;
    QSpinBox* ringSpin_;
    QLineEdit* modelEdit_;
    QLineEdit* outputEdit_;
    QLineEdit* recordEdit_;
};

#endif // SETTINGS_DIALOG_HPP
