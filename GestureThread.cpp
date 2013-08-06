#include <QMessageBox>
#include <QDebug>
#include "VirtualTouchScreen.h"
#include "GestureThread.h"
#include "GestureAlgos.h"

class MyPipeline : public UtilPipeline
{
public:
	explicit MyPipeline(VirtualTouchScreen *p) : UtilPipeline(), pres(p)
	{
		//for some reason only default values are accepted
		//EnableImage(PXCImage::IMAGE_TYPE_DEPTH, 320, 240);
	}
	void PXCAPI OnGesture(PXCGesture::Gesture *data)
	{
		switch (data->label)
		{
		case PXCGesture::Gesture::LABEL_NAV_SWIPE_LEFT:
			qDebug() << "Gesture detected: swipe left";
			if (NULL != pres)
			{
				pres->onSwipe(VK_RIGHT);//more natural swipe
			}
			break;
		case PXCGesture::Gesture::LABEL_NAV_SWIPE_RIGHT:
			qDebug() << "Gesture detected: swipe right";
			if (NULL != pres)
			{
				pres->onSwipe(VK_LEFT);
			}
			break;
		default:
			(void)0;
		}
	}
	void PXCAPI OnAlert(PXCGesture::Alert *alert)
	{
		switch (alert->label)
		{
		case PXCGesture::Alert::LABEL_FOV_BOTTOM:
			qDebug() << "The tracking object touches the bottom field of view";
			break;
		case PXCGesture::Alert::LABEL_FOV_LEFT:
			qDebug() << "The tracking object touches the left field of view";
			break;
		case PXCGesture::Alert::LABEL_FOV_RIGHT:
			qDebug() << "The tracking object touches the right field of view";
			break;
		case PXCGesture::Alert::LABEL_FOV_TOP:
			qDebug() << "The tracking object touches the top field of view";
			break;
		case PXCGesture::Alert::LABEL_FOV_OK:
			qDebug() << "The tracking object is within field of view";
			break;
		case PXCGesture::Alert::LABEL_FOV_BLOCKED:
			qDebug() << "The field of view is blocked";
			break;
		case PXCGesture::Alert::LABEL_GEONODE_ACTIVE:
			qDebug() << "The tracking object is found";
			break;
		case PXCGesture::Alert::LABEL_GEONODE_INACTIVE:
			qDebug() << "The tracking object is lost";
			break;
		default:
			(void)0;
		}
	}
private:
	VirtualTouchScreen *pres;
};

GestureThread::GestureThread(VirtualTouchScreen *obj) : QThread(), mainWnd(obj)
{
	setupPipeline();
	connect(this, SIGNAL(moveCursor(int,int)), mainWnd, SLOT(onMoveCursor(int,int)));
	connect(this, SIGNAL(tap(int,int)), mainWnd, SLOT(onTap(int,int)));
	connect(this, SIGNAL(showCoords(int,int)), mainWnd, SLOT(onShowCoords(int,int)));
}

GestureThread::~GestureThread()
{
	delete pipeline;
}

void GestureThread::run()
{
	while (true)
	{
		if(pipeline->AcquireFrame(true))
		{
			gesture = pipeline->QueryGesture();

			PXCGesture::GeoNode handNode;
			if(gesture->QueryNodeData(0, PXCGesture::GeoNode::LABEL_BODY_HAND_PRIMARY,
				&handNode) != PXC_STATUS_ITEM_UNAVAILABLE)
			{
				float imgX = static_cast<float>(handNode.positionImage.x);
				float imgY = static_cast<float>(handNode.positionImage.y);
				float depth = static_cast<float>(handNode.positionWorld.y);

				//filter data
				if (EXIT_FAILURE == mainWnd->gestureAlgos->filterKalman(imgX, imgY)) {
					qDebug() << "error in Kalman filter";
				}
				mainWnd->gestureAlgos->filterBiquad(depth);

				qDebug() << QThread::currentThreadId() << "(x,y,d) = (" << imgX << "," << imgY << "," << depth << ")";

				//move cursor to the new position
				int x = static_cast<int>(imgX);
				int y = static_cast<int>(imgY);
				//emit moveCursor(x, y);//TODO: no window movement
				//check for tap
				if(mainWnd->gestureAlgos->isTap(x, y, depth))
				{
					qDebug() << "tap detected";
					emit tap(x, y);
				}
				//display coordinates if requested
				/*if (mainWnd->showCoords)
				{
					emit showCoords(x, y);
				}*/
				//check hand status
				switch (handNode.opennessState)
				{
				case PXCGesture::GeoNode::Openness::LABEL_OPEN:
					qDebug() << "hand open";
					break;
				case PXCGesture::GeoNode::Openness::LABEL_CLOSE:
					qDebug() << "hand closed";
					break;
				default:
					(void)0;
				}
			}

			// we must release the frame
			pipeline->ReleaseFrame();
		}
	}
}

void GestureThread::setupPipeline()
{
	pipeline = new MyPipeline(mainWnd);
	pipeline->EnableGesture();
	if (pipeline->Init())
	{
		pxcU32 imgWidth;
		pxcU32 imgHeight;
		if (!pipeline->QueryImageSize(PXCImage::IMAGE_TYPE_DEPTH, imgWidth, imgHeight))
		{
			imgWidth = imgHeight = -1;
		}
		mainWnd->gestureAlgos->setImageSize(imgWidth, imgHeight);
	} else
	{
		QMessageBox::warning(NULL, "Presenter Helper", "Cannot init gesture camera");
		::TerminateProcess(::GetCurrentProcess(), EXIT_FAILURE);
	}
}
