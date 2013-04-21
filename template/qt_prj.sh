mkdir "$1"
cd "$1"

sed "" > main.cpp << EOF
#include <QtCore/QtCore>

int main(int argc, char * argv[])
{
	QCoreApplication app(argc, argv);

	QString hello = "hello";
	QTextStream qout(stdout);
	qout << hello << endl;
	
	return 0;
}
EOF

qmake -project .
