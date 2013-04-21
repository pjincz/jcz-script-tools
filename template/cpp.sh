f=$(basename "$1")

echo -e "#include \"$f.h\"\n\n" > "$1.cpp"

