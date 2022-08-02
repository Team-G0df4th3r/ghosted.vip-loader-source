#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <fstream>

#include <string>

#include <d3d9.h>
#include <d3dx9.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx9.h"
#include "ImGui/imgui_impl_win32.h"

#include "curl/curl/curl.h"

#include "Inject.h"

#include "MD5/MD5.h"
#include "XorComp.hpp"
#include "lazy.hpp"
#include "Antidebug.h"

#include "auth.hpp"
#include "logo.h"
#include "Verdana.h"
#include "bytes.h"

using std::string;
using std::ifstream;

std::string name = "PandaWare.xyz";
std::string ownerid = "iqVDFHjhTn";
std::string secret = "929c0f2310b3314040802bc947e0f5e8c93d1d5a3b9ce6e4f6597323a5cc02e8";
std::string version = "1.0";
std::string url = "https://keyauth.win/api/1.1/";
std::string sslPin = "ssl pin key (optional)";

KeyAuth::api KeyAuthApp(name, ownerid, secret, version, url, sslPin);

IDirect3DDevice9* g_pd3dDevice = nullptr;
D3DPRESENT_PARAMETERS g_d3dpp;
IDirect3D9* pD3D = nullptr;

ImVec2 pos = { 0, 0 };

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	switch (message) {
	case WM_SIZE:
		if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			g_d3dpp.BackBufferWidth = LOWORD(lParam);
			g_d3dpp.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
			if (hr == D3DERR_INVALIDCALL) IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
		break;
	case WM_DESTROY:
		LI_FN(PostQuitMessage)(0);
		return 0;
	}

	return LI_FN(DefWindowProcA).cached()(hWnd, message, wParam, lParam);
}

int writer(char* data, size_t size, size_t nmemb, string* buffer)
{
	int result = 0;
	if (buffer != NULL)
	{
		buffer->append(data, size * nmemb);
		result = size * nmemb;
	}
	return result;
}

__forceinline string GetHwid()
{
	DWORD dwVolumeSerialNumber;
	string ret;
	if (LI_FN(GetVolumeInformationA)(_("C:\\"), nullptr, 0, &dwVolumeSerialNumber, nullptr, nullptr, nullptr, 0))
		ret = md5(md5(std::to_string((dwVolumeSerialNumber << 3) - 4)));
	else
		ret = string();

	return ret;
}

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

bool inject() {
	PVOID rData = reinterpret_cast<BYTE*>(bytes);

	pIDH = (PIMAGE_DOS_HEADER)rData;
	pINH = (PIMAGE_NT_HEADERS)((LPBYTE)rData + pIDH->e_lfanew);

	DWORD pid = MyGetProcessId("csgo.exe");
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	image = VirtualAllocEx(hProcess, NULL, pINH->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(hProcess, image, rData, pINH->OptionalHeader.SizeOfHeaders, NULL);
	pISH = (PIMAGE_SECTION_HEADER)(pINH + 1);
	for (i = 0; i < pINH->FileHeader.NumberOfSections; i++)
	{
		WriteProcessMemory(hProcess, (PVOID)((LPBYTE)image + pISH[i].VirtualAddress),
			(PVOID)((LPBYTE)rData + pISH[i].PointerToRawData), pISH[i].SizeOfRawData, NULL);
	}
	mem = VirtualAllocEx(hProcess, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memset(&ManualInject, 0, sizeof(MANUAL_INJECT));

	ManualInject.ImageBase = image;
	ManualInject.NtHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)image + pIDH->e_lfanew);
	ManualInject.BaseRelocation = (PIMAGE_BASE_RELOCATION)((LPBYTE)image + pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	ManualInject.ImportDirectory = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)image + pINH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	ManualInject.fnLoadLibraryA = LoadLibraryA;
	ManualInject.fnGetProcAddress = GetProcAddress;

	WriteProcessMemory(hProcess, mem, &ManualInject, sizeof(MANUAL_INJECT), NULL);
	WriteProcessMemory(hProcess, (PVOID)((PMANUAL_INJECT)mem + 1), (LPCVOID)LoadDll, (DWORD)LoadDllEnd - (DWORD)LoadDll, NULL);

	hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((PMANUAL_INJECT)mem + 1), mem, 0, NULL);

	WaitForSingleObject(hThread, INFINITE);
	GetExitCodeThread(hThread, &ExitCode);
	return true;
}

__forceinline void RemoveFile(string file)
{
	LI_FN(SetFileAttributesA)(file.c_str(), FILE_ATTRIBUTE_NORMAL);
	LI_FN(DeleteFileA)(file.c_str());
}

BOOL WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
	AllocConsole();
	fprintf_s((FILE*)stdout, "CONOUT$", stdout);

	LI_FN(CreateThread)(nullptr, 0, Thread, nullptr, 0, nullptr);
	KeyAuthApp.init();
	LI_FN(LoadLibraryA)(_("d3d9.dll"));
	LI_FN(LoadLibraryA)(_("d3dx9_43.dll"));
	LI_FN(LoadLibraryA)(_("XINPUT1_3.dll"));
	LI_FN(LoadLibraryA)(_("ntdll.dll"));

	RECT rect;
	LI_FN(GetWindowRect)(LI_FN(GetDesktopWindow)(), &rect);
	pos.x = rect.right / 2 - 200;
	pos.y = rect.bottom / 2 - 200;

	IDirect3DTexture9* texture = nullptr;
	ImGuiStyle* style = nullptr;
	ImGuiIO* io = nullptr;
	ImVec4* colors = nullptr;
	bool opened = true;
	int phase = 0;
	char* username = new char[32];
	char* password = new char[32];
	char* license = new char[64];
	bool hovered = false;
	POINT lpx;
	MSG msg;
	HWND hwnd = 0;

	ZeroMemory(username, 32);
	ZeroMemory(password, 32);
	ZeroMemory(license, 64);

	{
		ifstream credintials_r(_("C:\\Ghosted.vip\\data.ini"));
		if (credintials_r.is_open())
		{
			int index = 0;
			string temp;
			while (getline(credintials_r, temp))
			{
				if (index == 0 && !temp.empty())
				{
					strcpy(username, temp.c_str());
					index++;
				}
				else if (index == 1 && !temp.empty())
				{
					strcpy(password, temp.c_str());
				}
			}
		}
		credintials_r.close();
	}

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hInstance = hInst;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = _("Ghosted.vip");
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!LI_FN(RegisterClassExA)(&wc))
	{
		LI_FN(MessageBoxA)(nullptr, _("Failed to open window"), _("Ghosted.vip"), 0);
		goto mainend;
	}

	hwnd = LI_FN(CreateWindowExA)(0, wc.lpszClassName, _("Ghosted.vip"), WS_POPUP, 10, 10, 400, 400, nullptr, nullptr, wc.hInstance, nullptr);
	if (!hwnd)
	{
		LI_FN(UnregisterClassA)(wc.lpszClassName, wc.hInstance);
		LI_FN(MessageBoxA)(nullptr, _("Failed to open window"), _("Ghosted.vip"), 0);
		goto mainend;
	}

	pD3D = LI_FN(Direct3DCreate9)(D3D_SDK_VERSION);
	if (!pD3D)
	{
		LI_FN(UnregisterClassA)(wc.lpszClassName, wc.hInstance);
		LI_FN(MessageBoxA)(nullptr, _("Failed to initialize DirectX"), _("Ghosted.vip"), 0);
		goto mainend;
	}

	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
	{
		pD3D->Release();
		LI_FN(UnregisterClassA)(wc.lpszClassName, wc.hInstance);
		goto mainend;
	}

	ImGui::CreateContext();

	io = &ImGui::GetIO(); (void)io;
	io->Fonts->AddFontFromMemoryCompressedTTF(verdana_compressed_data, verdana_compressed_size, 14.0f);

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	LI_FN(ShowWindow)(hwnd, SW_SHOW);
	LI_FN(UpdateWindow)(hwnd);
	{
		style = &ImGui::GetStyle();
		style->WindowPadding = { 15, 15 };
		style->FramePadding = { 5, 5 };
		style->ItemSpacing = { 15, 8 };
		style->ItemInnerSpacing = { 6, 6 };
		style->TouchExtraPadding = { 0, 0 };
		style->IndentSpacing = 25;
		style->ScrollbarSize = 14;
		style->GrabMinSize = 8;
		style->WindowBorderSize = 0.5f;
		style->PopupBorderSize = 0;
		style->FrameBorderSize = 0;
		style->WindowRounding = 0;
		style->FrameRounding = 12;
		style->PopupRounding = 0;
		style->ScrollbarRounding = 9;
		style->GrabRounding = 12;
		style->TabRounding = 4;
		style->WindowTitleAlign = { 0.5f, 0.5f };

		colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.5f, 0.59f, 0.98f, 0.60f);
		colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.23f, 0.46f, 0.81f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.23f, 0.46f, 0.81f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
		colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	}
	ZeroMemory(&msg, sizeof(MSG));
	LI_FN(GetCursorPos)(&lpx);
	if (texture == nullptr) {
		LI_FN(D3DXCreateTextureFromFileInMemory)(g_pd3dDevice, byteImage, sizeof(byteImage), &texture);
	}
	while (msg.message != WM_QUIT)
	{
		if (LI_FN(PeekMessageA).cached()(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			LI_FN(TranslateMessage).cached()(&msg);
			LI_FN(DispatchMessageA).cached()(&msg);
			continue;
		}

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (!opened) break;

		LI_FN(SetWindowPos).cached()(hwnd, 0, pos.x, pos.y, 400, 400, 0);
		ImGui::SetNextWindowSize({ 400, 400 });
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::Begin(_("Ghosted.vip"), &opened, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		if (texture != nullptr) {
			ImGui::SetCursorPos({ -18, 0 });
			ImGui::Image(texture, { 440, 325 });
			ImGui::NewLine();
		}

		ImGui::SetCursorPosY(256.0f);

		if (phase == 0)
		{
			ImGui::PushItemWidth(185.0f);

			ImGui::SetCursorPosY(260.0f);
			ImGui::Text(_("Username:")); ImGui::SameLine(90);
			ImGui::SetCursorPosY(256.0f);
			ImGui::InputText(_("##username"), username, 32);
			if (ImGui::IsItemHovered()) hovered = true;

			ImGui::SetCursorPosY(287.0f);
			ImGui::Text(_("Password:")); ImGui::SameLine(90);
			ImGui::SetCursorPosY(283.0f);
			ImGui::InputText(_("##password"), password, 32, ImGuiInputTextFlags_Password);
			if (ImGui::IsItemHovered()) hovered = true;
			ImGui::SetCursorPosY(314.0f);
			ImGui::Text(_("License:")); ImGui::SameLine(90);
			ImGui::SetCursorPosY(310.0f);
			ImGui::InputText(_("##license"), license, 64);
			if (ImGui::IsItemHovered()) hovered = true;

			ImGui::PopItemWidth();

			ImGui::NewLine();
			ImGui::SetCursorPosX(155);

			if (ImGui::Button(_("Login")))
			{
				KeyAuthApp.login(username, password);
				if (KeyAuthApp.data.success) {
					phase += 1;
				}
			}

			ImGui::SameLine();
			if (ImGui::Button(_("Register")))
			{
				KeyAuthApp.regstr(username, password, license);
			}

			if (ImGui::IsItemHovered()) hovered = true;
		}
		else if (phase == 1)
		{
			string welcome = _("Welcome back, ");
			welcome += username;
			auto welcome_size = ImGui::CalcTextSize(welcome.c_str());
			ImGui::SetCursorPosX(200.0f - welcome_size.x / 2.0f);
			ImGui::Text(welcome.c_str());

			static int ver = 0;
			ImGui::SetCursorPos({ 138, 285 });
			ImGui::Text(_("Choose version"));
			ImGui::SetCursorPos({ 122, 315 });
			ImGui::PushItemWidth(140.f);
			ImGui::Combo(_("##shit"), &ver, _("HvH\0Legit\0Beta\0"));
			ImGui::PopItemWidth();
			ImGui::SetCursorPos({ 172, 340 });
			if (ImGui::Button(_("Load"), ImVec2(45, 30)))
			{
				DWORD pId = MyGetProcessId(_("csgo.exe"));

				if (pId) {
					if (inject()) { //byte array ist in der injection funktion oder in bytes.h
						phase++;
					}
				}
				else {
					LI_FN(MessageBoxA)(nullptr, _("Run CS:GO first"), _("Ghosted.vip"), MB_ICONERROR);
				}
			}

			if (ImGui::IsItemHovered()) hovered = true;
		}
		else if (phase == 2)
		{
			ImVec2 text_size = ImGui::CalcTextSize(_("You may close loader now"));
			ImGui::SetCursorPosX(200.0f - text_size.x / 2.0f);
			ImGui::Text(_("You may close loader now"));
		}

		ImGui::End();

		ImGui::EndFrame();

		g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, false);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
		g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, false);
		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}
		auto result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

		if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
			if (hr == D3DERR_INVALIDCALL)
				IM_ASSERT(0);
			ImGui_ImplDX9_CreateDeviceObjects();
		}

		POINT px;
		LI_FN(GetCursorPos).cached()(&px);
		if (LI_FN(GetAsyncKeyState).cached()(0x1) && LI_FN(GetForegroundWindow).cached()() == hwnd && !hovered)
		{
			pos.x += px.x - lpx.x;
			pos.y += px.y - lpx.y;
		}

		lpx = px;
		hovered = false;
	}

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	if (pD3D) { pD3D->Release(); pD3D = NULL; }

	LI_FN(DestroyWindow)(hwnd);
	LI_FN(UnregisterClassA)(wc.lpszClassName, wc.hInstance);

mainend:

	return 0;
}