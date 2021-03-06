/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (C) 2011 - 2012 Research In Motion
**
** Contact: Research In Motion (blackberry-qt@qnx.com)
** Contact: KDAB (info@kdab.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#ifndef QNX_INTERNAL_QNXRUNNER_H
#define QNX_INTERNAL_QNXRUNNER_H

#include <projectexplorer/runconfiguration.h>

#include <utils/environment.h>
#include <utils/ssh/sshconnection.h>

#include <QObject>
#include <QProcess>

namespace Qt4ProjectManager {
class Qt4BuildConfiguration;
}

namespace Utils {
class SshRemoteProcessRunner;
}

namespace Qnx {
namespace Internal {

class QnxRunConfiguration;

class QnxRunner : public QObject
{
    Q_OBJECT
public:
    explicit QnxRunner(bool debugMode, QnxRunConfiguration *runConfiguration, QObject *parent = 0);

    bool isRunning() const;
    qint64 pid() const;

    ProjectExplorer::RunControl::StopResult stop();

public slots:
    void start();
    void tailApplicationLog();

signals:
    void output(const QString &msg, Utils::OutputFormat format);
    void started();
    void finished();

    void startFailed(const QString &msg);

private slots:
    void startFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void stopFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void readStandardOutput();
    void readStandardError();

    void handleTailOutput(const QByteArray &message);
    void handleTailError(const QByteArray &message);
    void handleTailConnectionError();

private:
    void killTailProcess();

    bool m_debugMode;

    qint64 m_pid;
    QString m_appId;

    bool m_running;
    bool m_stopping;

    Utils::Environment m_environment;
    QString m_deployCmd;
    QString m_deviceHost;
    QString m_password;
    QString m_barPackage;
    Utils::SshConnectionParameters m_sshParams;

    QProcess *m_launchProcess;
    QProcess *m_stopProcess;
    Utils::SshRemoteProcessRunner *m_tailProcess;
};

} // namespace Internal
} // namespace Qnx

#endif // QNX_INTERNAL_QNXRUNNER_H
