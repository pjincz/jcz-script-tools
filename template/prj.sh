mkdir "$1"

cd "$1"

cat > main.cpp << EOF
#include <stdio.h>

int main(int argc, char * argv[])
{
	printf("Hello World!\n");
	return 0;
}
EOF

sed "s/@prj@/$1/g" > Makefile << EOF
all: @prj@

@prj@: main.cpp
	g++ main.cpp -o @prj@

run: @prj@
	./@prj@
EOF
