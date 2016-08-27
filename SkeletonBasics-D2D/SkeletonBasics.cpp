//------------------------------------------------------------------------------
// <copyright file="SkeletonBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------
#include "stdafx.h"
#include <Windows.h>
#include <CommDlg.h>
#include <iostream>
#include <strsafe.h>
#include "SkeletonBasics.h"
#include "resource.h"
#include <conio.h>
#include <time.h>
#include <math.h>
using namespace std;
#include "tinyxml2.h"
#define BUTTONOK 10
#define STP 11
#define OPEN 12
#define MAX 16
static const float g_JointThickness = 3.0f;
static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;
int operador =0;
int op2=0;

//xml
using namespace tinyxml2;

 
 typedef struct{
	 Vector4 p;
	 double rotx, roty, rotz;
 }ponto;

//Lista Estatica
 typedef struct{
	ponto p1, p2, p3; 
	const char * nomeP1,* nomeP2, *nomeP3;
	double angulo; 
}Registro;

typedef struct lista{
	Registro A[MAX];
	struct lista * prox;
}lista;

typedef struct dados{
	struct lista* primeiro;
	struct lista* ultimo;
}dados;

dados* data = (dados*)malloc(sizeof(dados));

lista *l;

typedef struct frame{
	Vector4 p[20];
	struct frame* prox;
}frame;

frame* fr = (frame*)malloc(sizeof(frame));

frame* fraux;

typedef struct filme{
	struct frame* primeiro;
	struct frame* ultimo;
	int frames;
}filme;

filme* mv = (filme*)malloc(sizeof(filme));

HINSTANCE g_inst;
HWND ButtonOk;
HWND ButtonStp;
HWND ButtonOpen;
HWND CheckBox;
char txt [256];
//FILE * pFile;
tinyxml2::XMLDocument doc;
XMLNode * pRoot = doc.NewElement("Root");// cria um elemento root;

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{	
    CSkeletonBasics application;
    application.Run(hInstance, nCmdShow);
}

/// <summary>
/// Constructor
/// </summary>
CSkeletonBasics::CSkeletonBasics() :
    m_pD2DFactory(NULL),
    m_hNextSkeletonEvent(INVALID_HANDLE_VALUE),
    m_pSkeletonStreamHandle(INVALID_HANDLE_VALUE),
    m_bSeatedMode(false),
    m_pRenderTarget(NULL),
    m_pBrushJointTracked(NULL),
    m_pBrushJointInferred(NULL),
    m_pBrushBoneTracked(NULL),
    m_pBrushBoneInferred(NULL),
    m_pNuiSensor(NULL)
{
    ZeroMemory(m_Points,sizeof(m_Points));
}

/// <summary>
/// Destructor
/// </summary>
CSkeletonBasics::~CSkeletonBasics()
{
    if (m_pNuiSensor)
    {
        m_pNuiSensor->NuiShutdown();
    }

    if (m_hNextSkeletonEvent && (m_hNextSkeletonEvent != INVALID_HANDLE_VALUE))
    {
        CloseHandle(m_hNextSkeletonEvent);
    }

    // clean up Direct2D objects
    DiscardDirect2DResources();

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    SafeRelease(m_pNuiSensor);
}

void DrawComponents(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {

	ButtonOk = CreateWindowExW (
0,
L"BUTTON",
L"Gravar",
WS_VISIBLE|WS_CHILD,
30, 630, 100, 30,
hwnd,
(HMENU)BUTTONOK,
g_inst,
NULL
);

ButtonStp = CreateWindowExW (
0,
L"BUTTON",
L"Start",
WS_VISIBLE|WS_CHILD,
220, 630, 100, 30,
hwnd,
(HMENU)STP,
g_inst,
NULL
);

ButtonOpen = CreateWindowExW (
0,
L"BUTTON",
L"Open",
WS_VISIBLE|WS_CHILD,
450, 630, 100, 30,
hwnd,
(HMENU)OPEN,
g_inst,
NULL
);


}

void inicializaLista(lista *l) {
	l->prox = NULL;
	int i;
	for(i = 0; i<MAX; i++){
		l->A[i].p1.p.x = NULL;
		l->A[i].p1.p.y = NULL;
		l->A[i].p1.p.z = NULL;
		l->A[i].p2.p.x = NULL;
		l->A[i].p2.p.y = NULL;
		l->A[i].p2.p.z = NULL;
		l->A[i].p3.p.x = NULL;
		l->A[i].p3.p.y = NULL;
		l->A[i].p3.p.z = NULL;
		l->A[i].angulo = NULL;
		l->A[i].nomeP1 = "";
		l->A[i].nomeP2 = "";
		l->A[i].nomeP3 = "";
	}
}

void iniciaframe(frame *fr){
	int i;
	fr->prox = NULL;
	for(i = 0; i<MAX; i++){
		fr->p[i].x = NULL;
	}
}

/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CSkeletonBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc  = {0};
	
    // Dialog custom window class
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"SkeletonBasicsAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        hInstance,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CSkeletonBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);

    const int eventCount = 1;
    HANDLE hEvents[eventCount];

    // Main message loop
    while (WM_QUIT != msg.message)
    {
        hEvents[0] = m_hNextSkeletonEvent;

        // Check to see if we have either a message (by passing in QS_ALLEVENTS)
        // Or a Kinect event (hEvents)
        // Update() will check for Kinect events individually, in case more than one are signalled
        MsgWaitForMultipleObjects(eventCount, hEvents, FALSE, INFINITE, QS_ALLINPUT);

        // Explicitly check the Kinect frame event since MsgWaitForMultipleObjects
        // can return for other reasons even though it is signaled.
        Update();

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if ((hWndApp != NULL) && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
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

void tentativa2(Registro A){
 //cria o documento xml	

XMLElement * pElement;
XMLElement * ponto1;
XMLElement * ponto2;
XMLElement * ponto3;
XMLElement * pListElement;


	pElement = doc.NewElement( "no_angulo_entre" );
	ponto1 = doc.NewElement(A.nomeP1);
	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("X");
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p1.rotx);
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p1.p.x);
	ponto1->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Y");
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p1.roty);
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p1.p.y);
	ponto1->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Z");
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p1.rotz);
	ponto1->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p1.p.z);
	ponto1->InsertEndChild(pListElement);
	pElement ->InsertEndChild(ponto1);

	ponto2 = doc.NewElement(A.nomeP2);
	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("X");
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p2.rotx);
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p2.p.x);
	ponto2->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Y");
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p2.roty);
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p2.p.y);
	ponto2->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Z");
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p2.rotz);
	ponto2->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p2.p.z);
	ponto2->InsertEndChild(pListElement);

	pListElement = doc.NewElement("angulo_atributo");
	pListElement->SetText(A.angulo);
	ponto2->InsertEndChild(pListElement);
	pElement ->InsertEndChild(ponto2);



	ponto3 = doc.NewElement(A.nomeP3);
	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("X");
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p3.rotx);
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p3.p.x);
	ponto3->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Y");
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p3.roty);
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p3.p.y);
	ponto3->InsertEndChild(pListElement);

	pListElement = doc.NewElement("eixo_atributo");
	pListElement->SetText("Z");
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("rot_atributo");
	pListElement->SetText(A.p3.rotz);
	ponto3->InsertEndChild(pListElement);
	pListElement = doc.NewElement("pos_atributo");
	pListElement->SetText(A.p3.p.z);
	ponto3->InsertEndChild(pListElement);
	pElement ->InsertEndChild(ponto3);

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

float Angulo(Vector4 P1, Vector4 P2, Vector4 P3){
	float R1, R2, x, y, z, x2, y2, z2, cos, angulo,sen;


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
			angulo = (90 - angulo)+90;
		}
		else if(cos < 0 && sen < 0){ // terceiro quadrante
			angulo = tabela(sen,(cos * -1),angulo);
			angulo = (180 -((90 - angulo)+90))+180;
			}
		else if(cos > 0 && sen < 0){ // quarto quadrante
			angulo = tabela(sen,cos,angulo);
			angulo = (270-((180 -((90 -angulo)+90))+180))+270;
			}
		else angulo = tabela(sen,cos,angulo);// primeiro quadrante
	if(angulo == NULL)return -1;

		else return angulo;

}

void LeXml(){
	lista * f = (lista *)malloc(sizeof(lista));
	int i=0; // controla o maximo de dados dentro do vetor -> MAX = 16
	int j=0; // controla os pontos 
	 tinyxml2::XMLDocument xml_doc;

	tinyxml2::XMLError eResult = xml_doc.LoadFile("pontos.xml");

   if (eResult != tinyxml2::XML_SUCCESS) printf("\n erro na load \n");

   tinyxml2::XMLNode* root = xml_doc.FirstChildElement("Root");
   if (root == nullptr) printf("\n erro no root \n");

   tinyxml2::XMLElement* no_angulo = root->FirstChildElement();
   if (no_angulo == nullptr) printf("\n erro no no_ang \n");

   while(no_angulo){

	   while( i < MAX){
	   
   tinyxml2::XMLElement* pontos = no_angulo->FirstChildElement();
   if (pontos == nullptr) printf("\n erro nos pontos \n");
   while(pontos){

	   if(j > 2)j=0; // volta pro ponto 1

   tinyxml2::XMLElement * atrib = pontos->FirstChildElement();

   if (atrib == nullptr) printf("\n erro nos atrib \n");
    while(atrib){

		if(j == 0){
			tinyxml2::XMLElement* aux;

		if(strcmp(atrib->GetText(),"X") == 0 ){
				aux = atrib->NextSiblingElement();
				f->A[i].p1.rotx = atof(aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p1.p.x = atof(aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Y") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p1.roty = atof(aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p1.p.y = atof( aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Z") == 0){

			//printf("\ni: %i\n",i);
				aux = atrib->NextSiblingElement();
				f->A[i].p1.rotz = atof( aux->GetText());
				//printf("\ns: %s\n",aux->GetText());

				aux = aux->NextSiblingElement();
				f->A[i].p1.p.z = atof( aux->GetText());
				}
			}
		
		else if(j == 1){
		tinyxml2::XMLElement* aux;
			
		if (strcmp(atrib->GetText(),"X") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p2.rotx = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p2.p.x = atof( aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Y") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p2.roty = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p2.p.y = atof( aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Z") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p2.rotz = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p2.p.z = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				//printf("\n string %s \n",aux->GetText());
				//printf("\n atoi: %d \n",atoi(aux->GetText()));
				f->A[i].angulo = atoi(aux->GetText());
				//printf("\n %d\n",f->A[i].anguloA);
				}
			}
		
		else if(j == 2){
		tinyxml2::XMLElement* aux;
			
		if (strcmp(atrib->GetText(),"X") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p3.rotx = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p3.p.x = atof( aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Y") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p3.roty = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p3.p.y = atof(aux->GetText());
				}

		else if (strcmp(atrib->GetText(),"Z") == 0){
				aux = atrib->NextSiblingElement();
				f->A[i].p3.rotz = atof( aux->GetText());
				aux = aux->NextSiblingElement();
				f->A[i].p3.p.z = atof( aux->GetText());
				}
		}
		

	atrib = atrib->NextSiblingElement(); 
	}
	pontos = pontos->NextSiblingElement(); j++;
   }
   no_angulo = no_angulo->NextSiblingElement(); i++;
   }

	i=0;

	   fr->p[0].x= f->A[0].p1.p.x;
	   fr->p[0].y= f->A[0].p1.p.y;
	   fr->p[0].z= f->A[0].p1.p.z;
	   
	   fr->p[1].x= f->A[0].p2.p.x;
	   fr->p[1].y= f->A[0].p2.p.y;
	   fr->p[1].z= f->A[0].p2.p.z;

	   fr->p[2].x= f->A[0].p3.p.x;
	   fr->p[2].y= f->A[0].p3.p.y;
	   fr->p[2].z= f->A[0].p3.p.z;

	   fr->p[3].x= f->A[1].p1.p.x;
	   fr->p[3].y= f->A[1].p1.p.y;
	   fr->p[3].z= f->A[1].p1.p.z;

	   fr->p[4].x= f->A[4].p1.p.x;
	   fr->p[4].y= f->A[4].p1.p.y;
	   fr->p[4].z= f->A[4].p1.p.z;
	   
	   fr->p[5].x= f->A[4].p2.p.x;
	   fr->p[5].y= f->A[4].p2.p.y;
	   fr->p[5].z= f->A[4].p2.p.z;

	   fr->p[6].x= f->A[4].p3.p.x;
	   fr->p[6].y= f->A[4].p3.p.y;
	   fr->p[6].z= f->A[4].p3.p.z;

	   fr->p[7].x= f->A[5].p1.p.x;
	   fr->p[7].y= f->A[5].p1.p.y;
	   fr->p[7].z= f->A[5].p1.p.z;

	   fr->p[8].x= f->A[7].p1.p.x;
	   fr->p[8].y= f->A[7].p1.p.y;
	   fr->p[8].z= f->A[7].p1.p.z;
	   
	   fr->p[9].x= f->A[7].p2.p.x;
	   fr->p[9].y= f->A[7].p2.p.y;
	   fr->p[9].z= f->A[7].p2.p.z;

	   fr->p[10].x= f->A[7].p3.p.x;
	   fr->p[10].y= f->A[7].p3.p.y;
	   fr->p[10].z= f->A[7].p3.p.z;

	   fr->p[11].x= f->A[8].p1.p.x;
	   fr->p[11].y= f->A[8].p1.p.y;
	   fr->p[11].z= f->A[8].p1.p.z;

	   fr->p[12].x= f->A[10].p1.p.x;
	   fr->p[12].y= f->A[10].p1.p.y;
	   fr->p[12].z= f->A[10].p1.p.z;
	   
	   fr->p[13].x= f->A[10].p2.p.x;
	   fr->p[13].y= f->A[10].p2.p.y;
	   fr->p[13].z= f->A[10].p2.p.z;

	   fr->p[14].x= f->A[10].p3.p.x;
	   fr->p[14].y= f->A[10].p3.p.y;
	   fr->p[14].z= f->A[10].p3.p.z;

	   fr->p[15].x= f->A[11].p1.p.x;
	   fr->p[15].y= f->A[11].p1.p.y;
	   fr->p[15].z= f->A[11].p1.p.z;

	   fr->p[16].x= f->A[13].p1.p.x;
	   fr->p[16].y= f->A[13].p1.p.y;
	   fr->p[16].z= f->A[13].p1.p.z;
	   
	   fr->p[17].x= f->A[13].p2.p.x;
	   fr->p[17].y= f->A[13].p2.p.y;
	   fr->p[17].z= f->A[13].p2.p.z;

	   fr->p[18].x= f->A[13].p3.p.x;
	   fr->p[18].y= f->A[13].p3.p.y;
	   fr->p[18].z= f->A[13].p3.p.z;

	   fr->p[19].x= f->A[14].p1.p.x;
	   fr->p[19].y= f->A[14].p1.p.y;
	   fr->p[19].z= f->A[14].p1.p.z;

	   if(fr->prox == NULL && no_angulo != NULL){
	   frame *  nfr = (frame*)malloc(sizeof(frame));
	   printf("\nentrou\n");
	   iniciaframe(nfr);
	   fr->prox = nfr;
	   mv->ultimo = nfr;
	   fr = nfr;
   }


  }
  fraux = mv->primeiro;
}

void CSkeletonBasics::DrawBone2(NUI_SKELETON_POSITION_INDEX joint0, NUI_SKELETON_POSITION_INDEX joint1)
{
		if(m_Points[joint0].x > 0.01 && m_Points[joint1].x > 0.01)m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneTracked, g_TrackedBoneThickness);
}

void CSkeletonBasics::play(){
	FILE* f;
	int i;
	int j;



	HRESULT hr;

	 NUI_SKELETON_FRAME skeletonFrame = {0};

	hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
    if ( FAILED(hr) )
    {
		op2 = 0; return;
    }

    // smooth out the skeleton data
    m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

	hr = EnsureDirect2DResources( );
    if ( FAILED(hr) )
    {	
	 op2 = 0; return; 
    }

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear( );

    RECT rct;
    GetClientRect( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ), &rct);
    int width = rct.right;
    int height = rct.bottom;

	/* for (int j = 0 ; j < NUI_SKELETON_COUNT; ++j)
    {*/
		 LONG x=0, y=0;
    USHORT depth=0;

		for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {	


		if(fraux) NuiTransformSkeletonToDepthImage(fraux->p[i], &x, &y, &depth);
		 float screenPointX = static_cast<float>(x * width) / cScreenWidth;
		 float screenPointY = static_cast<float>(y * height) / cScreenHeight;

		 m_Points[i]=D2D1::Point2F(screenPointX, screenPointY);
		//m_Points[i] = SkeletonToScreen(skeletonFrame.SkeletonData[j].SkeletonPositions[i], width, height);

		//if(aux) m_Points[i] = SkeletonToScreen(aux->p[i], width, height);
	}
		/*f=fopen("Log_points.txt","r+");
		for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
		if(fraux)fprintf(f,"\nvalor do meuponto[%i]: x:%f y:%f\n",i,fraux->p[i].x,fraux->p[i].y);
		fprintf(f,"\nvalor do Points[%i]: x:%f y:%f\n",i,m_Points[i].x,m_Points[i].y);
		}
		fclose(f);*/

		
		// Render Torso
    DrawBone2(NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
    DrawBone2(NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
    DrawBone2(NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

    // Left Arm
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

    // Right Arm
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

    // Left Leg
    DrawBone2(NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

    // Right Leg
    DrawBone2(NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);


	

    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[i], g_JointThickness, g_JointThickness );
			m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
	}


		

	hr = m_pRenderTarget->EndDraw();

    // Device lost, need to recreate the render target
    // We'll dispose it now and retry drawing
    if (D2DERR_RECREATE_TARGET == hr)
    {
        hr = S_OK;
        DiscardDirect2DResources();
    }

	if(fraux)fraux = fraux->prox;
	else op2 = 0;
	
}

void printafilme(){
	FILE *f;
	f= fopen("saida3.txt","r+");
	frame* aux = mv->primeiro;
	int i;
	while(aux){
		for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
    {
		printf("\ni:%i\n", i);
		printf("\nx:%f y: %f\n", aux->p[i].x,aux->p[i].y);
		fprintf(f,"\ninicio do arquivo %i\n",i);
		fprintf(f,"\nvalor do Pontos: x:%f , y:%f \n",aux->p[i].x,aux->p[i].y);
		fprintf(f,"\nfinal do arquivo %i\n",i);
		
		}
		aux = aux->prox;
	}fclose(f);
}
/// <summary>
/// Main processing function
/// </summary>
void CSkeletonBasics::Update()
{	

    if (NULL == m_pNuiSensor)
    {
        return;
    }

    // Wait for 0ms, just quickly test if it is time to process a skeleton
    if ( WAIT_OBJECT_0 == WaitForSingleObject(m_hNextSkeletonEvent, 0) )
    {
      if(op2==0)ProcessSkeleton();
	  else play();
    }
}


/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CSkeletonBasics* pThis = NULL;

    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CSkeletonBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CSkeletonBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

lista* salvaBones(lista *li, frame *fr ,NUI_SKELETON_POSITION_INDEX p1, NUI_SKELETON_POSITION_INDEX p2,NUI_SKELETON_POSITION_INDEX p3, int cont, const char * n1, const char * n2, const char * n3 ){

	//DrawLine(m_Points[joint0], m_Points[joint1]);

	li->A[cont].p1.p = fr->p[p1];
	li->A[cont].nomeP1 = n1;
	li->A[cont].angulo = Angulo(fr->p[p1],fr->p[p2],fr->p[p3]);
	li->A[cont].p2.p = fr->p[p2];
	li->A[cont].nomeP2 = n2;
	li->A[cont].p3.p = fr->p[p3];
	li->A[cont].nomeP3 = n3;
	return li;
}

void InsereLista2(const NUI_SKELETON_DATA & skel){
	frame* novo = (frame*)malloc(sizeof(frame));
	int i;
	if(fr->prox == NULL){
		for(i=0; i < NUI_SKELETON_POSITION_COUNT; i++) fr->p[i] = skel.SkeletonPositions[i];
		iniciaframe(novo);
		fr->prox = novo;
		fr = novo;
		mv->frames++;
	}
}

void trans(){
	fraux = mv->primeiro;
	lista *laux;
	while(fraux){
		lista *list = (lista*)malloc(sizeof(lista));
		inicializaLista(list);
		if(data->primeiro == NULL){
			data->primeiro = list;
			data->ultimo = list;
		}
		else{
			laux = data->ultimo;
			laux->prox = list;
			}
	list = salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER,0,"Quadril_Centro","Coluna","Ombro_Centro"); //corpo
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD,1,"Coluna","Ombro_Centro","Cabeca" ); //cabeca frente
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT,2,"Cabeca","Ombro_Centro","Ombro_Esquerdo" ); //cabeca lateral
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT,3,"Ombro_Centro","Ombro_Esquerdo","Cotovelo_Esquerdo"); //braco esquerdo
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT,4 ,"Ombro_Esquerdo","Cotovelo_Esquerdo","Pulso_Esquerdo"); //anti-braco esquerdo
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT,5,"Cotovelo_Esquerdo","Pulso_Esquerdo","Mao_Esquerda"); //mao esquerda
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT,6,"Ombro_Centro","Ombro_Direto","Cotovelo_Direito"); //braco direito
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT,7,"Ombro_Direito","Cotovelo_Direito","Pulso_Direito"); //anti-braco direito
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT,8,"Cotovelo_Direito","Pulso_Direito","Mao_Direita"); //mao direita
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT,9,"Quadril_Centro","Quadril_Esquerda","Joelho_Esquerdo" ); //perna esquerda
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT ,NUI_SKELETON_POSITION_ANKLE_LEFT,10,"Quadril_Esquerda","Joelho_Esquerdo","Tornozelo_Esquerdo" ); //joelho esquerdo
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_KNEE_LEFT ,NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT,11,"Joelho_Esquerdo","Tornozelo_Esquerdo","Pe_Esquerdo" ); //pe esquerdo
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT,12,"Quadril_Centro","Quadril_Direito","Joelho_Direito" ); //perna direito
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT ,NUI_SKELETON_POSITION_ANKLE_RIGHT,13,"Quadril_Direito","Joelho_Direito","Tornozelo_Direito"); //joelho direito
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_KNEE_RIGHT ,NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT,14,"Joelho_Direito","Tornozelo_Direito","Pe_Direito"); //pe direito
	list =salvaBones(list,fraux,NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_RIGHT,15,"Quadril_Esquerda","Quadril_Centro","Quadril_Direita" ); //quadril
	fraux = fraux->prox;
	data->ultimo = list;
	int i;
	for(i=0; i < MAX;i++)tentativa2(list->A[i]);
	}

}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CSkeletonBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {


   case WM_INITDIALOG:
        {
			DrawComponents(hWnd,message,wParam,lParam);

            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Look for a connected Kinect, and create it if found
            CreateFirstConnected();
        }
        break;
	
        // If the titlebar X is clicked, destroy app
    case WM_CLOSE:
		//fclose (pFile);
        DestroyWindow(hWnd);
        break;

    case WM_DESTROY:
        // Quit the main message pump
        PostQuitMessage(0);
        break;

        // Handle button press
    case WM_COMMAND:
        // If it was for the near mode control and a clicked event, change near mode

		switch(LOWORD(wParam)){

		case IDC_CHECK_SEATED:
        
            // Toggle out internal state for near mode
            m_bSeatedMode = !m_bSeatedMode;

            if (NULL != m_pNuiSensor)
            {
                // Set near mode for sensor based on our internal state
                m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, m_bSeatedMode ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0);
            }
        break;

		case BUTTONOK:
			int j;
			doc.InsertFirstChild(pRoot);
			for(j=0; j < MAX; j++){
			tentativa2(l->A[j]);
			}
			doc.InsertEndChild(pRoot);
			doc.SaveFile("snap.xml");
			doc.Clear();
				break;

		case STP:
			if(operador == 1){
			operador = 0;
			DestroyWindow(ButtonStp);
			ButtonStp = CreateWindowExW (
			0,
			L"BUTTON",
			L"Start",
			WS_VISIBLE|WS_CHILD,
			220, 630, 100, 30,
			hWnd,
			(HMENU)STP,
			g_inst,
			NULL);/*
			doc.InsertEndChild(pRoot);
			doc.SaveFile("gravacao.xml");
			doc.Clear();*/
			trans();
			doc.InsertEndChild(pRoot);
			doc.SaveFile("pontos.xml");
			doc.Clear();
			}
			else{
				operador = 1;
				doc.InsertFirstChild(pRoot);
				iniciaframe(fr);
				mv->primeiro = fr;
				mv->ultimo = fr;
				mv->frames = 1;
				data->primeiro = NULL;
				data->ultimo = NULL;
				DestroyWindow(ButtonStp);
				ButtonStp = CreateWindowExW (
				0,
				L"BUTTON",
				L"Stop",
				WS_VISIBLE|WS_CHILD,
				220, 630, 100, 30,
				hWnd,
				(HMENU)STP,
				g_inst,
				NULL);
			}
			break;

		case OPEN:
			if(op2 == 0){
				op2=1;
				iniciaframe(fr);
				mv->primeiro = fr;
				mv->ultimo = fr;
				LeXml();
				//printafilme(); //gera arquivo de log*/
			}
			else{
				op2=0;
			}
			
			break;
		}
	}

    return FALSE;
}

/// <summary>
/// Create the first connected Kinect found 
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CSkeletonBasics::CreateFirstConnected()
{
    INuiSensor * pNuiSensor;

    int iSensorCount = 0;
    HRESULT hr = NuiGetSensorCount(&iSensorCount);
    if (FAILED(hr))
    {
        return hr;
    }

    // Look at each Kinect sensor
    for (int i = 0; i < iSensorCount; ++i)
    {
        // Create the sensor so we can check status, if we can't create it, move on to the next
        hr = NuiCreateSensorByIndex(i, &pNuiSensor);
        if (FAILED(hr))
        {
            continue;
        }

        // Get the status of the sensor, and if connected, then we can initialize it
        hr = pNuiSensor->NuiStatus();
        if (S_OK == hr)
        {
            m_pNuiSensor = pNuiSensor;
            break;
        }

        // This sensor wasn't OK, so release it since we're not using it
        pNuiSensor->Release();
    }

    if (NULL != m_pNuiSensor)
    {
        // Initialize the Kinect and specify that we'll be using skeleton
        hr = m_pNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_SKELETON); 
        if (SUCCEEDED(hr))
        {
            // Create an event that will be signaled when skeleton data is available
            m_hNextSkeletonEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

            // Open a skeleton stream to receive skeleton data
            hr = m_pNuiSensor->NuiSkeletonTrackingEnable(m_hNextSkeletonEvent, 0); 
        }
    }

    if (NULL == m_pNuiSensor || FAILED(hr))
    {
        SetStatusMessage(L"No ready Kinect found!");
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new skeleton data
/// </summary>
void CSkeletonBasics::ProcessSkeleton()
{	
	
    NUI_SKELETON_FRAME skeletonFrame = {0};

    HRESULT hr = m_pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
    if ( FAILED(hr) )
    {
        return;
    }

    // smooth out the skeleton data
    m_pNuiSensor->NuiTransformSmooth(&skeletonFrame, NULL);

    // Endure Direct2D is ready to draw
    hr = EnsureDirect2DResources( );
    if ( FAILED(hr) )
    {
        return;
    }

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear( );

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

            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }
    }

    hr = m_pRenderTarget->EndDraw();

    // Device lost, need to recreate the render target
    // We'll dispose it now and retry drawing
    if (D2DERR_RECREATE_TARGET == hr)
    {
        hr = S_OK;
        DiscardDirect2DResources();
    }
	//if(op2==1) play();
}

/*bool InsereLista(Vector4 p1 ,Vector4 p2, Vector4 p3, int i, lista *l){

if(l->A[l->bone].p1A.p.x == NULL && l->bone < MAX){
printf("\n indice %i",i);
	printf("\nbone %i\n",l->bone);
	printf("vetor %f",l->A[l->bone].p1A.p.x);
	printf("\n");
if(i >= 0 && l->bone < MAX && l->A[l->bone].p1A.p.x == NULL){
	printf("\ndentro %i\n",l->bone);
	if(i == 2){
		l->A[l->bone].p1A.p = p2;
		l->A[l->bone].p2A.p = p1;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A.p,l->A[l->bone].p2A.p,l->A[l->bone].p3A.p);
	}
	else if(i == 3){
		l->A[l->bone].p1A.p = l->A[1].p2A.p; // ponto 2 ombros_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A.p,l->A[l->bone].p2A.p,l->A[l->bone].p3A.p);
	}
	else if(i == 7){
		l->A[l->bone].p1A.p = l->A[1].p2A.p; // ponto 2 ombros_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A.p,l->A[l->bone].p2A.p,l->A[l->bone].p3A.p);
	}else if(i == 11){

		l->A[l->bone].p1A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A.p,l->A[l->bone].p2A.p,l->A[l->bone].p3A.p);
	}else if(i == 15){
		l->A[l->bone].p1A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[l->bone].p1A.p,l->A[l->bone].p2A.p,l->A[l->bone].p3A.p);
	}
	else{
	l->A[l->bone].p1A.p = p1;
	l->A[l->bone].p2A.p = p2;
	l->A[l->bone].p3A.p = p3;
	l->A[l->bone].anguloA = Angulo(p1,p2,p3);

	}



	l->A[l->bone].nomeP1 = decodifica(l->bone,1,1);
	l->A[l->bone].nomeP2 = decodifica(l->bone,1,2);
	l->A[l->bone].nomeP3 = decodifica(l->bone,1,3);

	if(l->bone == 14){
			l->A[15].p1A.p = l->A[13].p1A.p; // ponto 16 quadril_dir
			l->A[15].p2A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
			l->A[15].p3A.p = l->A[8].p1A.p;;// ponto 12 quadril_esuqrdo
			l->A[15].anguloA = Angulo(l->A[15].p1A.p,l->A[15].p2A.p,l->A[15].p3A.p);
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
	printf("\n");
	return true;
	}
else{
	if(l->bone == MAX)l->bone = 0;

	if(i == 2){
		l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
		l->A[l->bone].p1A.p = p2;
		l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
		l->A[l->bone].p2A.p = p1;
		l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A.p,l->A[i].p2A.p,l->A[i].p3A.p);
	}
	else if(i == 3){
		l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
		l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
		l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A.p = l->A[1].p2A.p; // ponto 2 ombros_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A.p,l->A[i].p2A.p,l->A[i].p3A.p);
	}
	else if(i == 7){
		l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
		l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
		l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A.p = l->A[1].p2A.p; // ponto 2 ombros_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A.p,l->A[i].p2A.p,l->A[i].p3A.p);
	}else if(i == 11){
		l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
		l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
		l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[l->bone].p1A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
		l->A[l->bone].p2A.p = p2;
		l->A[l->bone].p3A.p = p3;
		l->A[l->bone].anguloA = Angulo(l->A[i].p1A.p,l->A[i].p2A.p,l->A[i].p3A.p);
	}else if(i == 15){
		l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
		l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
		l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
		l->A[l->bone].anguloO = l->A[l->bone].anguloA;
		l->A[i].p1A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
		l->A[i].p2A.p = p2;
		l->A[i].p3A.p = p3;
		l->A[i].anguloA = Angulo(l->A[i].p1A.p,l->A[i].p2A.p,l->A[i].p3A.p);
	}

	else{
	l->A[l->bone].p1O.p = l->A[l->bone].p1A.p;
	l->A[l->bone].p1A.p = p1;
	l->A[l->bone].p2O.p = l->A[l->bone].p2A.p;
	l->A[l->bone].p2A.p = p2;
	l->A[l->bone].p3O.p = l->A[l->bone].p3A.p;
	l->A[l->bone].p3A.p = p3;

	l->A[l->bone].anguloO = l->A[l->bone].anguloA;
	l->A[l->bone].anguloA = Angulo(p1,p2,p3);

	if(l->bone == 14){
			l->A[15].p1O.p = l->A[15].p1A.p;
			l->A[15].p2O.p = l->A[15].p2A.p;
			l->A[15].p3O.p = l->A[15].p3A.p;
			l->A[15].p1A.p = l->A[13].p1A.p; // ponto 16 quadril_dir
			l->A[15].p2A.p = l->A[0].p1A.p; // ponto 1 quadril_centro
			l->A[15].p3A.p = l->A[8].p1A.p;;// ponto 12 quadril_esuqrdo
			l->A[15].anguloA = Angulo(l->A[15].p1A.p,l->A[15].p2A.p,l->A[15].p3A.p);
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
	printf("entrou no else");
	return true;
}
printf("deu erro");

return false;
}

//exibirLista(l);

}
*/



/*
void rotacao(NUI_SKELETON_BONE_ORIENTATION bo[], lista *l){
	double M11, M21, M31, M32, M33, x, y, z;
	int i;
	for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i){

		M11 =bo[i].hierarchicalRotation.rotationMatrix.M11;
		M21 =bo[i].hierarchicalRotation.rotationMatrix.M21;
		M31 =bo[i].hierarchicalRotation.rotationMatrix.M31;
		M32 =bo[i].hierarchicalRotation.rotationMatrix.M32;
		M33 =bo[i].hierarchicalRotation.rotationMatrix.M33;


		x = atan2(M32,M33);
		y = atan2(-M31,sqrt(pow(M32,2)+pow(M33,2)));
		z = atan2(M21,M11);

		if(i== 0){// rotacao do quadril Centro
		
			l->A[0].p1A.rotx = x;
			l->A[0].p1A.roty = y;
			l->A[0].p1A.rotz = z;

			l->A[9].p1A.rotx = x;
			l->A[9].p1A.roty = y;
			l->A[9].p1A.rotz = z;

			l->A[12].p1A.rotx = x;
			l->A[12].p1A.roty = y;
			l->A[12].p1A.rotz = z;

			l->A[15].p2A.rotx = x;
			l->A[15].p2A.roty = y;
			l->A[15].p2A.rotz = z;

		}
		else if(i == 1){ // rotacao da coluna

			l->A[0].p2A.rotx = x;
			l->A[0].p2A.roty = y;
			l->A[0].p2A.rotz = z;

			l->A[1].p1A.rotx = x;
			l->A[1].p1A.roty = y;
			l->A[1].p1A.rotz = z;
		}
		else if(i == 2){ // rotacao do ombro Centro

			l->A[0].p3A.rotx = x;
			l->A[0].p3A.roty = y;
			l->A[0].p3A.rotz = z;

			l->A[1].p2A.rotx = x;
			l->A[1].p2A.roty = y;
			l->A[1].p2A.rotz = z;

			l->A[2].p2A.rotx = x;
			l->A[2].p2A.roty = y;
			l->A[2].p2A.rotz = z;

			l->A[3].p1A.rotx = x;
			l->A[3].p1A.roty = y;
			l->A[3].p1A.rotz = z;

			l->A[6].p1A.rotx = x;
			l->A[6].p1A.roty = y;
			l->A[6].p1A.rotz = z;

		}
		else if(i == 3){ // rotacao da cabeca

			//cabeca frente
			l->A[1].p3A.rotx = x;
			l->A[1].p3A.roty = y;
			l->A[1].p3A.rotz = z;

			//cabeca lateral
			l->A[2].p1A.rotx = x;
			l->A[2].p1A.roty = y;
			l->A[2].p1A.rotz = z;

		}
		else if(i == 4){// rotacao do ombro Esquerdo
			
			//cabeca lateral
			l->A[2].p3A.rotx = x;
			l->A[2].p3A.roty = y;
			l->A[2].p3A.rotz = z;

			//braco esquerdo
			l->A[3].p2A.rotx = x;
			l->A[3].p2A.roty = y;
			l->A[3].p2A.rotz = z;

			//antibraco esqeurdo
			l->A[4].p1A.rotx = x;
			l->A[4].p1A.roty = y;
			l->A[4].p1A.rotz = z;

		}
		else if(i == 5){// rotacao do Cotovelo Esquerdo

			//braco esquerdo
			l->A[3].p3A.rotx = x;
			l->A[3].p3A.roty = y;
			l->A[3].p3A.rotz = z;

			//antibraco esquerdo
			l->A[4].p2A.rotx = x;
			l->A[4].p2A.roty = y;
			l->A[4].p2A.rotz = z;

			//mao esquerda
			l->A[5].p1A.rotx = x;
			l->A[5].p1A.roty = y;
			l->A[5].p1A.rotz = z;

		}
		else if(i == 6){// rotacao do pulso Esquerdo 

			//antibraco esquerdo
			l->A[4].p3A.rotx = x;
			l->A[4].p3A.roty = y;
			l->A[4].p3A.rotz = z;

			//mao esquerda
			l->A[5].p2A.rotx = x;
			l->A[5].p2A.roty = y;
			l->A[5].p2A.rotz = z;

		}
		else if(i == 7){// rotacao da mao Esquerda

			//mao esquerda
			l->A[5].p3A.rotx = x;
			l->A[5].p3A.roty = y;
			l->A[5].p3A.rotz = z;

		}
		else if(i == 8){// rotacao do ombro direito
			
			//braco direito
			l->A[6].p2A.rotx = x;
			l->A[6].p2A.roty = y;
			l->A[6].p2A.rotz = z;

			//antibraco direito
			l->A[7].p1A.rotx = x;
			l->A[7].p1A.roty = y;
			l->A[7].p1A.rotz = z;

		}
		else if(i == 9){// rotacao do cotovelo direito

			//braco direito
			l->A[6].p3A.rotx = x;
			l->A[6].p3A.roty = y;
			l->A[6].p3A.rotz = z;

			//antibraco direito
			l->A[7].p2A.rotx = x;
			l->A[7].p2A.roty = y;
			l->A[7].p2A.rotz = z;

			//mao direito
			l->A[8].p1A.rotx = x;
			l->A[8].p1A.roty = y;
			l->A[8].p1A.rotz = z;

		}
		else if(i == 10){// rotacao do pulso direito
			
			//antibraco direito
			l->A[7].p3A.rotx = x;
			l->A[7].p3A.roty = y;
			l->A[7].p3A.rotz = z;

			//mao direito
			l->A[8].p2A.rotx = x;
			l->A[8].p2A.roty = y;
			l->A[8].p2A.rotz = z;

		}
		else if(i == 11){// rotacao da mao direita

			//mao direito
			l->A[8].p3A.rotx = x;
			l->A[8].p3A.roty = y;
			l->A[8].p3A.rotz = z;

		}
		else if(i == 12){// rotacao do quadril Esquerdo

			//quadril
			l->A[15].p3A.rotx = x;
			l->A[15].p3A.roty = y;
			l->A[15].p3A.rotz = z;

			//perna esquerda
			l->A[9].p2A.rotx = x;
			l->A[9].p2A.roty = y;
			l->A[9].p2A.rotz = z;

			//joelho esquerdo
			l->A[10].p1A.rotx = x;
			l->A[10].p1A.roty = y;
			l->A[10].p1A.rotz = z;

		}
		else if(i == 13){// rotacao do joelho esquerdo

			//perna esquerda
			l->A[9].p3A.rotx = x;
			l->A[9].p3A.roty = y;
			l->A[9].p3A.rotz = z;

			//joelho esquerdo
			l->A[10].p2A.rotx = x;
			l->A[10].p2A.roty = y;
			l->A[10].p2A.rotz = z;

			//pe esquerdo
			l->A[11].p1A.rotx = x;
			l->A[11].p1A.roty = y;
			l->A[11].p1A.rotz = z;

		}
		else if(i == 14){// rotacao do tornozelo esquerdo
			
			//joelho esquerdo
			l->A[10].p3A.rotx = x;
			l->A[10].p3A.roty = y;
			l->A[10].p3A.rotz = z;

			//Pe esquerdo
			l->A[11].p2A.rotx = x;
			l->A[11].p2A.roty = y;
			l->A[11].p2A.rotz = z;

		}
		else if(i == 15){// rotacao do pe esquerdo

			//Pe esquerdo
			l->A[11].p3A.rotx = x;
			l->A[11].p3A.roty = y;
			l->A[11].p3A.rotz = z;

		}
		else if(i == 16){// rotacao do quadril direito
			
			//quadril
			l->A[15].p1A.rotx = x;
			l->A[15].p1A.roty = y;
			l->A[15].p1A.rotz = z;

			//perna direita
			l->A[12].p2A.rotx = x;
			l->A[12].p2A.roty = y;
			l->A[12].p2A.rotz = z;

			//joelho direito
			l->A[13].p1A.rotx = x;
			l->A[13].p1A.roty = y;
			l->A[13].p1A.rotz = z;

		}
		else if(i == 17){// rotacao do joelho direito

			//perna direito
			l->A[12].p3A.rotx = x;
			l->A[12].p3A.roty = y;
			l->A[12].p3A.rotz = z;

			//joelho direito
			l->A[13].p2A.rotx = x;
			l->A[13].p2A.roty = y;
			l->A[13].p2A.rotz = z;

			//pe direito
			l->A[14].p1A.rotx = x;
			l->A[14].p1A.roty = y;
			l->A[14].p1A.rotz = z;

		}
		else if(i == 18){// rotacao do tornozelo direito

			//joelho direito
			l->A[13].p3A.rotx = x;
			l->A[13].p3A.roty = y;
			l->A[13].p3A.rotz = z;

			//pe direito
			l->A[14].p2A.rotx = x;
			l->A[14].p2A.roty = y;
			l->A[14].p2A.rotz = z;

		}
		else if(i == 19){// rotacao do pe direito

			//pe direito
			l->A[14].p3A.rotx = x;
			l->A[14].p3A.roty = y;
			l->A[14].p3A.rotz = z;

		}
	}

}*/

/// <summary>
/// Draws a skeleton
/// </summary>
/// <param name="skel">skeleton to draw</param>
/// <param name="windowWidth">width (in pixels) of output buffer</param>
/// <param name="windowHeight">height (in pixels) of output buffer</param>
void CSkeletonBasics::DrawSkeleton(const NUI_SKELETON_DATA & skel, int windowWidth, int windowHeight)
{      
	/*File* f;
	f = fopen("dentro_do_vetor.txt","w");*/
    int i;
	//int x = 0;
	NUI_SKELETON_BONE_ORIENTATION boneOrientations[NUI_SKELETON_POSITION_COUNT];
	NuiSkeletonCalculateBoneOrientations(&skel, boneOrientations);
	
	//while(fraux){
	if(op2 == 0){
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
		if(i < NUI_SKELETON_POSITION_COUNT-2){
			if(i != 6 && i != 10 && i != 14){
			//InsereLista(skel.SkeletonPositions[i],skel.SkeletonPositions[i+1],skel.SkeletonPositions[i+2],i,l);
				}}

			 m_Points[i] = SkeletonToScreen(skel.SkeletonPositions[i], windowWidth, windowHeight);

			 if(op2==1 && fraux){
				 
				 m_Points[i] = SkeletonToScreen(fraux->p[i],windowWidth, windowHeight);
				 /*fprintf(f,"\nhfraux->p[%i] ; valor de x:%f valor de y:%f valor de z:%f\n",x,fraux->p[i].x,fraux->p[i].y,fraux->p[i].z);
				 fprintf(f,"\nm_points[%i] = valor de x:%f valor de y:%f \n",i,m_Points[i].x,m_Points[i].y);
				 x++;*/
			}
		
    }
	/*if(fraux){
	if(op2==1)fraux = fraux->prox;
	}
	else op2 = 0;*/


	if(operador == 1 ){
				InsereLista2(skel);
			}


		//rotacao(boneOrientations,l);
		/*
		if(operador == 1 ){
			int j;
			for(j=0; j<MAX;j++){
			tentativa2(l, l->A[j]);
			 } 

		}*/
		
	


    // Render Torso
    DrawBone(skel, NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
    DrawBone(skel, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

    // Left Arm
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

    // Right Arm
    DrawBone(skel, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

    // Left Leg
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
    DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

    // Right Leg
    DrawBone(skel, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
    DrawBone(skel, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);








    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
    {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[i], g_JointThickness, g_JointThickness );
		
        if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_INFERRED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointInferred);
        }
        else if ( skel.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_TRACKED )
        {
            m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
        }

	}
	}
	else{
		int cont=0;
		fraux = mv->primeiro;
	while(fraux){
				for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
					 {		
					m_Points[i] = SkeletonToScreen(fraux->p[i], windowWidth, windowHeight);
				}

		// Render Torso
    DrawBone2(NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE);
    DrawBone2(NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER);
    DrawBone2(NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT);

    // Left Arm
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);

    // Right Arm
    DrawBone2(NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);

    // Left Leg
    DrawBone2(NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT);
    DrawBone2(NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);

    // Right Leg
    DrawBone2(NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT);
    DrawBone2(NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);





    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
		{
        D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[i], g_JointThickness, g_JointThickness );
			m_pRenderTarget->DrawEllipse(ellipse, m_pBrushJointTracked);
			}

				fraux = fraux->prox;
				
			//} 
		
		}
	/*fraux = fraux->prox;
	}*/
	}
}

/// <summary>
/// Draws a bone line between two joints
/// </summary>
/// <param name="skel">skeleton to draw bones from</param>
/// <param name="joint0">joint to start drawing from</param>
/// <param name="joint1">joint to end drawing at</param>
void CSkeletonBasics::DrawBone(const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX joint0, NUI_SKELETON_POSITION_INDEX joint1)
{
    NUI_SKELETON_POSITION_TRACKING_STATE joint0State = skel.eSkeletonPositionTrackingState[joint0];
    NUI_SKELETON_POSITION_TRACKING_STATE joint1State = skel.eSkeletonPositionTrackingState[joint1];


    // If we can't find either of these joints, exit
    if (joint0State == NUI_SKELETON_POSITION_NOT_TRACKED || joint1State == NUI_SKELETON_POSITION_NOT_TRACKED)
    {
        return;
    }

    // Don't draw if both points are inferred
    if (joint0State == NUI_SKELETON_POSITION_INFERRED && joint1State == NUI_SKELETON_POSITION_INFERRED)
    {
        return;
    }

    // We assume all drawn bones are inferred unless BOTH joints are tracked
    if (joint0State == NUI_SKELETON_POSITION_TRACKED && joint1State == NUI_SKELETON_POSITION_TRACKED)
    {
        m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneTracked, g_TrackedBoneThickness);
    }
    else
    {
        m_pRenderTarget->DrawLine(m_Points[joint0], m_Points[joint1], m_pBrushBoneInferred, g_InferredBoneThickness);
    }
}

/// <summary>
/// Converts a skeleton point to screen space
/// </summary>
/// <param name="skeletonPoint">skeleton point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F CSkeletonBasics::SkeletonToScreen(Vector4 skeletonPoint, int width, int height)
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

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT CSkeletonBasics::EnsureDirect2DResources()
{
    HRESULT hr = S_OK;

    // If there isn't currently a render target, we need to create one
    if (NULL == m_pRenderTarget)
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
        hr = m_pD2DFactory->CreateHwndRenderTarget(
            rtProps,
            D2D1::HwndRenderTargetProperties(GetDlgItem( m_hWnd, IDC_VIDEOVIEW), size),
            &m_pRenderTarget
            );
        if ( FAILED(hr) )
        {
            SetStatusMessage(L"Couldn't create Direct2D render target!");
            return hr;
        }

        //light green
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.27f, 0.75f, 0.27f), &m_pBrushJointTracked);

        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, 1.0f), &m_pBrushJointInferred);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green, 1.0f), &m_pBrushBoneTracked);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Gray, 1.0f), &m_pBrushBoneInferred);
    }

    return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void CSkeletonBasics::DiscardDirect2DResources( )
{
    SafeRelease(m_pRenderTarget);

    SafeRelease(m_pBrushJointTracked);
    SafeRelease(m_pBrushJointInferred);
    SafeRelease(m_pBrushBoneTracked);
    SafeRelease(m_pBrushBoneInferred);
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
void CSkeletonBasics::SetStatusMessage(WCHAR * szMessage)
{
    SendDlgItemMessageW(m_hWnd, IDC_STATUS, WM_SETTEXT, 0, (LPARAM)szMessage);
}