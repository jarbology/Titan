#include <iostream>
#include <exception>
#define HAVE_M_PI
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_keycode.h>
#include <moai-core/host.h>
#include <host-modules/aku_modules.h>
extern "C"
{
	#include <lua.h>
}

#include "sim.h"
#include "Titan.h"

using namespace std;

namespace InputDevice
{
	enum Enum
	{
		Main,

		Count
	};
}

namespace MainSensor
{
	enum Enum
	{
		RawKeyboard,
		TextInput,
		Touch,

		Mouse,
		MouseWheel,
		MouseLeft,
		MouseMiddle,
		MouseRight,

		Count
	};
}

class Init
{
public:
	typedef void(*InitFunc)();
	typedef void(*ShutdownFunc)();

	Init(InitFunc initFunc, ShutdownFunc shutdownFunc)
		:mShutdownFunc(shutdownFunc)
	{
		initFunc();
	}

	~Init()
	{
		mShutdownFunc();
	}

private:
	ShutdownFunc mShutdownFunc;
};

template<typename T>
class AKUContextWrapper
{
public:
	AKUContextWrapper(T* userData)
	{
		mContextId = AKUCreateContext();
		AKUSetUserdata(userData);
	}

	~AKUContextWrapper()
	{
		AKUDeleteContext(mContextId);
	}

private:
	AKUContextID mContextId;
};

class LuaPanicException: public std::exception
{
public:
	LuaPanicException(STLString errorMsg)
		:mErrorMsg(errorMsg)
	{}

	virtual ~LuaPanicException() {}

	virtual const char* what() const throw()
	{
		return mErrorMsg.c_str();
	}

private:
	STLString mErrorMsg;
};


class Sim
{
public:
	Sim(int argc, char* argv[])
		:mArgc(argc)
		,mArgv(argv)
		,mWindow(NULL)
		,mGLContext(NULL)
		,mWindowX(SDL_WINDOWPOS_UNDEFINED)
		,mWindowY(SDL_WINDOWPOS_UNDEFINED)
		,mAppInitialize(AKUAppInitialize, AKUAppFinalize)
		,mModulesAppInitialize(AKUModulesAppInitialize, AKUModulesAppFinalize)
	{
		SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}

	~Sim()
	{
		SDL_Quit();
	}

	ExitReason::Enum run()
	{
		cout << "-------------------------------" << endl
			 << "Initializing Titan" << endl
			 << "-------------------------------" << endl;

		//Initialize context
		AKUContextWrapper<Sim> context(this);
		AKUSetArgv(mArgv);

		AKUModulesContextInitialize();
		AKUModulesRunLuaAPIWrapper();

		//Register host addons
		REGISTER_LUA_CLASS(Titan);

		//Initialize input
		AKUSetInputConfigurationName("AKUTitan");
		AKUReserveInputDevices(InputDevice::Count);

		//Main input device
		AKUSetInputDevice(InputDevice::Main, "device");
		AKUReserveInputDeviceSensors(InputDevice::Main, MainSensor::Count);
		AKUSetInputDeviceKeyboard(InputDevice::Main, MainSensor::RawKeyboard, "keyboard");
		AKUSetInputDeviceKeyboard(InputDevice::Main, MainSensor::TextInput, "textInput");
		AKUSetInputDevicePointer(InputDevice::Main, MainSensor::Mouse, "mouse");
		AKUSetInputDeviceWheel(InputDevice::Main, MainSensor::MouseWheel, "mouseWheel");
		AKUSetInputDeviceButton(InputDevice::Main, MainSensor::MouseLeft, "mouseLeft");
		AKUSetInputDeviceButton(InputDevice::Main, MainSensor::MouseMiddle, "mouseMiddle");
		AKUSetInputDeviceButton(InputDevice::Main, MainSensor::MouseRight, "mouseRight");
		AKUSetInputDeviceTouch(InputDevice::Main, MainSensor::Touch, "touch");

		//Register callbacks
		AKUSetFunc_OpenWindow(thunk_openWindow);
		AKUSetFunc_ShowCursor(showCursor);
		AKUSetFunc_HideCursor(hideCursor);

		//Run main script
		lua_atpanic(AKUGetLuaState(), onLuaPanic);//Prevent lua from quitting in panic
		AKURunString("MOAISim.setTraceback(function() end)");//Default trace handler

		bool toolMode = false;

		try
		{
			if(mArgc < 2)
			{
				AKURunScript("main.lua");
			}
			else
			{
				for(int i = 1; i < mArgc; ++i)
				{
					const char* arg = mArgv[i];
					if(strcmp(arg, "-s") == 0 && (++i < mArgc))
					{
						const char* script = mArgv[i];
						AKURunString(script);
					}
					else if(strcmp(arg, "-t") == 0)
					{
						toolMode = true;
					}
					else
					{
						AKURunScript(arg);
					}
				}
			}
		}
		catch(LuaPanicException e)
		{
			cout << "Error in script: " << e.what() << endl;
		}

		if(toolMode)
		{
			return ExitReason::Normal;
		}

		if(!mWindow)
		{
			cout << "-------------------------------" << endl
				 << "Failed to create render window" << endl
				 << "-------------------------------" << endl;
			return ExitReason::Error;
		}

		//Main loop
		mRunning = true;
		Titan& titan = Titan::Get();
		mExitReason = ExitReason::Restart;

		try
		{
			while(titan.Update() && mRunning)
			{
				SDL_Event event;
				while(SDL_PollEvent(&event))
					processEvent(event);

				AKUModulesUpdate();
				AKURender();
				SDL_GL_SwapWindow(mWindow);
			}
		}
		catch(exception e)
		{
			cout << "Exception: " << e.what() << endl;
			return ExitReason::Error;
		}
		catch(...)
		{
			cout << "Unknown exception" << endl;
			return ExitReason::Error;
		}

		return mExitReason;
	}

private:
	void openWindow(const char* title, int width, int height)
	{
		if(mWindow) return;

		mWindow = SDL_CreateWindow(title, mWindowX, mWindowY, width, height, SDL_WINDOW_OPENGL);
		if(SDL_GL_SetSwapInterval(-1) < 0)
			SDL_GL_SetSwapInterval(1);
		mGLContext = SDL_GL_CreateContext(mWindow);

		AKUDetectGfxContext();
		AKUSetScreenSize(width, height);
	}

	void closeWindow()
	{
		if(!mWindow) return;

		AKUReleaseGfxContext();

		SDL_GL_DeleteContext(mGLContext);
		SDL_GetWindowPosition(mWindow, &mWindowX, &mWindowY);
		SDL_DestroyWindow(mWindow);
		mWindow = NULL;
		mGLContext = NULL;
		mRunning = false;
	}

	void processEvent(SDL_Event event)
	{
		switch(event.type)
		{
		case SDL_MOUSEMOTION:
			AKUEnqueuePointerEvent(InputDevice::Main, MainSensor::Mouse, (int)event.motion.x, (int)event.motion.y);
			if(event.motion.state & SDL_BUTTON_LMASK > 0)
			{
				AKUEnqueueTouchEvent(InputDevice::Main, MainSensor::Touch, 0, true, event.motion.x, event.motion.y);
			}
			break;
		case SDL_MOUSEBUTTONUP:
			handleMouseButton(event.button, false);
			break;
		case SDL_MOUSEBUTTONDOWN:
			handleMouseButton(event.button, true);
			break;
		case SDL_MOUSEWHEEL:
			AKUEnqueueWheelEvent(InputDevice::Main, MainSensor::MouseWheel, (float)event.wheel.y);
			break;
		case SDL_KEYDOWN:
			handleKey(event.key, true);
			break;
		case SDL_KEYUP:
			handleKey(event.key, false);
			break;
		case SDL_QUIT:
			handleQuit();
			break;
		}
	}

	void handleMouseButton(SDL_MouseButtonEvent event, bool down)
	{
		switch(event.button)
		{
		case SDL_BUTTON_LEFT:
			AKUEnqueueButtonEvent(InputDevice::Main, MainSensor::MouseLeft, down);
			AKUEnqueueTouchEvent(InputDevice::Main, MainSensor::Touch, 0, down, event.x, event.y);
			break;
		case SDL_BUTTON_MIDDLE:
			AKUEnqueueButtonEvent(InputDevice::Main, MainSensor::MouseMiddle, down);
			break;
		case SDL_BUTTON_RIGHT:
			AKUEnqueueButtonEvent(InputDevice::Main, MainSensor::MouseRight, down);
			break;
		}
	}

	void handleKey(SDL_KeyboardEvent event, bool down)
	{
		if(event.repeat) return;

		if(down && (event.keysym.sym == SDLK_r) && ((event.keysym.mod & KMOD_CTRL) > 0))
		{
			closeWindow();
			mExitReason = ExitReason::Restart;
		}
		else
		{
			//Clamp keycode
			int key = event.keysym.sym;
			if (key & 0x40000000) key = (key & 0x3FFFFFFF) + 256;

			AKUEnqueueKeyboardEvent(InputDevice::Main, MainSensor::RawKeyboard, key, down);
		}
	}

	void handleQuit()
	{
		closeWindow();
		mExitReason = ExitReason::Normal;
	}

	static void thunk_openWindow(const char* title, int width, int height)
	{
		static_cast<Sim*>(AKUGetUserdata())->openWindow(title, width, height);
	}

	static void showCursor()
	{
		SDL_ShowCursor(1);
	}

	static void hideCursor()
	{
		SDL_ShowCursor(0);
	}

	static int onLuaPanic(lua_State* L)
	{
		MOAILuaState state ( L );
		STLString errorMsg = lua_tostring(L, -1);
		throw LuaPanicException(errorMsg);

		return 0;
	}

	//Initialize AKU and plugins
	Init mAppInitialize;
	Init mModulesAppInitialize;

	int mArgc;
	char** mArgv;
	SDL_Window* mWindow;
	SDL_GLContext mGLContext;
	ExitReason::Enum mExitReason;
	bool mRunning;
	int mWindowX;
	int mWindowY;
};

Sim* createSim(int argc, char* argv[])
{
	return new Sim(argc, argv);
}

ExitReason::Enum runSim(Sim* sim)
{
	return sim->run();
}

void destroySim(Sim* sim)
{
	delete sim;
}
