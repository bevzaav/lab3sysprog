#!/bin/bash
cd "$(dirname "$0")"
mkdir -p out
for g in dangling_else reduce_reduce expr_ambiguous expr_with_precedence; do
  echo "================ $g.y"
  cat $g.y
  echo "---- bison says:"
  bison -v -o out/$g.tab.c $g.y 2>&1 | sed 's/^/    /'
  echo "---- states with conflicts (from out/$g.output):"
  grep -n -i "conflict" out/$g.output | head -8 | sed 's/^/    /'
  echo
done
echo "---- main grammar (src/parser.y) has %expect 0, so any conflict would be a build error:"
bison -v -o out/parser_main.tab.c ../src/parser.y 2>&1 | sed 's/^/    /'
echo "    (no output above = 0 conflicts)"
echo
echo "State with the dangling-else conflict (from out/dangling_else.output):"
n=$(grep -o "^State [0-9]*" out/dangling_else.output | head -1 | awk '{print $2}')
sed -n "/^state $n\$/,/^state $((n+1))\$/p" out/dangling_else.output | sed '$d' | sed 's/^/    /'
