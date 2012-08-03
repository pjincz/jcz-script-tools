#include <QtCore/QtCore>
#include <QtDBus/QtDBus>

#define qCoreApp QCoreApplication::instance()

const char * SERVICE_NAME = "com.uthae.scs";

QTextStream g_sin(stdin);
QTextStream g_sout(stdout);
QTextStream g_serr(stderr);

void report(const QString & str)
{
	g_sout << str << endl;
}

class SCSINotifyThread : public QThread
{
	Q_OBJECT
public:
	SCSINotifyThread()
	{
	}
	void run()
	{
        sleep(5);
		emit onMonitorEstablished();
		sleep(5);
	}
signals:
	void onFileAdded(QString filePath);
	void onFileRemoved(QString filePath);
	void onMonitorEstablished();
};

class SCSINotifyServ : public QObject
{
	Q_OBJECT
public:
	SCSINotifyServ(QObject * parent)
		: QObject(parent)
	{
		QEventLoop emptyLoop;
		connect(&m_thread, SIGNAL(onFileAdded(QString))
				, this, SIGNAL(onFileAdded(QString)));
		connect(&m_thread, SIGNAL(onFileRemoved(QString))
				, this, SIGNAL(onFileRemoved(QString)));
		connect(&m_thread, SIGNAL(onMonitorEstablished())
				, &emptyLoop, SLOT(quit()));

		m_thread.start();
		report("Wait for establish monitor.");
		emptyLoop.exec(QEventLoop::ExcludeUserInputEvents
					 | QEventLoop::ExcludeSocketNotifiers
					 | QEventLoop::WaitForMoreEvents);
		report("Monitor established.");
	}
signals:
	void onFileAdded(QString filePath);
	void onFileRemoved(QString filePath);
private:
	SCSINotifyThread m_thread;
};

class SCSServer : public QObject
{
	Q_OBJECT
public:
	SCSServer()
	{
		m_inotifyServ = new SCSINotifyServ(this);
	}
public slots:
	void quit()
	{
		qCoreApp->exit(0);
	}
private:
	SCSINotifyServ * m_inotifyServ;
};

int server_main(QStringList args)
{
	QDBusConnection c = QDBusConnection::sessionBus();
	if (!c.registerService(SERVICE_NAME))
	{
		g_serr << c.lastError().message() << endl;
	}
	SCSServer s;
	c.registerObject("/server", &s, QDBusConnection::ExportAllSlots);
	return qCoreApp->exec();
}

int client_main(QStringList args)
{
	QDBusConnection c = QDBusConnection::sessionBus();
	QDBusInterface i(SERVICE_NAME, "/server", "", c);
	if (!i.isValid())
	{
		g_serr << c.lastError().message() << endl;
	}
	QDBusReply<QString> r = i.call("quit");
	return 0;
}

int main(int argc, char * argv[])
{
	QCoreApplication app(argc, argv);
	QStringList args = app.arguments();
	if (args.removeOne("-s"))
	{
		return server_main(args);
	}
	else
	{
		return client_main(args);
	}
	return 0;
}

#include "main.moc"
