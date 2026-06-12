#include "mainwindow.hpp"
#include "ui_mainwindow.h"
#include "gui_logger.hpp"
#include "detection_utils.hpp"
#include "video_source.hpp"
#include "camera_manager.hpp"
#include "model_manager.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QCloseEvent>
#include <QAction>
#include <QInputDialog>
#include <filesystem>
#include <QMessageBox><QScreen><QPixmap><QDir><QCloseEvent><QAction><QInputDialog><filesystem>
namespace fs=std::filesystem;
MainWindow::MainWindow(QWidget*p):QMainWindow(p),confThreshold_(0.25f),nmsThreshold_(0.45f){
 ui=new Ui::MainWindow;ui->setupUi(this);
 statusMessageLabel_=new QLabel("Ready",this);fpsLabel_=new QLabel("FPS:--",this);timeLabel_=new QLabel("Time:--",this);
 fpsLabel_->setStyleSheet("margin-right:15px;");timeLabel_->setStyleSheet("margin-right:15px;");
 statusBar()->addWidget(statusMessageLabel_,1);statusBar()->addPermanentWidget(fpsLabel_);statusBar()->addPermanentWidget(timeLabel_);
 cameraListWidget_=new QListWidget(this);cameraListWidget_->setMaximumHeight(150);
 cameraListWidget_->setStyleSheet("QListWidget{background:#1e1e1e;border:1px solid #444;}QListWidget::item{padding:4px 8px;}QListWidget::item:selected{background:#2a3f5f;}");
 auto*rl=qobject_cast<QVBoxLayout*>(ui->rightPanel->layout());if(rl)rl->insertWidget(rl->indexOf(ui->resultTitleLabel)+1,cameraListWidget_);
 connect(cameraListWidget_,&QListWidget::itemClicked,this,&MainWindow::onCameraListClicked);
 auto*bl=qobject_cast<QHBoxLayout*>(ui->buttonLayout);if(bl){
  startDetectBtn_=new QPushButton("Start Detection",this);startDetectBtn_->setStyleSheet("QPushButton{background:#5cb85c;color:white;font-weight:bold;padding:6px 16px;}QPushButton:disabled{background:#555;color:#888;}");
  startDetectBtn_->setEnabled(false);bl->addWidget(startDetectBtn_);connect(startDetectBtn_,&QPushButton::clicked,this,&MainWindow::onStartDetection);}
 modelManager_=std::make_unique<ModelManager>();cameraManager_=std::make_unique<CameraManager>();
 connect(ui->browseModelBtn,&QPushButton::clicked,this,&MainWindow::onBrowseModel);connect(ui->loadModelBtn,&QPushButton::clicked,this,&MainWindow::onLoadModel);
 connect(ui->reloadModelBtn,&QPushButton::clicked,this,&MainWindow::onReloadModel);connect(ui->stopBtn,&QPushButton::clicked,this,&MainWindow::onStopProcessing);
 connect(ui->confSlider,&QSlider::valueChanged,this,&MainWindow::onConfThresholdChanged);connect(ui->nmsSlider,&QSlider::valueChanged,this,&MainWindow::onNmsThresholdChanged);
 connect(ui->actionOpenImage,&QAction::triggered,this,&MainWindow::onOpenImage);connect(ui->actionOpenVideo,&QAction::triggered,this,&MainWindow::onOpenVideo);
 connect(ui->actionOpenCamera,&QAction::triggered,this,[this](bool){onOpenCamera(true);});connect(ui->actionExit,&QAction::triggered,this,&QWidget::close);
 connect(ui->actionLoadModel,&QAction::triggered,this,&MainWindow::onLoadModel);updateThresholdLabels();log(GuiLogger::CAT_SYSTEM,"Started");
}
MainWindow::~MainWindow(){stopAllCameras();delete ui;}
void MainWindow::log(const QString&c,const QString&m){GuiLogger::log(ui->logTextEdit,c,m);}
void MainWindow::updateThresholdLabels(){ui->confValueLabel->setText(QString::number(confThreshold_,'f',2));ui->nmsValueLabel->setText(QString::number(nmsThreshold_,'f',2));}
void MainWindow::onBrowseModel(){QString p=QFileDialog::getOpenFileName(this,"Select ONNX","","ONNX(*.onnx);;All(*.*)");if(!p.isEmpty())ui->modelPathEdit->setText(p);}
void MainWindow::onLoadModel(){try{modelManager_->setCallbacks({[this](auto&c,auto&m){log(c,m);},[this](auto&m){log(GuiLogger::CAT_ERROR,m);}});
 bool ok=modelManager_->load(ui->modelPathEdit->text().toStdString());if(ok){if(startDetectBtn_)startDetectBtn_->setEnabled(true);log(GuiLogger::CAT_MODEL,"Model loaded");}}catch(std::exception&e){log(GuiLogger::CAT_ERROR,QString("Load failed:")+e.what());}}
void MainWindow::onReloadModel(){if(modelManager_)modelManager_->reload(ui->modelPathEdit->text().toStdString());}
void MainWindow::onOpenImage(){QString p=QFileDialog::getOpenFileName(this,"Open Image","","Images(*.jpg *.png)");if(p.isEmpty())return;processSingleImage(p.toStdString());}
void MainWindow::onOpenVideo(){QString p=QFileDialog::getOpenFileName(this,"Open Video","","Videos(*.mp4 *.avi)");if(p.isEmpty())return;stopAllCameras();startVideoWorker(p);}
void MainWindow::onOpenCamera(bool){stopAllCameras();startCameraWorker(0,"Camera 0","0");}
void MainWindow::onStartDetection(){/*stub*/}
void MainWindow::onStopProcessing(){stopAllCameras();}
void MainWindow::onConfThresholdChanged(int v){confThreshold_=v/100.0f;updateThresholdLabels();}
void MainWindow::onNmsThresholdChanged(int v){nmsThreshold_=v/100.0f;updateThresholdLabels();}
void MainWindow::onFrameProcessed(int cid,QImage img,std::vector<Detection> dets,double ms){QPixmap p=QPixmap::fromImage(img);ui->displayLabel->setPixmap(p.scaled(ui->displayLabel->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
 fpsLabel_->setText(QString("FPS:%1").arg(1000.0/std::max(ms,1.0),0,'f',1));timeLabel_->setText(QString("Time:%1ms").arg(ms,0,'f',1));
 QString t;for(auto&d:dets)t+=QString::fromStdString(detectionToString(d))+"\n";if(dets.empty())t="None";ui->logTextEdit->setPlainText(t);
 if(activeDisplayCamera_!=cid){activeDisplayCamera_=cid;}}
void MainWindow::onWorkerFinished(int cid){log(GuiLogger::CAT_SYSTEM,QString("Worker %1 finished").arg(cid));}
void MainWindow::onWorkerError(int cid,const QString&m){log(GuiLogger::CAT_ERROR,QString("Worker %1: ").arg(cid)+m);}
void MainWindow::onAddCamera(){/*TODO*/}
void MainWindow::onRemoveCamera(int cid){stopCamera(cid);refreshCameraList();}
void MainWindow::onOpenFolder(){QString d=QDir::currentPath()+"/output";QDir().mkpath(d);QDesktopServices::openUrl(QUrl::fromLocalFile(d));}
void MainWindow::onSettings(){/*TODO: settings dialog*/}
void MainWindow::onBatchInferenceToggled(bool){/*stub*/}
void MainWindow::processSingleImage(const std::string&path){/*TODO*/}
void MainWindow::startCameraWorker(int cid,const QString&name,const QString&src){
 if(!modelManager_||!modelManager_->currentEngine()){log(GuiLogger::CAT_ERROR,"Load model first");return;}
 IEngine*eng=modelManager_->currentEngine();cameraManager_->add(cid,new QThread(this),new InferenceWorker(eng,cid,name));
 auto*w=cameraManager_->worker(cid);connect(w,&InferenceWorker::frameProcessed,this,&MainWindow::onFrameProcessed);
 connect(w,&InferenceWorker::finished,this,&MainWindow::onWorkerFinished);connect(w,&InferenceWorker::errorOccurred,this,&MainWindow::onWorkerError);
 auto*th=cameraManager_->thread(cid);w->moveToThread(th);
 connect(th,&QThread::started,this,[=](){auto src2=std::make_unique<CameraVideoSource>(cid,src);w->process(std::move(src2),confThreshold_,nmsThreshold_,Config::INPUT_WIDTH,Config::INPUT_HEIGHT);});
 th->start();cameraSources_[cid]=src;cameraAliases_[cid]=name;activeDisplayCamera_=cid;refreshCameraList();
 log(GuiLogger::CAT_SYSTEM,QString("Camera %1 started").arg(cid));}
void MainWindow::startVideoWorker(const QString&fp){
 if(!modelManager_||!modelManager_->currentEngine()){log(GuiLogger::CAT_ERROR,"Load model first");return;}
 IEngine*eng=modelManager_->currentEngine();int vid=999;cameraManager_->add(vid,new QThread(this),new InferenceWorker(eng,vid,"video"));
 auto*w=cameraManager_->worker(vid);connect(w,&InferenceWorker::frameProcessed,this,&MainWindow::onFrameProcessed);
 connect(w,&InferenceWorker::finished,this,&MainWindow::onWorkerFinished);connect(w,&InferenceWorker::errorOccurred,this,&MainWindow::onWorkerError);
 auto*th=cameraManager_->entries_.value(vid).thread;w->moveToThread(th);
 connect(th,&QThread::started,this,[=](){auto src=std::make_unique<FileVideoSource>(fp);w->process(std::move(src),confThreshold_,nmsThreshold_,Config::INPUT_WIDTH,Config::INPUT_HEIGHT);});
 th->start();activeDisplayCamera_=vid;log(GuiLogger::CAT_SYSTEM,"Video started");}
void MainWindow::stopCamera(int cid){cameraManager_->stop(cid);cameraSources_.erase(cid);cameraAliases_.erase(cid);}
void MainWindow::stopAllCameras(){if(cameraManager_)cameraManager_->stopAll();}
void MainWindow::refreshCameraList(){cameraListWidget_->clear();for(auto&[id,alias]:cameraAliases_){auto*it=new QListWidgetItem(QString("[%1] %2").arg(id).arg(alias));it->setData(Qt::UserRole,id);cameraListWidget_->addItem(it);}}
void MainWindow::onCameraListClicked(QListWidgetItem*it){int id=it->data(Qt::UserRole).toInt();activeDisplayCamera_=id;}
void MainWindow::closeEvent(QCloseEvent*e){stopAllCameras();e->accept();}
void MainWindow::loadRuntimeConfig(){}void MainWindow::saveRuntimeConfig(){}
void MainWindow::onAlertSaved(int,const QString&,const QString&,const QString&){}void MainWindow::onStartRecording(){}void MainWindow::onStopRecording(){}
void MainWindow::onViewRecordings(){}void MainWindow::onClearOldRecordings(){}