// This file is going to hold the entrypoint for Windows

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;


struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width; 
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Height;
    int Width;
};

// TODO: This is a global for now.
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackbuffer;

// NOTE: Support for XInputGetState
// These stub functions get loaded into our function pointers just in case the
// Windows functions can't load. This way we don't crash.
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwuserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: Support for XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
    // Here we're going to try to do the steps the Windows loader would do
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary) {
        // TODO: Diagnostic here just in case we didn't get 1_4
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE: Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // NOTE: Get a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(DSBUFFERDESC);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                // TODO: Check to see if we want DSBCAPS_GLOBALFOCUS

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0))) {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
                        // NOTE: Finally set the format
                        OutputDebugStringA("Primary buffer format was set.\n");
                    } else {
                        // TODO: Diagnostic
                    }
                } else {
                    // TODO: Diagnostic
                }
            } else {
                // TODO: Diagnostic
            }

            // TODO: Possibly use DSBCAPS_GETCURRENTPOSITION2 for accuracy?
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(DSBUFFERDESC);
            BufferDescription.dwFlags = 0;
            BufferDescription.lpwfxFormat = &WaveFormat;
            LPDIRECTSOUNDBUFFER SecondaryBuffer;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
            else 
            {

            }

            // NOTE: Start it playing
        }
        else {
            // TODO: Give a diagnostic
        }
    }
    else {
        // TODO: Give a diagnostic
    }
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    // C doesn't understand void* so we need to cast this to something it can
    // understand, which in this place will be a pointer to bytes, or char*.
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Width * Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width*Buffer->BytesPerPixel;

    // TODO: Probably clear this to black
    //
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext,
                           int WindowWidth, int WindowHeight, int X, int Y)
{
    // TODO: Aspect ratio correction
    StretchDIBits(DeviceContext,
                  //X, Y, Width, Height,
                  //X, Y, Width, Height,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message) {
        case WM_SIZE:
            {
            } break;

        case WM_DESTROY:
            {
                // TODO: Handle this as an error -- recreate window?
                Running = false;
            } break;

        case WM_CLOSE:
            {
                // TODO: Handle this with a message to the user
                Running = false;
            } break;

        case WM_ACTIVATEAPP:
            {
                OutputDebugStringA("WM_ACTIVATEAPP\n");
            } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                uint32 VKCode = WParam;
                bool WasDown = ((LParam & (1 << 30)) != 0);
                bool IsDown = ((LParam & (1 << 31)) == 0);
                if (WasDown != IsDown)
                {
                    if (VKCode == 'W') 
                    {
                    }
                    else if (VKCode == 'A') 
                    {
                    }
                    else if (VKCode == 'S') 
                    {
                    }
                    else if (VKCode == 'D') 
                    {
                    }
                    else if (VKCode == 'Q')
                    {
                    }
                    else if (VKCode == 'E')
                    {
                    }
                    else if (VKCode == VK_UP) 
                    {
                    }
                    else if (VKCode == VK_DOWN) 
                    {
                    }
                    else if (VKCode == VK_LEFT) 
                    {
                    }
                    else if (VKCode == VK_RIGHT) 
                    {
                    }
                    else if (VKCode == VK_ESCAPE) 
                    {
                        OutputDebugStringA("ESCAPE: ");
                        if (IsDown)
                        {
                            OutputDebugStringA("IsDown ");
                        }
                        if (WasDown)
                        {
                            OutputDebugStringA("WasDown");
                        }
                        OutputDebugStringA("\n");
                    }
                    else if (VKCode == VK_SPACE)
                    {
                    }
                }

                bool AltKeyWasDown = (LParam & (1 << 29)) != 0;
                if ((VKCode == VK_F4) && AltKeyWasDown) {
                    Running = false; 
                }
            } break;


        case WM_PAINT:
            {
                PAINTSTRUCT Paint;
                HDC DeviceContext = BeginPaint(Window, &Paint);
                int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
                int Width = Paint.rcPaint.right - Paint.rcPaint.left;
                int X = Paint.rcPaint.left;
                int Y = Paint.rcPaint.top;

                RECT ClientRect;
                GetClientRect(Window, &ClientRect);

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height, X, Y);
                EndPaint(Window, &Paint);
            } break;

        default:
            {
                //OutputDebugStringA("default\n");
                Result = DefWindowProc(Window, Message, WParam, LParam);
            } break;
    }

    return Result;
}

int WINAPI wWinMain(HINSTANCE Instance,
                    HINSTANCE PrevInstance,
                    PWSTR CommandLine,
int ShowCode)
{
    Win32LoadXInput();

    WNDCLASSA WindowClass = {};
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        if (Window)
        {
            Running = true;

            int XOffset = 0;
            int YOffset = 0;

            Win32InitDSound(Window, 48000, 48000*sizeof(int16)*2);

            while (Running)
            {
                MSG Message;
                while (PeekMessageA(&Message, Window, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        Running = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                // TODO: Should we poll this more frequently?
                for (DWORD ControllerIndex = 0;
                ControllerIndex < XUSER_MAX_COUNT;
                ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE: This controller is plugged in
                        // TODO: See if ControllerState.dwPacketNumber increments too rapidly?
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);

                        int16 LStickX = Pad->sThumbLX;
                        int16 LStickY = Pad->sThumbLY;

                        if (AButton)
                        {
                            ++YOffset;
                        }
                    }
                    else
                {
                        // NOTE: This controller is unavailable
                    }
                }

                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

                HDC DeviceContext = GetDC(Window);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height, 0, 0);
                ReleaseDC(Window, DeviceContext);

                --XOffset;
                //++YOffset;
            }
        }
        else
    {
            // TODO: Logging
        }
    }
    else
{
        // TODO: Logging
    }

    return 0;
}
