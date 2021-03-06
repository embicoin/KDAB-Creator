/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "procinterrupt.h"
#include "debuggerconstants.h"

#include <QCoreApplication>
#include <QProcess> // makes kill visible on Windows.
#include <QFile>
#include <QDir>
using namespace Debugger::Internal;

static inline QString msgCannotInterrupt(int pid, const QString &why)
{
    return QString::fromLatin1("Cannot interrupt process %1: %2").arg(pid).arg(why);
}

#if defined(Q_OS_WIN)

#define _WIN32_WINNT 0x0501 /* WinXP, needed for DebugBreakProcess() */

#include <utils/winutils.h>
#include <windows.h>

#if !defined(PROCESS_SUSPEND_RESUME) // Check flag for MinGW
#    define PROCESS_SUSPEND_RESUME (0x0800)
#endif // PROCESS_SUSPEND_RESUME

static BOOL isWow64Process(HANDLE hproc)
{
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

    BOOL ret = false;

    static LPFN_ISWOW64PROCESS fnIsWow64Process = NULL;
    if (!fnIsWow64Process) {
        if (HMODULE hModule = GetModuleHandle(L"kernel32.dll"))
            fnIsWow64Process = reinterpret_cast<LPFN_ISWOW64PROCESS>(GetProcAddress(hModule, "IsWow64Process"));
    }

    if (!fnIsWow64Process) {
        qWarning("Cannot retrieve symbol 'IsWow64Process'.");
        return false;
    }

    if (!fnIsWow64Process(hproc, &ret)) {
        qWarning("IsWow64Process() failed for %p: %s",
                 hproc, qPrintable(Utils::winErrorMessage(GetLastError())));
        return false;
    }
    return ret;
}

// Open the process and break into it
bool Debugger::Internal::interruptProcess(int pID, int engineType, QString *errorMessage)
{
    bool ok = false;
    HANDLE inferior = NULL;
    do {
        const DWORD rights = PROCESS_QUERY_INFORMATION|PROCESS_SET_INFORMATION
                |PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ
                |PROCESS_DUP_HANDLE|PROCESS_TERMINATE|PROCESS_CREATE_THREAD|PROCESS_SUSPEND_RESUME;
        inferior = OpenProcess(rights, FALSE, pID);
        if (inferior == NULL) {
            *errorMessage = QString::fromLatin1("Cannot open process %1: %2").
                    arg(pID).arg(Utils::winErrorMessage(GetLastError()));
            break;
        }
        // Try DebugBreakProcess if either Qt Creator is compiled 64 bit or
        // both Qt Creator and application are 32 bit.
#ifdef Q_OS_WIN64
        // Qt-Creator compiled 64 bit
        // Windows must be 64 bit
        // CDB 64 bit: use DebugBreakProcess for 32 an 64 bit processes.
        // CDB 32 bit: untested
        // GDB: not supported
        const bool useDebugBreakApi= true;

#else
        // Qt-Creator compiled 32 bit:

        bool useDebugBreakApi;
        if (isWow64Process(GetCurrentProcess())) {
            // Windows is 64 bit
            if (engineType == CdbEngineType) {
                // CDB 64 bit: If Qt-Creator is a WOW64 process (meaning a 32bit process
                //    running in emulation), always use win64interrupt.exe for native
                //    64 bit processes and WOW64 processes. While DebugBreakProcess()
                //    works in theory for other WOW64 processes, the break appears
                //    as a WOW64 breakpoint, which CDB is configured to ignore since
                //    it also triggers on module loading.
                // CDB 32 bit: untested
                useDebugBreakApi = false;
            } else {
                // GDB: Use win64interrupt for native 64bit processes only (it fails
                //    for WOW64 processes.
                useDebugBreakApi = isWow64Process(inferior);
            }
        } else {
            // Windows is 32 bit
            // All processes are 32 bit, so DebugBreakProcess can be used in all cases.
            useDebugBreakApi = true;
        }
#endif
        if (useDebugBreakApi) {
            ok = DebugBreakProcess(inferior);
            if (!ok)
                *errorMessage = QLatin1String("DebugBreakProcess failed: ") + Utils::winErrorMessage(GetLastError());
        } else {
            const QString executable = QCoreApplication::applicationDirPath() + QLatin1String("/win64interrupt.exe");
            switch (QProcess::execute(executable + QLatin1Char(' ') + QString::number(pID))) {
            case -2:
                *errorMessage = QString::fromLatin1("Cannot start %1. Check src\\tools\\win64interrupt\\win64interrupt.c for more information.").
                                arg(QDir::toNativeSeparators(executable));
                break;
            case 0:
                ok = true;
                break;
            default:
                *errorMessage = QDir::toNativeSeparators(executable)
                                + QLatin1String(" could not break the process.");
                break;
            }
            break;
        }
    } while (false);
    if (inferior != NULL)
        CloseHandle(inferior);
    if (!ok)
        *errorMessage = msgCannotInterrupt(pID, *errorMessage);
    return ok;
}

#else // Q_OS_WIN

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

bool Debugger::Internal::interruptProcess(int pID, int /* engineType */,
                                          QString *errorMessage)
{
    if (pID <= 0) {
        *errorMessage = msgCannotInterrupt(pID, QString::fromLatin1("Invalid process id."));
        return false;
    }
    if (kill(pID, SIGINT)) {
        *errorMessage = msgCannotInterrupt(pID, QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
    return true;
}

#endif // !Q_OS_WIN
