﻿#include "stdafx.h"
#include "error.h"
#include "utils.h"

const uint32_t win_build = []() {
	OSVERSIONINFO ver;
	ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(disable:4996)
	if (!GetVersionEx(&ver)) {
#pragma warning(default:4996)
		throw lro_error::from_other(err_msg::err_get_version, {});
	}
	return ver.dwBuildNumber;
}();

const auto hcon = GetStdHandle(STD_ERROR_HANDLE);
bool progress_printed;

inline void write(crwstr output, const uint16_t color) {
	CONSOLE_SCREEN_BUFFER_INFO ci;
	const auto ok = hcon != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hcon, &ci);
	if (ok) {
		if (progress_printed && SetConsoleCursorPosition(hcon, { 0, ci.dwCursorPosition.Y })) {
			if(ci.dwSize.X)[[likely]]
				print(fast_io::wout,fast_io::fill_nc(ci.dwSize.X-1,L' '));
			SetConsoleCursorPosition(hcon, { 0, ci.dwCursorPosition.Y });
		}
		SetConsoleTextAttribute(hcon, color);
	}
	println(fast_io::werr,output);
	if (ok) SetConsoleTextAttribute(hcon, ci.wAttributes);
	progress_printed = false;
}

void log_warning(crwstr msg) {
	write(L"[WARNING] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
}

void log_error(crwstr msg) {
	write(L"[ERROR] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED);
}

void print_progress(const double progress) {
	static int lc;
	if (hcon == INVALID_HANDLE_VALUE) return;
	CONSOLE_SCREEN_BUFFER_INFO ci;
	if (!GetConsoleScreenBufferInfo(hcon, &ci)) return;
	const auto tot = ci.dwSize.X - 3;
	const auto cnt = static_cast<int>(round(tot * progress));
	if (progress_printed && (cnt == lc || !SetConsoleCursorPosition(hcon, { 0, ci.dwCursorPosition.Y }))) return;
	lc = cnt;
	std::wcerr << L'[';
	for (auto i = 0; i < tot; i++) {
		if (i < cnt) std::wcerr << L'=';
		else std::wcerr << L'-';
	}
	std::wcerr << L']';
	progress_printed = true;
}

wstr from_utf8(const char *s) {
	const auto res = probe_and_call<wchar_t, int>([&](wchar_t *buf, int len) {
		return MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, len);
	});
	if (!res.second) throw lro_error::from_win32_last(err_msg::err_convert_encoding, {});
	return res.first.get();
}

std::unique_ptr<char[]> to_utf8(wstr s) {
	auto res = probe_and_call<char, int>([&](char *buf, int len) {
		return WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, buf, len, nullptr, nullptr);
	});
	if (!res.second) throw lro_error::from_win32_last(err_msg::err_convert_encoding, {});
	return std::move(res.first);
}

wstr get_full_path(crwstr path) {
	const auto fp = probe_and_call<wchar_t, int>([&](wchar_t *buf, int len) {
		return GetFullPathName(path.c_str(), len, buf, nullptr);
	});
	if (!fp.second) {
		throw lro_error::from_win32_last(err_msg::err_transform_path, { path });
	}
	return fp.first.get();
}
