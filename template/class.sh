d=$(basename $(dirname "$PWD/$1"))
f=$(basename "$1")
macro=$(echo "__${d}_${f}_H_$(date "+%Y_%m_%d_%H_%M_%S")__" | tr ' \-a-z' '__A-Z')

sed "s/@macro/$macro/g" > "$1.h" << EOF
#ifndef @macro
#define @macro


#endif//@macro
EOF

echo -e "#include \"$f.h\"\n\n" > "$1.cpp"

