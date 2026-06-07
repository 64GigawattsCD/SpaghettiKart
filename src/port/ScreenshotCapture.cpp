#include "ScreenshotCapture.h"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>

#include <gdiplus.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

namespace {

constexpr wchar_t kWindowTitle[] = L"Spaghetti Kart (DirectX 11)";
constexpr wchar_t kScreenshotDirectory[] = L"C:\\Users\\Craig\\OneDrive\\Documents\\mk64 Arcade\\screenshots";

std::atomic_bool sScreenshotInFlight(false);
std::once_flag sDpiAwareOnce;

int GetEncoderClsid(const wchar_t* format, CLSID* clsid) {
    UINT numEncoders = 0;
    UINT encoderBytes = 0;
    Gdiplus::GetImageEncodersSize(&numEncoders, &encoderBytes);
    if ((encoderBytes == 0) || (numEncoders == 0)) {
        return -1;
    }

    std::vector<BYTE> encoderBuffer(encoderBytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(encoderBuffer.data());
    if (Gdiplus::GetImageEncoders(numEncoders, encoderBytes, encoders) != Gdiplus::Ok) {
        return -1;
    }

    for (UINT i = 0; i < numEncoders; i++) {
        if (wcscmp(encoders[i].MimeType, format) == 0) {
            *clsid = encoders[i].Clsid;
            return static_cast<int>(i);
        }
    }

    return -1;
}

std::wstring MakeScreenshotPath() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm localTime = {};
    localtime_s(&localTime, &time);

    std::wstringstream stream;
    stream << kScreenshotDirectory << L"\\controller-capture-" << std::put_time(&localTime, L"%Y%m%d-%H%M%S")
           << L"-" << std::setw(3) << std::setfill(L'0') << millis.count() << L".png";
    return stream.str();
}

HWND GetCaptureWindow() {
    HWND window = FindWindowW(nullptr, kWindowTitle);
    if (window != nullptr) {
        return window;
    }

    window = GetActiveWindow();
    if (window != nullptr) {
        return window;
    }

    return GetForegroundWindow();
}

std::string NarrowPath(const std::wstring& path) {
    if (path.empty()) {
        return std::string();
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string();
    }

    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

void CaptureWindowScreenshotWorker() {
    struct CaptureGuard {
        ~CaptureGuard() {
            sScreenshotInFlight.store(false);
        }
    } guard;

    try {
        std::call_once(sDpiAwareOnce, []() {
            SetProcessDPIAware();
        });

        HWND window = GetCaptureWindow();
        if (window == nullptr) {
            std::printf("[ArcadeKart] Screenshot skipped: game window was not found.\n");
            return;
        }

        if (IsIconic(window)) {
            std::printf("[ArcadeKart] Screenshot skipped: game window is minimized.\n");
            return;
        }

        RECT rect = {};
        if (!GetWindowRect(window, &rect)) {
            std::printf("[ArcadeKart] Screenshot skipped: window bounds were not available.\n");
            return;
        }

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if ((width <= 0) || (height <= 0)) {
            std::printf("[ArcadeKart] Screenshot skipped: window bounds were empty.\n");
            return;
        }

        HDC screenDc = GetDC(nullptr);
        if (screenDc == nullptr) {
            std::printf("[ArcadeKart] Screenshot skipped: screen DC was not available.\n");
            return;
        }

        HDC memoryDc = CreateCompatibleDC(screenDc);
        if (memoryDc == nullptr) {
            ReleaseDC(nullptr, screenDc);
            std::printf("[ArcadeKart] Screenshot skipped: memory DC was not available.\n");
            return;
        }

        HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
        if (bitmap == nullptr) {
            DeleteDC(memoryDc);
            ReleaseDC(nullptr, screenDc);
            std::printf("[ArcadeKart] Screenshot skipped: bitmap allocation failed.\n");
            return;
        }

        HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
        if ((previousBitmap == nullptr) || (previousBitmap == HGDI_ERROR)) {
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            ReleaseDC(nullptr, screenDc);
            std::printf("[ArcadeKart] Screenshot skipped: bitmap selection failed.\n");
            return;
        }

        const BOOL captured =
            BitBlt(memoryDc, 0, 0, width, height, screenDc, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
        SelectObject(memoryDc, previousBitmap);
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);

        if (!captured) {
            DeleteObject(bitmap);
            std::printf("[ArcadeKart] Screenshot skipped: window capture failed.\n");
            return;
        }

        ULONG_PTR gdiplusToken = 0;
        Gdiplus::GdiplusStartupInput startupInput;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok) {
            DeleteObject(bitmap);
            std::printf("[ArcadeKart] Screenshot skipped: PNG writer failed to start.\n");
            return;
        }

        CLSID pngClsid = {};
        const std::wstring path = MakeScreenshotPath();
        std::error_code directoryError;
        std::filesystem::create_directories(kScreenshotDirectory, directoryError);
        if (directoryError) {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            DeleteObject(bitmap);
            std::printf("[ArcadeKart] Screenshot skipped: screenshot directory failed: %s\n",
                        directoryError.message().c_str());
            return;
        }

        bool saved = false;
        {
            Gdiplus::Bitmap png(bitmap, nullptr);
            saved = (GetEncoderClsid(L"image/png", &pngClsid) >= 0) &&
                    (png.Save(path.c_str(), &pngClsid, nullptr) == Gdiplus::Ok);
        }

        Gdiplus::GdiplusShutdown(gdiplusToken);
        DeleteObject(bitmap);

        if (saved) {
            std::printf("[ArcadeKart] Screenshot captured: %s\n", NarrowPath(path).c_str());
        } else {
            std::printf("[ArcadeKart] Screenshot skipped: PNG save failed.\n");
        }
    } catch (const std::exception& exception) {
        std::printf("[ArcadeKart] Screenshot skipped after exception: %s\n", exception.what());
    } catch (...) {
        std::printf("[ArcadeKart] Screenshot skipped after unknown exception.\n");
    }
}

} // namespace

extern "C" void ArcadeKart_CaptureWindowScreenshot(void) {
    bool expected = false;
    if (!sScreenshotInFlight.compare_exchange_strong(expected, true)) {
        std::printf("[ArcadeKart] Screenshot skipped: capture already in progress.\n");
        return;
    }

    try {
        std::thread(CaptureWindowScreenshotWorker).detach();
    } catch (...) {
        sScreenshotInFlight.store(false);
        std::printf("[ArcadeKart] Screenshot skipped: capture worker failed to start.\n");
    }
}

#else

extern "C" void ArcadeKart_CaptureWindowScreenshot(void) {
    std::printf("[ArcadeKart] Screenshot capture is only implemented on Windows.\n");
}

#endif
