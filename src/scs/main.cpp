#include <QtCore/QtCore>
#include <QtDBus/QtDBus>
#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>
#include <regex.h>

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
	SCSINotifyThread(const QString & path)
		: m_path(path)
	{
	}
	void run()
	{
		inotifytools_initialize();
		inotifytools_watch_recursively(m_path.toLocal8Bit().constData()
				, IN_ALL_EVENTS);
		
		emit monitorEstablished();
		
		struct inotify_event * e = inotifytools_next_event(-1);
		while (e)
		{
			QString fname = QString("%1%2").arg(inotifytools_filename_from_wd(e->wd)).arg(e->name);
			if ((e->mask & IN_CREATE) || (e->mask & IN_MOVED_TO))
			{
				inotifytools_watch_recursively(fname.toLocal8Bit().constData(), IN_ALL_EVENTS);
				emit fileAdded(fname);
			}
			else if ((e->mask & IN_DELETE) || (e->mask & IN_MOVED_FROM))
				emit fileRemoved(fname);
			else if (e->mask & IN_CLOSE_WRITE)
				emit fileModified(fname);
			e = inotifytools_next_event(-1);
		}
	}
signals:
	void fileAdded(QString filePath);
	void fileRemoved(QString filePath);
	void fileModified(QString filePath);
	void monitorEstablished();
private:
	QString m_path;
};

class SCSINotifyServ : public QObject
{
	Q_OBJECT
public:
	SCSINotifyServ(const QString & path, QObject * parent)
		: QObject(parent), m_thread(path)
	{
		QEventLoop emptyLoop;
		connect(&m_thread, SIGNAL(fileAdded(QString))
				, this, SIGNAL(fileAdded(QString)));
		connect(&m_thread, SIGNAL(fileRemoved(QString))
				, this, SIGNAL(fileRemoved(QString)));
		connect(&m_thread, SIGNAL(fileModified(QString))
				, this, SIGNAL(fileModified(QString)));
		connect(&m_thread, SIGNAL(monitorEstablished())
				, &emptyLoop, SLOT(quit()));

		m_thread.start();
		report(tr("Wait for establish monitor: %1.").arg(path));
		emptyLoop.exec(QEventLoop::ExcludeUserInputEvents
					 | QEventLoop::ExcludeSocketNotifiers
					 | QEventLoop::WaitForMoreEvents);
		report(tr("Monitor established."));
	}
signals:
	void fileAdded(QString filePath);
	void fileRemoved(QString filePath);
	void fileModified(QString filePath);
private:
	SCSINotifyThread m_thread;
};

class SCSDataCenter
{
private:
	//! each of item is a entry
	//! when QByteArray is Null, the entry is not cached
	typedef QMap<QString, QByteArray> CacheMap;
	typedef CacheMap::iterator CacheMapIterator;
public:
	SCSDataCenter()
	{
	}
	void scan(const QString & path)
	{
		QMutexLocker locker(&m_mutex);
		QFileInfo fi(path);
		if (fi.isDir())
			_scan(fi.absoluteFilePath());
		else if (fi.isFile())
			_try_add_file(fi);
	}
	void remove(const QString & path)
	{
		QMutexLocker locker(&m_mutex);

		QString absPath = QFileInfo(path).absoluteFilePath();

		CacheMapIterator it = m_data.lowerBound(path);
		while (it != m_data.end())
		{
			if (it.key().left(absPath.length()) == absPath)
			{
				qDebug() << "del entry: " << it.key();
				it = m_data.erase(it);
			}
			else 
				break;
		}
	}
	void update(const QString & path)
	{
		QMutexLocker locker(&m_mutex);

		QFileInfo fi(path);
		if (_needCache(fi))
		{
			m_data[fi.absoluteFilePath()] = QByteArray();
			qDebug() << "upd entry: " << fi.absoluteFilePath();
		}
	}
	//! n: how many file to fetch
	//! return: how many file fetched
	int fetch(int n)
	{
		QMutexLocker locker(&m_mutex);
		
		int fetched = 0;
		for (CacheMapIterator it = m_data.begin(); it != m_data.end() && fetched != n; ++it)
		{
			if (it.value().isNull())
			{
				if (_fetch(it))
					++fetched;
			}
		}
		return fetched;
	}
	QByteArray getData(const QString & key)
	{
		CacheMapIterator it = m_data.find(key);
		if (it != m_data.end())
		{
			if (it.value().isNull())
			{
				if (!_fetch(it))
				{
					qDebug() << "could not fetch: " << it.key();
				}
			}
			return it.value();
		}
		else
		{
			return QByteArray();
		}
	}
	QStringList entrys()
	{
		return m_data.keys();
	}
private:
	bool _fetch(CacheMapIterator it)
	{
		QFileInfo fi(it.key());
		if (!fi.isFile()) // file removed during fetch, possibly. skip it
			return false;
		if (fi.size() != 0)
		{
			QFile f(it.key());
			if (!f.open(QIODevice::ReadOnly)) // file could not open, skip it
				return false;
			*it = f.readAll();
			if ((*it).isNull()) // read data failed, skip it
				return false;
		}
		else
		{
			*it = QByteArray("");
		}
		qDebug() << "fet entry: " << it.key();
		return true;
	}
	bool _needCache(const QFileInfo & fi)
	{
		QString ext = fi.suffix();
		if (ext == "c" || ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "inl")
			return true;
		if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "idl")
			return true;
		if (ext == "txt" || ext == "kui" || ext == "kuip")
			return true;
		return false;
	}
	void _try_add_file(const QFileInfo & fi)
	{
		if (_needCache(fi))
		{
			m_data[fi.absoluteFilePath()] = QByteArray();
			qDebug() << "add entry: " << fi.absoluteFilePath();
		}
	}
	void _scan(const QString & path)
	{
		QDir dir(path);
		if (!dir.exists())
			return;
		QFileInfoList fil = dir.entryInfoList();
		foreach (QFileInfo fi, fil)
		{
			if (fi.fileName() == "." || fi.fileName() == "..")
				continue;
			if (fi.isFile())
				_try_add_file(fi);
			if (fi.isDir())
				_scan(fi.absoluteFilePath());
		}
	}
private:
	CacheMap m_data;
	QMutex m_mutex;
};

static const QEvent::Type SCSSearchTaskReport = (QEvent::Type)QEvent::registerEventType();

class SCSSearchTaskReportEvent : public QEvent
{
public:
	SCSSearchTaskReportEvent(const QStringList & r)
		: QEvent(SCSSearchTaskReport), m_result(r)
	{
	}
	QStringList result()
	{
		return m_result;
	}
private:
	QStringList m_result;
};

class SCSSearchTask : public QRunnable
{
public:
	SCSSearchTask(const QStringList & entrys, const QString & regexp
			, SCSDataCenter * d, QObject * reportTo)
		: m_entrys(entrys), m_regexp(regexp), m_reportTo(reportTo), m_dataCenter(d)
	{
	}
	void run()
	{
		QStringList result;
		regex_t r;
		regcomp(&r, m_regexp.toLocal8Bit().constData(), 0);
		foreach (QString entry, m_entrys)
		{
			QByteArray data = m_dataCenter->getData(entry);
			if (data.isEmpty())
				continue;
//			QTextStream s(&data, QIODevice::ReadOnly);
//			QString line;
//			while (!(line = s.readLine()).isNull())
//			{
//				if (m_regexp.indexIn(line) != -1)
//				{
//					result << entry;
//					break;
//				}
//			}
			// 这一段代码性能很关键
			// 使用 gnu regex代替 QRegex，避免编码转换导致效率低下
			if (regexec(&r, data.constData(), 0, NULL, 0) == 0)
				result << entry;
		}
		regfree(&r);

		QEvent * e = new SCSSearchTaskReportEvent(result);
		QCoreApplication::postEvent(m_reportTo, e);
	}

private:
	QStringList m_entrys;
	QString m_regexp;
	QObject * m_reportTo;
	SCSDataCenter * m_dataCenter;
};

class SCSSearchRequestServ : public QObject
{
public:
	SCSSearchRequestServ(SCSDataCenter * d, QObject * parent)
		: QObject(parent), m_dataCenter(d), m_intf(0)
	{
	}
	bool run(const QString & reportTo, const QString & regex)
	{
		m_reportTo = reportTo;

		QStringList entrys = m_dataCenter->entrys();
		if (entrys.count() == 0)
		{
			// nothing need to do.
			deleteLater();
			return false;
		}

		m_taskcount = 0;
		m_taskcompleted = 0;
		int disted = 0;
		//QThreadPool::globalInstance()->setMaxThreadCount(1);
		while(disted < entrys.count())
		{
			SCSSearchTask * p = new SCSSearchTask(entrys.mid(disted, 100)
					, regex, m_dataCenter, this);
			QThreadPool::globalInstance()->start(p);
			disted += 100;
			++m_taskcount;
		}
		return true;
	}
	bool event(QEvent * e)
	{
		if (e->type() == SCSSearchTaskReport)
		{
			if (!m_intf && !initIntf())
			{
				g_serr << "Can not report to:" << m_reportTo;
				deleteLater();
				return true;
			}

			SCSSearchTaskReportEvent * se = (SCSSearchTaskReportEvent *)e;
			QStringList rl = se->result();
			foreach (QString r, rl)
			{
				m_intf->call("reportResult", r);
			}

			++m_taskcompleted;
			m_intf->call("reportProgress", m_taskcompleted, m_taskcount);

			if (m_taskcompleted == m_taskcount)
			{
				// all done
				deleteLater();
			}
		}
		return QObject::event(e);
	}
private:
	bool initIntf()
	{
		QDBusConnection c = QDBusConnection::sessionBus();
		m_intf = new QDBusInterface(m_reportTo, "/receiver" , "", c, this);
		if (!m_intf->isValid())
		{
			g_sout << QDBusConnection::sessionBus().lastError().message() << endl;
			return false;
		}
		return true;
	}

private:
	SCSDataCenter * m_dataCenter;
	QDBusInterface * m_intf;
	int m_taskcount;
	int m_taskcompleted;
	QString m_reportTo;
};

class SCSServer : public QObject
{
	Q_OBJECT
public:
	SCSServer(const QString & path)
	{
		m_inotifyServ = new SCSINotifyServ(path, this);
		connect(m_inotifyServ, SIGNAL(fileAdded(QString)), this, SLOT(onFileAdded(QString)));
		connect(m_inotifyServ, SIGNAL(fileRemoved(QString)), this, SLOT(onFileRemoved(QString)));
		connect(m_inotifyServ, SIGNAL(fileModified(QString)), this, SLOT(onFileModified(QString)));

		m_dataCenter = new SCSDataCenter;
		report(tr("First scan."));
		m_dataCenter->scan(path);
		report(tr("First scan complete."));

		connect(&m_pendingLoop, SIGNAL(timeout()), this, SLOT(fetchData()));
		connect(&m_zeroTimer, SIGNAL(timeout()), this, SLOT(fetchData()));
		m_pendingLoop.start(10000); // pending question per 10 sec
		m_zeroTimer.setSingleShot(true);
		m_zeroTimer.setInterval(0);
		m_zeroTimer.start(); // begin a fetch immediately.
	}
	~SCSServer()
	{
		//... todo 
		// stop thread and delete dataCenter;
	}
protected slots:
	void onFileAdded(QString filePath)
	{
		m_dataCenter->scan(filePath);
	}
	void onFileRemoved(QString filePath)
	{
		m_dataCenter->remove(filePath);
	}
	void onFileModified(QString filePath)
	{
		m_dataCenter->update(filePath);
	}
public slots:
	void quit()
	{
		qCoreApp->exit(0);
	}
	void fetchData()
	{
		if (m_dataCenter->fetch(10) == 10)
		{
			m_zeroTimer.start();
		}
	}
	QByteArray getContent(const QString & path)
	{
		QFileInfo fi(path);
		return m_dataCenter->getData(fi.absoluteFilePath());
	}
	QStringList entrys()
	{
		return m_dataCenter->entrys();
	}
	bool requestSearch(QString receiver, QString regexp)
	{
		report(tr("%1 search for %2").arg(receiver).arg(regexp));
		SCSSearchRequestServ * p = new SCSSearchRequestServ(m_dataCenter, this);
		return p->run(receiver, regexp);
	}
private:
	SCSINotifyServ * m_inotifyServ;
	SCSDataCenter * m_dataCenter;
	QTimer m_pendingLoop;
	QTimer m_zeroTimer;
};

int server_main(QStringList args)
{
	QString path;
	if (args.count() == 0)
	{
		path = QDir::currentPath();
	}
	else
	{
		path = args[0];
		QFileInfo fi(path);
		if (!fi.exists())
		{
			g_serr << "Path: " << path << " not exists.";
			return 1;
		}
		else if (!fi.isDir())
		{
			g_serr << "Path: " << path << " is not a direcotry.";
			return 1;
		}
	}
	QDBusConnection c = QDBusConnection::sessionBus();
	if (!c.registerService(SERVICE_NAME))
	{
		g_serr << c.lastError().message() << endl;
	}
	SCSServer s(path);
	c.registerObject("/server", &s, QDBusConnection::ExportAllSlots);
	return qCoreApp->exec();
}

class SCSReportReceiver : public QObject
{
	Q_OBJECT
public slots:
	void reportResult(QString fname)
	{
		g_sout << fname << endl;
	}
	void reportProgress(int n, int ncount)
	{
		g_sout << QString("[%1 / %2]   \r").arg(n).arg(ncount);
		g_sout.flush();
		if (n == ncount)
			qCoreApp->exit(0);
	}
};

int client_main(QStringList args)
{
	QDBusConnection c = QDBusConnection::sessionBus();
	QDBusInterface i(SERVICE_NAME, "/server", "", c);
	if (!i.isValid())
	{
		g_serr << c.lastError().message() << endl;
	}
	if (args.removeOne("-q"))
	{
		QDBusReply<QString> r = i.call("quit");
		return 0;
	}
	QString regexp = args[0];

	SCSReportReceiver receiver;
	c.registerObject("/receiver", &receiver, QDBusConnection::ExportAllSlots);
	i.call("requestSearch", c.baseService(), regexp);
	return qCoreApp->exec();
}

int main(int argc, char * argv[])
{
	QCoreApplication app(argc, argv);
	QStringList args = app.arguments();
	args.removeFirst();
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
