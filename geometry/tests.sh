set -e

for test in *.test.js
do
  ava --timeout=600s "${test}"
done
