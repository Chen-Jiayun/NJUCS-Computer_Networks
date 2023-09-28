# #!/bin/bash

# WEB_HASH=`./apps/webget cs144.keithw.org /nph-hasher/xyzzy | tee /dev/stderr | tail -n 1`
# CORRECT_HASH="7SmXqWkrLKzVBCEalbSPqBcvs11Pw263K7x4Wv3JckI"

# if [ "${WEB_HASH}" != "${CORRECT_HASH}" ]; then
#     echo ERROR: webget returned output that did not match the test\'s expectations
#     exit 1
# fi
# exit 0

#!/bin/bash

WEB_HASH=`./apps/webget www.njucn2022.top /check_ans/ans | tee /dev/stderr | tail -n 1`
CORRECT_HASH="adfafsdf3lfo5d6s6f7oj7f8ad8s876546SDSFSGDGGFDGRY"

if [ "${WEB_HASH}" != "${CORRECT_HASH}" ]; then
    echo ERROR: webget returned output that did not match the test\'s expectations
    exit 1
fi
exit 0
