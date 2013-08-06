#include <QDebug>
#include <QDesktopWidget>
#include <QBitmap>
#include <QMessageBox>
#include <QApplication>
#include <QAction>
#include <QSettings>
#include "GestureThread.h"
#include "GestureAlgos.h"
#include "VirtualTouchScreen.h"
//#include "ConfigDialog.h"

#define APPLICATION_NAME "Virtual Touch Screen"

VirtualTouchScreen::VirtualTouchScreen(QWidget *parent)
	: QMainWindow(parent),
	gestureThread(NULL),
	pointerSize(POINTER_SIZE),
	offsetX(OFFSET_X), offsetY(OFFSET_Y),
	scaleFactor(SCALE_FACTOR_x100/100.0),
	config(NULL)
{
	setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);

	loadSettings();
	//loadPointer(pointerIconPath, pointerSize);

	//get screen size
	QDesktopWidget *desktop = QApplication::desktop();
	QRect geom = desktop->availableGeometry(0);//first screen

	//init gesture algosithms
	gestureAlgos = GestureAlgos::instance();
	gestureAlgos->setScreenSize(geom.width(), geom.height());
	gestureAlgos->setCorrectionFactors(scaleFactor, offsetX, offsetY);

	//config = new ConfigDialog(NULL, this);//screen size must be set

	setupActions();

	qDebug() << QThread::currentThreadId() << "starting gesture thread";
	gestureThread = new GestureThread(this);
	gestureThread->start();
}

VirtualTouchScreen::~VirtualTouchScreen()
{
	gestureThread->terminate();
	gestureThread->wait();
	delete gestureThread;
	delete config;
	saveSettings();
}

void VirtualTouchScreen::setupActions()
{
	QAction *menuAction = new QAction(this);
	menuAction->setShortcut(QKeySequence("F2"));
	connect(menuAction, SIGNAL(triggered(bool)), this, SLOT(showMenu()));
	addAction(menuAction);

	QAction *quitAction = new QAction(this);
	quitAction->setShortcut(QKeySequence("Ctrl+q"));
	connect(quitAction, SIGNAL(triggered(bool)), qApp, SLOT(quit()));
	addAction(quitAction);

	QAction *helpAction = new QAction(this);
	helpAction->setShortcut(QKeySequence("F1"));
	connect(helpAction, SIGNAL(triggered(bool)), this, SLOT(showHelp()));
	addAction(helpAction);

	QAction *hideAction = new QAction(this);
	hideAction->setShortcut(QKeySequence("F3"));
	connect(hideAction, SIGNAL(triggered(bool)), this, SLOT(showMinimized()));
	addAction(hideAction);
}

void VirtualTouchScreen::showMenu()
{
	if (NULL != config)
	{
		//config->show();
	}
}

void VirtualTouchScreen::showHelp()
{
	QMessageBox::about(NULL, APPLICATION_NAME, 
		"                            Presenter Helper\n\n"
		"                        author: Bogdan Cristea\n"
		"                        e-mail: cristeab@gmail.com\n\n"
		"   Presenter Helper aims at replacing the laser pointer used in presentations "
		"by a Creative Interactive Gesture Camera, so that a pointer can be moved on "
		"the screen while the presenter moves his hand in front of the camera.\n"
		"   Any presentation software can be used as long as slides are changed using "
		"left/right arrow keys. Optionally animations can be activated with the "
		"mouse left click.\n"
		"   The presenter changes the slides using left/right swipes and a single tap "
		"can be used to either activate presentation animations or can be assigned "
		"as left swipe. If needed, the pointer can be completely hidden. "
		"In order to send the key or mouse events to the presentation application, make "
		"sure that the presentation application has the focus during presentations.\n"
		"   The pointer icon (*.jpg and *.png image formats are supported) and "
		"its size are configurable, also the coordinates of the "
		"upper-left corner of the area covered by the pointer. A scaling factor can "
		"be used in order to make sure that the entire screen area is covered by the "
		"pointer. The number of swipes needed to activate a page switch can be changed, "
		"the default value is one, a value of zero disables the swipe gesture, while "
		"a value superior to one can be used if the slide switch happens too often "
		"during presentation. By default pointer coordinates are filtered with a "
		"Kalman filter that can be disabled if needed.\n"
		"   The following shortcut keys are available:\n"
		"- F1: shows this help\n"
		"- F2: shows the configuration dialog\n"
		"- F3: minimizes the pointer\n"
		"- Ctrl+q: quits the application\n"
		"In order to receive the shortcut keys the application needs to have "
		"the focus (see above). In order to have the focus either click on "
		"the application icon on the main toolbar or click the pointer.");
}

void VirtualTouchScreen::onMoveCursor(int x, int y)
{
	//filter position
	float fx = x;
	float fy = y;
	if (EXIT_SUCCESS == gestureAlgos->filterKalman(fx, fy)) {
		x = static_cast<int>(fx);
		y = static_cast<int>(fy);
	} else {
		qDebug() << "Error when using the Kalman filter";
	}
	move(x, y);
}

void VirtualTouchScreen::onTap(int x, int y)
{
	qDebug() << "onTap: mouse event";
	mouse_event(MOUSEEVENTF_LEFTDOWN, static_cast<int>(x), 
		static_cast<int>(y), 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, static_cast<int>(x), 
		static_cast<int>(y), 0, 0);
}

void VirtualTouchScreen::onSwipe(BYTE code)
{
	keybd_event(code & 0xff, 0, 0, 0);
	keybd_event(code & 0xff, 0, KEYEVENTF_KEYUP, 0);
}

void VirtualTouchScreen::onShowCoords(int x, int y)
{
	//config->showCoords(x, y);
}

void VirtualTouchScreen::loadPointer(const QString &path, int size)
{
	QPixmap pix(path);
	if ((size != pix.size().width()) && (0 < size))
	{
		pix = pix.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	if (!pix.isNull()) {
		QPalette p = palette();
		p.setBrush(QPalette::Background, pix);
		setPalette(p);
		QSize size = pix.size();
		resize(pix.size());
		setMask(pix.mask());
	}
	else {
		QMessageBox::warning(this, APPLICATION_NAME, "Cannot load cursor pixmap");
	}
}

#define COMPANY_NAME "Bogdan Cristea"
#define POINTER_ICON_PATH "PointerIconPath"
#define KEY_POINTER_SIZE "PointerSize"
#define KEY_OFFSET_X "OffsetX"
#define KEY_OFFSET_Y "OffsetY"
#define SCALE_FACTOR "ScaleFactor"
#define NB_SWIPES "NbSwipes"
#define TAP_FOR_FORWARD_SWITCH "TapForForwardSwitch"
#define USE_KALMAN_FILTER "UseKalmanFilter"

void VirtualTouchScreen::loadSettings()
{
	QSettings settings(COMPANY_NAME, APPLICATION_NAME);
	pointerIconPath = settings.value(POINTER_ICON_PATH, ":/icons/green-pointer.png").toString();
	pointerSize = settings.value(KEY_POINTER_SIZE, POINTER_SIZE).toInt();
	offsetX = settings.value(KEY_OFFSET_X, OFFSET_X).toInt();
	offsetY = settings.value(KEY_OFFSET_Y, OFFSET_Y).toInt();
	scaleFactor = settings.value(SCALE_FACTOR, SCALE_FACTOR_x100/100.0).toDouble();
}

void VirtualTouchScreen::saveSettings()
{
	QSettings settings(COMPANY_NAME, APPLICATION_NAME);
	settings.setValue(POINTER_ICON_PATH, pointerIconPath);
	settings.setValue(KEY_POINTER_SIZE, pointerSize);
	settings.setValue(KEY_OFFSET_X, offsetX);
	settings.setValue(KEY_OFFSET_Y, offsetY);
	settings.setValue(SCALE_FACTOR, scaleFactor);
}
