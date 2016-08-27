#include "windows.h" // Must include before NuiApi

#include <d2d1.h>
#include "stdafx.h"
#include <strsafe.h>
#include "NuiApi.h"
#include "resource.h"
#include "stdio.h"
#include "math.h"
#include "tinyxml2.h"
#define MAX 16
#define PI 3.14159265
using namespace tinyxml2;



static const float g_JointThickness = 3.0f;
static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;
static const int        cScreenWidth  = 320;
static const int        cScreenHeight = 240;
static const int        cStatusMessageMaxLen = MAX_PATH*2;

INuiSensor* pNuiSensor;
HANDLE nextSkeletonEvent;

 HWND                    m_hWnd;

 bool                    m_bSeatedMode;

//Skeletal Drawing

	ID2D1HwndRenderTarget*   pRenderTarget;
    ID2D1SolidColorBrush*    pBrushJointTracked;
    ID2D1SolidColorBrush*    pBrushJointInferred;
    ID2D1SolidColorBrush*    pBrushBoneTracked;
    ID2D1SolidColorBrush*    pBrushBoneInferred;
    D2D1_POINT_2F            Points[NUI_SKELETON_POSITION_COUNT];


// Direct2D
    ID2D1Factory*           pD2DFactory;
    



//xml

	tinyxml2::XMLDocument doc; //cria o documento xml
	XMLNode * pRoot = doc.NewElement("Root"); // cria um elemento root


typedef struct{
	Vector4 p1A, p2A, p3A, p1O, p2O, p3O; //A = valores atuais
	const char * nomeP1,* nomeP2, *nomeP3;
	float anguloA, anguloO;	//O = valores velhos
}Registro;

typedef struct {
	Registro A[MAX];
	int bone;
}lista;



HRESULT StartKinectST(){
	HRESULT hr;
	nextSkeletonEvent = NULL;


    hr= NuiCreateSensorByIndex(0, &pNuiSensor);
    if (FAILED(hr)){

		printf("Kinect desconectado");
		return hr;}

	// Initialize the Kinect and specify that we'll be using skeleton, colors and depth
	hr = pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON);
	if (SUCCEEDED(hr))	{

		printf("Kinect Conectado");

		// Create an event that will be signaled when skeleton data is available
		nextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

		
		doc.InsertFirstChild(pRoot); // associa o root ao doc


		// Open a skeleton stream to receive skeleton and depth data
		hr = pNuiSensor->NuiSkeletonTrackingEnable(nextSkeletonEvent, 0);

	/*	hr2 = pNuiSensor2->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH,NUI_IMAGE_RESOLUTION_640x480,
			NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE,2,NULL,&depth);*/
	}
    return hr;
}

void SetStatusMessage(WCHAR * szMessage)
{
    SendDlgItemMessageW(m_hWnd, IDC_STATUS, WM_SETTEXT, 0, (LPARAM)szMessage);
}

HRESULT EnsureDirect2DResources()
{
    HRESULT hr = S_OK;

    // If there isn't currently a render target, we need to create one
    if (NULL == pRenderTarget)
    {
        RECT rc;
        GetWindowRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rc );  

        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        D2D1_SIZE_U size = D2D1::SizeU( width, height );
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        rtProps.pixelFormat = D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
        rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

        // Create a Hwnd render target, in order to render to the window set in initialize
        hr = pD2DFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(GetDlgItem( m_hWnd, IDC_VIDEOVIEW), size),
            &pRenderTarget
            );
        if ( FAILED(hr) )
        {
            SetStatusMessage(L"Couldn't create Direct2D render target!");
            return hr;
        }

        //light green
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.27f, 0.75f, 0.27f), &pBrushJointTracked);

        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 1.0f), &pBrushJointInferred);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green, 1.0f), &pBrushBoneTracked);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 1.0f), &pBrushBoneInferred);
    }

    return hr;
}

void DrawBone( const NUI_SKELETON_DATA & skeleton, NUI_SKELETON_POSITION_INDEX jointFrom, NUI_SKELETON_POSITION_INDEX jointTo)


      {
        NUI_SKELETON_POSITION_TRACKING_STATE jointFromState = skeleton.eSkeletonPositionTrackingState[jointFrom];
        NUI_SKELETON_POSITION_TRACKING_STATE jointToState = skeleton.eSkeletonPositionTrackingState[jointTo];

        if (jointFromState == NUI_SKELETON_POSITION_NOT_TRACKED || jointToState == NUI_SKELETON_POSITION_NOT_TRACKED)
        {
          return; // nothing to draw, one of the joints is not tracked
        }

        // Don't draw if both points are inferred
        if (jointFromState == NUI_SKELETON_POSITION_INFERRED || jointToState == NUI_SKELETON_POSITION_INFERRED)
        {
          return; // Draw thin lines if either one of the joints is inferred
        }

        // We assume all drawn bones are inferred unless BOTH joints are tracked
        if (jointFromState == NUI_SKELETON_POSITION_TRACKED && jointToState == NUI_SKELETON_POSITION_TRACKED)
        {

			pRenderTarget->DrawLine(Points[jointFrom], Points[jointTo], pBrushBoneTracked, g_TrackedBoneThickness); // Draw bold lines if the joints are both tracked
        }
      }

D2D1_POINT_2F SkeletonToScreen(Vector4 skeletonPoint, int width, int height)
{
    LONG x, y;
    USHORT depth;

    // Calculate the skeleton's position on the screen
    // NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
    NuiTransformSkeletonToDepthImage(skeletonPoint, &x, &y, &depth);

    float screenPointX = static_cast<float>(x * width) / cScreenWidth;
    float screenPointY = static_cast<float>(y * height) / cScreenHeight;

    return D2D1::Point2F(screenPointX, screenPointY);
}

void DrawSkeleton(const NUI_SKELETON_DATA & skeleton,int windowWidth,int windowHeight)
    {

		int i;

    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
       Points[i] = SkeletonToScreen(skeleton.SkeletonPositions[i], windowWidth, windowHeight);
    }


      // Render Head and Shoulders
      DrawBone(skeleton, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
      DrawBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
      DrawBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);

      // Render Left Arm
      DrawBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
      DrawBone(skeleton, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
      DrawBone(skeleton, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

      // Render Right Arm
      DrawBone(skeleton, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
      DrawBone(skeleton, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
      DrawBone(skeleton, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);


      // Render other bones... 


	   // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( Points[i], g_JointThickness, g_JointThickness );

        if ( skeleton.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED )
        {
            pRenderTarget->DrawEllipse(ellipse, pBrushJointInferred);
        }
        else if ( skeleton.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED )
        {
            pRenderTarget->DrawEllipse(ellipse, pBrushJointTracked);
        }
    }
    }

void DiscardDirect2DResources( )
{
    SafeRelease(pRenderTarget);
    SafeRelease(pBrushJointTracked);
    SafeRelease(pBrushJointInferred);
    SafeRelease(pBrushBoneTracked);
    SafeRelease(pBrushBoneInferred);
}

void ProcessSkeleton()
{
    NUI_SKELETON_FRAME skeletonFrame = {0};

    HRESULT hr = pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
    if ( FAILED(hr) )
    {
        return;
    }

    // smooth out the skeleton data
    pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

    // Endure Direct2D is ready to draw
    hr = EnsureDirect2DResources( );
    if ( FAILED(hr) )
    {
        return;
    }

    pRenderTarget->BeginDraw();
    pRenderTarget->Clear( );

    RECT rct;
    GetClientRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rct);
    int width = rct.right;
    int height = rct.bottom;

    for (int i = 0 ; i < NUI_SKELETON_COUNT; ++i)
    {
        NUI_SKELETON_TRACKING_STATE trackingState = skeletonFrame.SkeletonData[i].eTrackingState;

        if (NUI_SKELETON_TRACKED == trackingState)
        {
            // We're tracking the skeleton, draw it
            DrawSkeleton(skeletonFrame.SkeletonData[i], width, height);
        }
        else if (NUI_SKELETON_POSITION_ONLY == trackingState)
        {
            // we've only received the center point of the skeleton, draw that
            D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                SkeletonToScreen(skeletonFrame.SkeletonData[i].Position, width, height),
                g_JointThickness,
                g_JointThickness
                );

            pRenderTarget->DrawEllipse(ellipse, pBrushJointTracked);
        }
    }

    hr = pRenderTarget->EndDraw();

    // Device lost, need to recreate the render target
    // We'll dispose it now and retry drawing
    if (D2DERR_RECREATE_TARGET == hr)
    {
        hr = S_OK;
        DiscardDirect2DResources();
    }
}

const char * decodifica(int x, int op, int ponto){

	if(op == 0){
	switch (x){
			
	case 0:
		return "corpo";
		break;

	case 1:
		return "cab frt";
		break;

	case 2:
		return "cab lat";
		break;

	case 3:
		return "bra esq";
		break;

	case 4:
		return "antbra esq"; 
		break;

	case 5:
		return "mao esq";
		break;

	case 6:
		return "bra dir";
		break;

	case 7:
		return "antbra dir"; 
		break;

	case 8:
		return "mao dir"; 
		break;

	case 9:
		return "perna esquerdo";
		break;

	case 10:
		return "joelho esquerdo";
		break;

	case 11:
		return "pe esquerdo";
		break;

	case 12:
		return "perna direita";
		break;

	case 13:
		return "joelho dir";
		break;

	case 14:
		return "pe dir";
		break;

	case 15:
		return "cintura";
		break;
		}

	}
	else if(op == 1){

		switch (x){
		
	case 0:
			if(ponto == 1){
				return "cintura_centro";
				break;
			}
			else if(ponto == 2){
				return "Coluna";
				break;
			}
			else if(ponto == 3){
				return "ombros_centro";
				break;
			}
		break;

	case 1:
		if(ponto == 1){
				return "coluna";
				break;
			}
			else if(ponto == 2){
				return "ombros_centro";
				break;
			}
			else if(ponto == 3){
				return "cabeca";
				break;
			}
		break;

	case 2:
		if(ponto == 1){
				return "cabeca";
				break;
			}
			else if(ponto == 2){
				return "ombros_centro";
				break;
			}
			else if(ponto == 3){
				return "ombro_esquerdo";
				break;
			}
		break;

	case 3:
		if(ponto == 1){
				return "ombros_centro";
				break;
			}
			else if(ponto == 2){
				return "ombro_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "cotovelo_esquerdo";
				break;
			}
		break;

	case 4:
		if(ponto == 1){
				return "ombro_esquerdo";
				break;
			}
			else if(ponto == 2){
				return "cotovelo_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "pulso_esquerdo";
				break;
			}
		break;

	case 5:
		if(ponto == 1){
				return "cotovelo_esquerdo";
				break;
			}
			else if(ponto == 2){
				return "pulso_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "mao_esquerda";
				break;
			}
		break;

	case 6:
		if(ponto == 1){
				return "ombros_centro";
				break;
			}
			else if(ponto == 2){
				return "ombro_direito";
				break;
			}
			else if(ponto == 3){
				return "cotovelo_direito";
				break;
			}
		break;

	case 7:
		if(ponto == 1){
				return "ombro_direito";
				break;
			}
			else if(ponto == 2){
				return "cotovelo_direito";
				break;
			}
			else if(ponto == 3){
				return "pulso_direito";
				break;
			} 
		break;

	case 8:
		if(ponto == 1){
				return "cotovelo_direito";
				break;
			}
			else if(ponto == 2){
				return "pulso_direito";
				break;
			}
			else if(ponto == 3){
				return "mao_direita";
				break;
			} 
		break;

	case 9:
		if(ponto == 1){
				return "quadril_centro";
				break;
			}
			else if(ponto == 2){
				return "quadril_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "joelho_esquerdo";
				break;
			}
		break;

	case 10:
		if(ponto == 1){
				return "quadril_esquerdo";
				break;
			}
			else if(ponto == 2){
				return "joelho_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "tornozelo_esquerdo";
				break;
			}
		break;

	case 11:
		if(ponto == 1){
				return "joelho_esquerdo";
				break;
			}
			else if(ponto == 2){
				return "tornozelo_esquerdo";
				break;
			}
			else if(ponto == 3){
				return "pe_esquerdo";
				break;
			}
		break;

	case 12:
		if(ponto == 1){
				return "quadril_centro";
				break;
			}
			else if(ponto == 2){
				return "quadril_direito";
				break;
			}
			else if(ponto == 3){
				return "joelho_direito";
				break;
			}
		break;

	case 13:
		if(ponto == 1){
				return "quadril_direito";
				break;
			}
			else if(ponto == 2){
				return "joelho_direto";
				break;
			}
			else if(ponto == 3){
				return "tornozelo_direito";
				break;
			}
		break;

	case 14:
		if(ponto == 1){
				return "joelho_direto";
				break;
			}
			else if(ponto == 2){
				return "tornozelo_direito";
				break;
			}
			else if(ponto == 3){
				return "pe_direito";
				break;
			}
		break;

	case 15:
		if(ponto == 1){
				return "quadril_direito";
				break;
			}
			else if(ponto == 2){
				return "quadril_centro";
				break;
			}
			else if(ponto == 3){
				return "quadril_esquerdo";
				break;
			}
		break;

		}
		
		}
	}


void tentativa1(lista *l ){
XMLElement * ponto1;
XMLElement * ponto2;
XMLElement * ponto3;
XMLElement * bone;
XMLElement * pElement;
XMLElement * pListElement;
int i;
	for(i = 0; i < MAX; i++){

	pElement = doc.NewElement( "angulo_entre" );

	bone = doc.NewElement(decodifica(i,0,0));

	ponto1 = doc.NewElement(l->A[i].nomeP1);

	pListElement = doc.NewElement("formato_atributo");
	pListElement->SetText("pococa");
	ponto1->InsertEndChild(pListElement);

	pListElement = doc.NewElement("min_rot_atributos");
	pListElement->SetText("nada ainda");
	ponto1->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributos");
	pListElement->SetText("nada ainda");
	ponto1->InsertEndChild(pListElement);

	pElement ->InsertEndChild(ponto1);

	ponto2 = doc.NewElement(l->A[i].nomeP2);

	pListElement = doc.NewElement("formato_atributo");
	pListElement->SetText(decodifica(i,0,NULL));
	ponto2->InsertEndChild(pListElement);

	pListElement = doc.NewElement("min_rot_atributos");
	pListElement->SetText("nada ainda");
	ponto2->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributos");
	pListElement->SetText("nada ainda");
	ponto2->InsertEndChild(pListElement);

	pElement->InsertEndChild(ponto2);
	
	ponto3 = doc.NewElement(l->A[i].nomeP3);

	pListElement = doc.NewElement("formato_atributo");
	pListElement->SetText(decodifica(i,0,NULL));
	ponto3->InsertEndChild(pListElement);

	pListElement = doc.NewElement("min_rot_atributos");
	pListElement->SetText("nada ainda");
	ponto3->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributos");
	pListElement->SetText("nada ainda");
	ponto3->InsertEndChild(pListElement);
	
	bone->InsertEndChild(ponto3);

	pElement ->InsertEndChild(bone);
	}
	
	
	pRoot->InsertEndChild(pElement);

}

double tabela(double sen, double cos, double angulo){



	if( cos >=0.9998 || sen <= 0.0175 ){
				angulo = 1;
		}

		else if(cos >= 0.9994 || sen <= 0.0349 ){
				angulo = 2;
		}
		else if(cos >= 0.9986 || sen <= 0.0523 ){
				angulo = 3;
		}
		else if(cos >= 0.9976 || sen <= 0.0698 ){
				angulo = 4;
		}
		else if(cos >= 0.9962 || sen <= 0.0872 ){
				angulo = 5;
		}
		else if(cos >= 0.9945 || sen <= 0.1045 ){
				angulo = 6;
		}
		else if(cos >= 0.9925 || sen <= 0.1219 ){
				angulo = 7;
		}
		else if(cos >= 0.9903 || sen <= 0.1392 ){
				angulo = 8;
		}
		else if(cos >= 0.9877 || sen <= 0.1564 ){
				angulo = 9;
		}
		else if(cos >=0.9848 || sen <=0.1736 ){
				angulo = 10;
		}
		else if(cos >=0.9816 || sen <=0.1908 ){
				angulo = 11;
		}
		else if(cos >=0.9781 || sen <=0.2079 ){
				angulo = 12;
		}
		else if(cos >=0.9744 || sen <=0.2250 ){
				angulo = 13;
		}
		else if(cos >=0.9703 || sen <=0.2419 ){
				angulo = 14;
		}
		else if(cos >=0.9659 || sen <= 0.2588 ){
				angulo = 15;
}
		else if(cos >=0.9613 || sen <=0.2756 ){
				angulo = 16;
		}
		else if(cos >=0.9563 || sen <=0.2924 ){
				angulo = 17;
		}
		else if(cos >=0.9511 || sen <=0.3090 ){
				angulo = 18;
		}
		else if(cos >=0.9455 || sen <=0.3256 ){
				angulo = 19;
		}
		else if(cos >=0.9397 || sen <= 0.3420 ){
				angulo = 20;
				}
		else if(cos >=0.9336 || sen <=0.3584 ){
				angulo = 21;
		}
		else if(cos >=0.9272 || sen <=0.3746 ){
				angulo = 22;
		}
		else if(cos >=0.9205 || sen <=0.3907 ){
				angulo = 23;
		}
		else if(cos >=0.9848 || sen <=0.4067 ){
				angulo = 24;
		}
		else if( cos >=0.9063 || sen <= 0.4226){
				angulo = 25;
				}
		else if( cos >=0.8988 || sen <= 0.4384){
				angulo = 26;
				}
		else if( cos >=0.8910 || sen <= 0.4540){
				angulo = 27;
				}
		else if( cos >=0.8829 || sen <= 0.4695){
				angulo = 28;
				}
		else if( cos >=0.8746 || sen <= 0.4848){
				angulo = 29;
				}
		else if( cos >=0.8660 || sen <= 0.5000 ){
				angulo = 30;
				}
		else if( cos >=0.8572 || sen <= 0.5150){
				angulo = 31;
				}
		else if( cos >=0.8480 || sen <= 0.5299){
				angulo = 32;
				}
		else if( cos >=0.8387 || sen <= 0.5446){
				angulo = 33;
				}
		else if( cos >=0.5592 || sen <= 0.8290){
				angulo = 34;
				}
		else if( cos >=0.8192 || sen <= 0.5736){
				angulo = 35;
		}
		else if( cos >=0.8090 || sen <= 0.5878){
				angulo = 36;
				}
		else if( cos >=0.7986 || sen <= 0.6018){
				angulo = 37;
				}
		else if( cos >=0.7880 || sen <= 0.6157){
				angulo = 38;
				}
		else if( cos >=0.7771 || sen <= 0.6293){
				angulo = 39;
				}
		else if( cos >=0.7660 || sen <= 0.6428 ){
				angulo = 40;
		}
		else if( cos >=0.7547 || sen <= 0.6561){
				angulo = 41;
				}
		else if( cos >=0.7431 || sen <= 0.6691){
				angulo = 42;
				}
		else if( cos >=0.7314 || sen <= 0.6820){
				angulo = 43;
				}
		else if( cos >=0.6947 || sen <= 0.7193){
				angulo = 44;
				}
		else if( cos >=0.7071 || sen <= 0.7071 ){
				angulo = 45;
		}
		else if( cos >=0.6947 || sen <= 0.7193){
				angulo = 46;
				}
		else if( cos >=0.6820 || sen <= 0.7314){
				angulo = 47;
				}
		else if( cos >=0.6691 || sen <= 0.7431){
				angulo = 48;
				}
		else if( cos >=0.6561 || sen <= 0.7547){
				angulo = 49;
				}
		else if( cos >=0.6428 || sen <= 0.7660){
				angulo = 50;
		}
		else if( cos >=0.6293 || sen <= 0.7771){
				angulo = 51;
				}
		else if( cos >=0.6157 || sen <= 0.7880){
				angulo = 52;
				}
		else if( cos >=0.6018 || sen <= 0.7986){
				angulo = 53;
				}
		else if( cos >=0.5878 || sen <= 0.8090){
				angulo = 54;
				}
		else if( cos >=0.5736 || sen <= 0.8192){
				angulo = 55;
		}
		else if( cos >=0.5592 || sen <= 0.8290){
				angulo = 56;
				}
		else if( cos >=0.5446 || sen <= 0.8387){
				angulo = 57;
				}
		else if( cos >=0.5299 || sen <= 0.8480){
				angulo = 58;
				}
		else if( cos >=0.5150 || sen <= 0.8572){
				angulo = 59;
				}
		else if( cos >=0.5000 || sen <= 0.8660){
				angulo = 60;
		}
		else if( cos >=0.4848 || sen <= 0.8746){
				angulo = 61;
				}
		else if( cos >=0.4695 || sen <= 0.8829){
				angulo = 62;
				}
		else if( cos >=0.4540 || sen <= 0.8910){
				angulo = 63;
				}
		else if( cos >=0.4384 || sen <= 0.8988){
				angulo = 64;
				}
		else if( cos >=0.4226 || sen <= 0.9063){
				angulo = 65;
		}
		else if( cos >=0.4067 || sen <= 0.9135){
				angulo = 66;
				}
		else if( cos >=0.3907 || sen <= 0.9205){
				angulo = 67;
				}
		else if( cos >=0.3746 || sen <= 0.9272){
				angulo = 68;
				}
		else if( cos >=0.3584 || sen <= 0.9336){
				angulo = 69;
				}
		else if( cos >=0.3420 || sen <= 0.9397){
				angulo = 70;
		}
		else if( cos >=0.3256 || sen <= 0.9455){
				angulo = 71;
				}
		else if( cos >=0.3090 || sen <= 0.9511){
				angulo = 72;
				}
		else if( cos >=0.2924 || sen <= 0.9563){
				angulo = 73;
				}
		else if( cos >=0.2756 || sen <= 0.9613){
				angulo = 74;
				}
		else if( cos >=0.2588 || sen <= 0.9659 ){
				angulo = 75;
		}
		else if( cos >=0.2419 || sen <= 0.9703){
				angulo = 76;
				}
		else if( cos >=0.2250 || sen <= 0.9744){
				angulo = 77;
				}
		else if( cos >=0.2079 || sen <= 0.9781){
				angulo = 78;
				}
		else if( cos >=0.1908 || sen <= 0.9816){
				angulo = 79;
				}
		else if( cos >=0.1736 || sen <= 0.9848){
				angulo = 80;
		}
		else if( cos >=0.1564 || sen <= 0.9877){
				angulo = 81;
				}
		else if( cos >=0.1392 || sen <= 0.9903){
				angulo = 82;
				}
		else if( cos >=0.1219 || sen <= 0.9925){
				angulo = 83;
				}
		else if( cos >=0.1045 || sen <= 0.9945){
				angulo = 84;
				}
		else if( cos >=0.0872 || sen <= 0.9962){
				angulo = 85;
		}
		else if( cos >=0.0698 || sen <= 0.9976){
				angulo = 86;
				}
		else if( cos >=0.0523 || sen <= 0.9986){
				angulo = 87;
				}
		else if( cos >=0.0349 || sen <= 0.9994){
				angulo = 88;
				}
		else if( cos >=0.0175 || sen <= 0.9998){
				angulo = 89;
				}
		else if( cos >=0.0000 || sen <=0.9999 ){
				angulo = 90;
		}

	return angulo;
}


double Angulo(Vector4 P1, Vector4 P2, Vector4 P3){
	double R1, R2, x, y, z, x2, y2, z2, cos, angulo,sen;


	x= P1.x - P2.x;
	y= P1.y - P2.y;
	z= P1.z - P2.z;

	x2=P3.x - P2.x;
	y2=P3.y - P2.y;
	z2=P3.z - P2.z;



	R1 =  sqrt(powl(x,2) + powl(y,2) + powl(z,2));

	R2 =  sqrt(powl(x2,2) + powl(y2,2) + powl(z2,2));

	cos = ((x * x2) +(y * y2) +(z * z2)) / (R1 * R2);

	sen = sqrt( 1 - (powl(cos,2)));


	angulo = NULL;

		

		if( cos < 0 && sen > 0){ // segundo quadrante
			angulo = tabela(sen,cos,angulo);
			angulo = 90 - angulo;
			angulo = angulo + 90;
		}
		else if(cos < 0 && sen < 0){ // terceiro quadrante
			angulo = tabela(sen,(cos * -1),angulo);
			angulo = (180 -((90 - angulo)+90))+180;
			}
		else if(cos > 0 && sen < 0){ // quarto quadrante
			angulo = tabela(sen,cos,angulo);
			angulo = (270-(180 -((90 - angulo)+90))+180)+270;
			}
		else angulo = tabela(sen,cos,angulo);// primeiro quadrante
	if(angulo == NULL)return -1;

		else return angulo;

}

void inicializarListaSequencial(lista *l) {
l->bone = 0;
}


void exibirLista(lista *l) {
int i;
for (i=0; i < l->bone; i++)
 printf("%f ", l->A[i].anguloA);
}

bool InsereLista(Vector4 p1 ,Vector4 p2, Vector4 p3, int i, lista *l){

if(i >= 0 && l->bone < MAX && l->A[l->bone].p1A.x == NULL){
	if(i == 2){
		l->A[l->bone].p1A = p2;
		l->A[l->bone].p2A = p1;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A,l->A[l->bone].p2A,l->A[l->bone].p3A);
	}
	else if(i == 3){
		l->A[l->bone].p1A = l->A[1].p2A; // ponto 2 ombros_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A,l->A[l->bone].p2A,l->A[l->bone].p3A);
	}
	else if(i == 7){
		l->A[l->bone].p1A = l->A[1].p2A; // ponto 2 ombros_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A,l->A[l->bone].p2A,l->A[l->bone].p3A);
	}else if(i == 11){

		l->A[l->bone].p1A = l->A[0].p1A; // ponto 1 quadril_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A,l->A[l->bone].p2A,l->A[l->bone].p3A);
	}else if(i == 15){
		l->A[l->bone].p1A = l->A[0].p1A; // ponto 1 quadril_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A,l->A[l->bone].p2A,l->A[l->bone].p3A);
	}
	else{
	l->A[l->bone].p1A = p1;
	l->A[l->bone].p2A = p2;
	l->A[l->bone].p3A = p3;
	l->A[l->bone].anguloA = Angulo(p1,p2,p3);

	}



	l->A[l->bone].nomeP1 = decodifica(l->bone,1,1);
	l->A[l->bone].nomeP2 = decodifica(l->bone,1,2);
	l->A[l->bone].nomeP3 = decodifica(l->bone,1,3);

	if(l->bone == 14){
			l->A[15].p1A = l->A[13].p1A; // ponto 16 quadril_dir
			l->A[15].p2A = l->A[0].p1A; // ponto 1 quadril_centro
			l->A[15].p3A = l->A[8].p1A;;// ponto 12 quadril_esuqrdo
			l->A[15].anguloA = Angulo(l->A[15].p1A,l->A[15].p2A,l->A[15].p3A);
			l->A[15].nomeP1 = decodifica(15,1,1);
			l->A[15].nomeP2 = decodifica(15,1,2);
			l->A[15].nomeP3 = decodifica(15,1,3);
			l->bone = l->bone++;
		}

	l->bone = l->bone++;
	/*printf("entrou no if");
	printf("\n indice %i",i);
	printf("\nbone %i\n",l->bone);
	printf("vetor %f",l->A[l->bone].p1A.x);
	printf("\n");*/
	return true;
	}
else{
	if(l->bone == MAX)l->bone = 0;

	if(i == 2){
		l->A[l->bone].p1O = l->A[l->bone].p1A;
		l->A[l->bone].p1A = p2;
		l->A[l->bone].p2O = l->A[l->bone].p2A;
		l->A[l->bone].p2A = p1;
		l->A[l->bone].p3O = l->A[l->bone].p3A;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A,l->A[i].p2A,l->A[i].p3A);
	}
	else if(i == 3){
		l->A[l->bone].p1O = l->A[l->bone].p1A;
		l->A[l->bone].p2O = l->A[l->bone].p2A;
		l->A[l->bone].p3O = l->A[l->bone].p3A;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A = l->A[1].p2A; // ponto 2 ombros_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A,l->A[i].p2A,l->A[i].p3A);
	}
	else if(i == 7){
		l->A[l->bone].p1O = l->A[l->bone].p1A;
		l->A[l->bone].p2O = l->A[l->bone].p2A;
		l->A[l->bone].p3O = l->A[l->bone].p3A;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A = l->A[1].p2A; // ponto 2 ombros_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A,l->A[i].p2A,l->A[i].p3A);
	}else if(i == 11){
		l->A[l->bone].p1O = l->A[l->bone].p1A;
		l->A[l->bone].p2O = l->A[l->bone].p2A;
		l->A[l->bone].p3O = l->A[l->bone].p3A;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A = l->A[0].p1A; // ponto 1 quadril_centro
		l->A[l->bone].p2A = p2;
		l->A[l->bone].p3A = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A,l->A[i].p2A,l->A[i].p3A);
	}else if(i == 15){
		l->A[l->bone].p1O = l->A[l->bone].p1A;
		l->A[l->bone].p2O = l->A[l->bone].p2A;
		l->A[l->bone].p3O = l->A[l->bone].p3A;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[i].p1A = l->A[0].p1A; // ponto 1 quadril_centro
		l->A[i].p2A = p2;
		l->A[i].p3A = p3;
		l->A[i].anguloA = Angulo(l->A[i].p1A,l->A[i].p2A,l->A[i].p3A);
	}

	else{
	l->A[l->bone].p1O = l->A[l->bone].p1A;
	l->A[l->bone].p1A = p1;
	l->A[l->bone].p2O = l->A[l->bone].p2A;
	l->A[l->bone].p2A = p2;
	l->A[l->bone].p3O = l->A[l->bone].p3A;
	l->A[l->bone].p3A = p3;

	l->A[l->bone].anguloO = l->A[l->bone].anguloA;
	l->A[l->bone].anguloA = Angulo(p1,p2,p3);

	if(l->bone == 14){
			l->A[15].p1O = l->A[15].p1A;
			l->A[15].p2O = l->A[15].p2A;
			l->A[15].p3O = l->A[15].p3A;
			l->A[15].p1A = l->A[13].p1A; // ponto 16 quadril_dir
			l->A[15].p2A = l->A[0].p1A; // ponto 1 quadril_centro
			l->A[15].p3A = l->A[8].p1A;;// ponto 12 quadril_esuqrdo
			l->A[15].anguloA = Angulo(l->A[15].p1A,l->A[15].p2A,l->A[15].p3A);
			l->A[15].nomeP1 = decodifica(15,1,1);
			l->A[15].nomeP2 = decodifica(15,1,2);
			l->A[15].nomeP3 = decodifica(15,1,3);
			l->bone = l->bone++;
		}

	 }l->bone = l->bone++;
	/*printf("\n indice %i",i);
	printf("\nbone %i\n",l->bone);
	printf("vetor %f",l->A[l->bone].p1A.x);
	printf("\n");
	printf("entrou no else");*/
	return true;
}
printf("deu erro");

return false;
}



void SkeletonFrameReady(NUI_SKELETON_FRAME* pSkeletonFrame, lista *l){

	NUI_SKELETON_BONE_ORIENTATION boneOrientations[NUI_SKELETON_POSITION_COUNT];
		 


	for (int i = 0; i < NUI_SKELETON_COUNT; i++){
		 NUI_SKELETON_DATA& skeleton = pSkeletonFrame->SkeletonData[i];
		 NuiSkeletonCalculateBoneOrientations(&skeleton, boneOrientations);


		switch (skeleton.eTrackingState){
 
		case NUI_SKELETON_TRACKED:

			
			int j;
			for (j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j){
				

				



			if(j < NUI_SKELETON_POSITION_COUNT-2){
				/*printf("\n");
				printf("%i",j);
				printf("\n%i\n",j+1);
				printf("%i",j+2);
				printf("\n");*/

				if(j != 6 && j != 10 && j != 14){
					

			InsereLista(skeleton.SkeletonPositions[j],skeleton.SkeletonPositions[j+1],skeleton.SkeletonPositions[j+2],j,l);
							}
						}

			printf("x  y  z  w ");
				printf("\n x  %f, %f, %f, %f\n", boneOrientations[j].hierarchicalRotation.rotationMatrix.M11, boneOrientations[j].hierarchicalRotation.rotationMatrix.M12,
					boneOrientations[j].hierarchicalRotation.rotationMatrix.M13,boneOrientations[j].hierarchicalRotation.rotationMatrix.M14);

				printf("y  %f, %f, %f, %f\n", boneOrientations[j].hierarchicalRotation.rotationMatrix.M21, boneOrientations[j].hierarchicalRotation.rotationMatrix.M22,
					boneOrientations[j].hierarchicalRotation.rotationMatrix.M23,boneOrientations[j].hierarchicalRotation.rotationMatrix.M24);

				printf("z  %f, %f, %f, %f\n", boneOrientations[j].hierarchicalRotation.rotationMatrix.M31, boneOrientations[j].hierarchicalRotation.rotationMatrix.M32,
					boneOrientations[j].hierarchicalRotation.rotationMatrix.M33,boneOrientations[j].hierarchicalRotation.rotationMatrix.M34);

				printf("w  %f, %f, %f, %f\n", boneOrientations[j].hierarchicalRotation.rotationMatrix.M41, boneOrientations[j].hierarchicalRotation.rotationMatrix.M42,
					boneOrientations[j].hierarchicalRotation.rotationMatrix.M43,boneOrientations[j].hierarchicalRotation.rotationMatrix.M44);

				printf("Ox : %f, Oy : %f, Oz : %f\n",atan2(boneOrientations[j].hierarchicalRotation.rotationMatrix.M32,boneOrientations[j].hierarchicalRotation.rotationMatrix.M33)* 180 / PI,
					atan2(((boneOrientations[j].hierarchicalRotation.rotationMatrix.M31)* -1),sqrt(pow(boneOrientations[j].hierarchicalRotation.rotationMatrix.M32,2)+ pow(boneOrientations[j].hierarchicalRotation.rotationMatrix.M33,2)))* 180 / PI,
						atan2(boneOrientations[j].hierarchicalRotation.rotationMatrix.M21,boneOrientations[j].hierarchicalRotation.rotationMatrix.M11))* 180 / PI;

					}

	
		/*	for(j = 0; j < MAX; ++j){
				printf("\n");
				printf("%s\n",decodifica(j,0,0));
				printf("P1 %s x %f y %f z %f", l->A[j].nomeP1,l->A[j].p1A.x,l->A[j].p1A.y,l->A[j].p1A.z);
				printf("\nP2 %s x %f y %f z %f\n",l->A[j].nomeP2,l->A[j].p2A.x,l->A[j].p2A.y,l->A[j].p2A.z);
				printf("P3 %s  x %f y %f z %f\n",l->A[j].nomeP3,l->A[j].p3A.x,l->A[j].p3A.y,l->A[j].p3A.z);
				printf("angulo %f",l->A[j].anguloA);
				printf("\n");
			
			
			}*/

			//tentativa1(l);

			doc.SaveFile("teste.xml");
			break;

		case NUI_SKELETON_POSITION_ONLY:
			printf("ELE ENTROU MESMO\n\n");
			printf("%d\n\n",i);

			break;



			//DrawSkeleton(skeleton);

			/*printf("%f\n\n"," X ", leftHand.x);
			printf("%f\n\n"," X ", leftHand.y);
			printf("%f\n\n"," X ", leftHand.z);

			printf("%f"," \n ");*/
		
		}
	}

	
}

void UpdateKinectST(lista *l)
{
      // Wait for 0ms, just quickly test if it is time to process a skeleton
      if ( WAIT_OBJECT_0 == WaitForSingleObject(nextSkeletonEvent, 0) )
      {
        NUI_SKELETON_FRAME skeletonFrame = {0};

        // Get the skeleton frame that is ready
        if (SUCCEEDED(pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
        {
          // Process the skeleton frame
          SkeletonFrameReady(&skeletonFrame,l);
        }
      }
    }

void iniciavetor(lista *l){
	int i;
	for(i = 0; i<MAX; i++){
		l->A[i].p1A.x = NULL;
	}
}
void main(){

	printf("entrou\n\n");

	lista  *l = (lista *) malloc(sizeof(lista));

	inicializarListaSequencial(l);
	iniciavetor(l);
	printf("%i aqui",l->bone);
	StartKinectST();
	
	while(true){
		UpdateKinectST(l);
	}

	
}
