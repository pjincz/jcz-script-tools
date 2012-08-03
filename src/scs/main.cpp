#include <QtCore/QtCore>
#include <QtDBus/QtDBus>

#define qCoreApp QCoreApplication::instance()

const char * SERVICE_NAME = "com.uthae.scs";

QTextStream g_sin(stdin);
QTextStream g_sout(stdout);
QTextStream g_serr(stderr);

class SCSServer : public QObject
{
	Q_OBJECT
public:
	SCSServer()
	{
	}
public slots:
	void quit()
	{
		qCoreApp->exit(0);
	}
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
