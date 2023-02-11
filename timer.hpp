#pragma once

#include <Windows.h>
#include <iostream>
#include <winnt.h>
#include <fstream>

#include "Detours x64/detours.h"
#pragma comment(lib, "../Detours x64/detours.lib")
#pragma comment(lib, "../Detours x64/syelog.lib")
#pragma comment(lib,"Kernel32.lib")
#pragma comment(lib,"Winmm.lib")

namespace Helper
{
	void WriteTextToFile(std::string str)
	{
		
	}
}

namespace Speedhack
{
	extern"C" {
		static BOOL(WINAPI* originalQueryPerformanceCounter)(LARGE_INTEGER* performanceCounter) = QueryPerformanceCounter;
		static DWORD(WINAPI* originalGetTickCount)() = GetTickCount;
		static ULONGLONG(WINAPI* originalGetTickCount64)() = GetTickCount64;
		static DWORD(WINAPI* originalTimeGetTime)() = timeGetTime;
	}

	class TSimpleLock
	{
	public:
		TSimpleLock()
		{
			owner = GetCurrentThreadId();
		}
		unsigned long count;
		DWORD owner;
	};

	void lock(TSimpleLock& d)
	{
		auto tid = GetCurrentThreadId();
		if (d.owner != tid)
		{
			do {
				Sleep(0);
			} while (InterlockedExchange(&d.count, 1) == 0);
			d.owner = tid;
		}
		else {
			InterlockedIncrement(&d.count);
		}
	}

	void unlock(TSimpleLock& d)
	{
		if (d.count == 1)
			d.owner = 0;
		InterlockedDecrement(&d.count);
		if (d.count < 0) Helper::WriteTextToFile("error -1, during unlocking: " + d.owner);
	}

	TSimpleLock GTCLock;
	TSimpleLock QPCLock;

	template<class T>
	class SpeedHackClass
	{
	private:
		double speed = 0;
		T initialoffset;
		T initialtime;
	public:
		SpeedHackClass()
		{
			speed = 1.0;
		}
		SpeedHackClass(T _initialtime, T _initialoffset, double _speed = 1.0)
		{
			speed = _speed;
			initialoffset = _initialoffset;
			initialtime = _initialtime;
		}

		double get_speed() const { return speed; }

		T get(T currentTime)
		{
			T false_val = (T)((currentTime - initialtime) * speed) + initialoffset;
			return (T)false_val;
		}

		void set_speed(double _speed)
		{
			speed = _speed;
		}
	};


	SpeedHackClass<LONGLONG> h_QueryPerformanceCounter;
	SpeedHackClass<DWORD> h_GetTickCount;
	SpeedHackClass<ULONGLONG> h_GetTickCount64;
	SpeedHackClass<DWORD> h_GetTime;

	double lastspeed = 1.0; 
	
	BOOL WINAPI newQueryPerformanceCounter(LARGE_INTEGER* counter) {
		lock(QPCLock);
		LARGE_INTEGER currentLi;
		LARGE_INTEGER falseLi;
		originalQueryPerformanceCounter(&currentLi);
		falseLi.QuadPart = h_QueryPerformanceCounter.get(currentLi.QuadPart);
		unlock(QPCLock);

		*counter = falseLi;
		return true;
	}

	DWORD WINAPI newGetTickCount() {
		lock(GTCLock);
		auto res = h_GetTickCount.get(originalGetTickCount());
		unlock(GTCLock);

		return res;																					// Return false tick count
	}

	ULONGLONG WINAPI newGetTickCount64() {
		lock(GTCLock);
		auto res = h_GetTickCount64.get(originalGetTickCount64());
		unlock(GTCLock);

		return res;
	}
	
	DWORD WINAPI newTimeGetTime() {
		return h_GetTime.get(originalTimeGetTime());
	}

	LARGE_INTEGER initialtime64;
	LARGE_INTEGER initialoffset64;

	void InitializeSpeedHack(double speed) {
		lock(QPCLock);
		lock(GTCLock);

		originalQueryPerformanceCounter(&initialtime64);
		newQueryPerformanceCounter(&initialoffset64);

		h_QueryPerformanceCounter = SpeedHackClass<LONGLONG>(initialtime64.QuadPart, initialoffset64.QuadPart, speed);
		h_GetTickCount = SpeedHackClass<DWORD>(originalGetTickCount(), newGetTickCount(), speed);
		h_GetTickCount64 = SpeedHackClass<ULONGLONG>(originalGetTickCount64(), newGetTickCount64(), speed);
		h_GetTime = SpeedHackClass<DWORD>(originalTimeGetTime(), newTimeGetTime(), speed);

		lastspeed = speed;

		unlock(GTCLock);
		unlock(QPCLock);
	}

	void InintDLL(LPVOID hModule)
	{
		GTCLock = TSimpleLock();
		QPCLock = TSimpleLock();

		originalQueryPerformanceCounter(&initialtime64);
		initialoffset64 = initialtime64;

		h_QueryPerformanceCounter = SpeedHackClass<LONGLONG>(initialtime64.QuadPart, initialoffset64.QuadPart);
		h_GetTickCount = SpeedHackClass<DWORD>(originalGetTickCount(), originalGetTickCount());
		h_GetTickCount64 = SpeedHackClass<ULONGLONG>(originalGetTickCount64(), originalGetTickCount64());
		h_GetTime = SpeedHackClass<DWORD>(originalTimeGetTime(), originalTimeGetTime());

		DisableThreadLibraryCalls((HMODULE)hModule);
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)originalQueryPerformanceCounter, newQueryPerformanceCounter);
		DetourAttach(&(PVOID&)originalGetTickCount, newGetTickCount);
		DetourAttach(&(PVOID&)originalGetTickCount64, newGetTickCount64);
		DetourAttach(&(PVOID&)originalTimeGetTime, newTimeGetTime);
		DetourTransactionCommit();
	}
}
