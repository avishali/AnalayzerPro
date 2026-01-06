if grep -q "warning:" /tmp/build.log; then
  echo "FAIL: warnings present"
  exit 1
else
  echo "OK: zero warnings"
fi