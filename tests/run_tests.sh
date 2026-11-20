#!/bin/bash
cd "$(dirname "$0")/.."
pass=0
fail=0
tmp=$(mktemp -d)

check() {
  if [ "$2" = "ok" ]; then pass=$((pass+1)); echo "  ok   $1"; else fail=$((fail+1)); echo "  FAIL $1: $3"; fi
}

echo "interpreter vs generated Python"
for f in examples/*.c; do
  name=$(basename "$f" .c)
  ./minic "$f" --run > "$tmp/$name.interp" 2>/dev/null
  ./minic "$f" --py "$tmp/$name.py" 2>/dev/null
  python3 "$tmp/$name.py" > "$tmp/$name.py.out" 2>/dev/null
  if diff -q "$tmp/$name.interp" "$tmp/$name.py.out" >/dev/null; then
    check "$name" ok
  else
    check "$name" fail "outputs differ"
  fi
done

echo "expected diagnostics"
expect_error() {
  file=$1; pattern=$2
  out=$(./minic "$file" 2>&1)
  rc=$?
  if [ $rc -ne 0 ] && echo "$out" | grep -q "$pattern"; then check "$file" ok; else check "$file" fail "rc=$rc, wanted '$pattern', got: $out"; fi
}
expect_error examples/errors/syntax.c "syntax error"
expect_error examples/errors/undeclared.c "undeclared variable 'b'"
expect_error examples/errors/undeclared.c "undefined function 'foo'"
expect_error examples/errors/args.c "redeclaration of 'a'"
expect_error examples/errors/args.c "expects 2 argument"

echo "ast json is valid"
for f in examples/*.c; do
  name=$(basename "$f" .c)
  ./minic "$f" --json "$tmp/$name.json" 2>/dev/null
  if python3 -c "import json,sys; json.load(open('$tmp/$name.json'))" 2>/dev/null; then check "$name.json" ok; else check "$name.json" fail "invalid json"; fi
done

rm -rf "$tmp"
echo "passed: $pass, failed: $fail"
[ $fail -eq 0 ]
