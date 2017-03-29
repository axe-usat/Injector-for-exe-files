// inyeccion.cpp: define el punt
#include <stdio.h>
#include <windows.h>
#include <Tlhelp32.h>
#include "stdafx.h"

void error(char *err);
static DWORD WINAPI myFunc(LPVOID data);

HANDLE myProc = NULL;

// Con esto cargaremos punteros a LoadLibrary y GetProcAddress en nuestra estrucura de datos
typedef int (WINAPI *datLoadLibrary)(LPCTSTR);
typedef int (WINAPI *datGetProcAddress)(HMODULE, LPCSTR);

struct tdata {
	char lnUser32[50]; // -> "User32.dll"
	char fnMessageBoxA[50]; // -> "MessageBoxA"
	datLoadLibrary apiLoadLibrary; // Puntero a LoadLibrary
	datGetProcAddress apiGetProcAddress; // Puntero a GetProcAddress
	char Msg[50]; // Texto que usaremos en MessageBoxA
};

int main(int argc, char *argv[])
{
	if (argc<2)
		error("Uso: hook.exe PROCESO\n");
	// Esta es nuestra estructura de argumentos
	struct tdata thData;

	strcpy(thData.lnUser32, "User32.dll");
	strcpy(thData.fnMessageBoxA, "MessageBoxA");
	strcpy(thData.Msg, "Hola!");
	thData.apiLoadLibrary = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
	thData.apiGetProcAddress = GetProcAddress(GetModuleHandle("kernel32.dll"), "GetProcAddress");

	int funcSize = 600; 	// El tamaño de nuestra función. Podria calcularse de forma exacta, pero como ejemplo
							// exagerado nos vale .

	HANDLE processList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pInfo;
	BOOL st = TRUE;
	pInfo.dwSize = sizeof(PROCESSENTRY32);
	Process32First(processList, &pInfo);
	int myPid = 0;
	do
	{
		if (strcmp(pInfo.szExeFile, argv[1]) == 0)
		{
			myPid = pInfo.th32ProcessID;
			break;
		}
		Process32Next(processList, &pInfo);
	} while (st != FALSE);

	// Abrir proceso
	printf("[+] Abriendo proceso %i\n", myPid);
	myProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, myPid);
	if (myProc == NULL)
		error("[-] Error abriendo proceso.\n");
	else
		printf("[+] Proceso abierto.\n");

	// Reservar memoria para argumentos
	LPVOID dirToArg = VirtualAllocEx(myProc, NULL, sizeof(thData), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (dirToArg == NULL)
		error("[-] Error reservando memoria para arg.\n");
	else
		printf("[+] Memoria reservada para arg (%i bytes).\n", sizeof(thData));
	// Escribir argumentos
	SIZE_T written = 0;
	if (WriteProcessMemory(myProc, dirToArg, (LPVOID)&thData, sizeof(thData), &written) == 0)
		error("[-] Error escribiendo la estructura de datos.\n");
	else
		printf("[+] Memoria escrita (arg %i bytes).\n", written);

	// Reservar memoria para la funcion
	LPVOID dirToWrite = VirtualAllocEx(myProc, NULL, funcSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (dirToWrite == NULL)
		error("[-] Error rreservando memoria para codigo.\n");
	else
		printf("[+] Memoria reservada para codigo (%i bytes).\n", funcSize);

	// Escribimos el codigo de nuestra funcion
	if (WriteProcessMemory(myProc, dirToWrite, (LPVOID)myFunc, funcSize, &written) == 0)
		error("[-]Error escribiendo memoria.\n");
	else
		printf("[+] Memoria escrita (codigo).\n");

	// Lanzamos el hilo
	HANDLE rThread = CreateRemoteThread(myProc, NULL, 0, (LPTHREAD_START_ROUTINE)dirToWrite, dirToArg, 0, NULL);
	if (rThread == NULL)
		error("[-] Error lanzando hilo.\n");
	else
		printf("[+] Hilo lanzado.\n");

	CloseHandle(myProc);
	return 0;
}

void error(char *err)
{
	if (myProc != NULL) CloseHandle(myProc);
	printf("%s", err);
	exit(0);
}

static DWORD WINAPI myFunc(LPVOID data)
{
	// Cargamos nuestros datos en una estructura como la que hicimos
	struct tdata *thData;
	thData = data;
	// Podemos conseguir cualquier API con estas dos, siempre que tengamos su nombre en la estructura
	void *apiDir = (void *)thData->apiGetProcAddress((HANDLE)thData->apiLoadLibrary(thData->lnUser32), thData->fnMessageBoxA);
	// Puntero a funcion similar a MessageBoxA
	INT WINAPI(*myMessageBox)(HWND, LPCSTR, LPCSTR, UINT);
	myMessageBox = apiDir;
	myMessageBox(NULL, thData->MSG, thData->MSG, 0);
	return;
}