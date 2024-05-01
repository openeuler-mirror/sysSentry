#!/bin/bash
# Copyright (c), 2023-2024, Huawei Tech. Co., Ltd.

source ./libs/expect.sh
export SYS_PATH=$(pwd)
tmp_log="result.log"
testcase_success=0
testcase_fail=0

success_cases=()
fail_cases=()

for file in $(find test/ -name test_\*\.sh)
do
	echo "=== run test $file start ==="
	sh $file > ${tmp_log} 2>&1 

	if [ $? -eq 0 ]; then
	    ((++testcase_success))
	     success_cases+=("$file")
	else
	    ((++testcase_fail))
	    fail_cases+=("$file")
	    echo "test $file failed!"
	    cat ${tmp_log}
	fi
	echo "=== run test $file end ==="
done

echo "----------Success cases:----------"
for file_name in "${success_cases[@]}"; do
    echo "$file_name"
done

echo "----------Fail cases:----------"
for file_name in "${fail_cases[@]}"; do
    echo "$file_name"
done

rm -rf ${tmp_log}

echo "============ TOTAL RESULT ============"
echo "[result] success:$testcase_success failed:$testcase_fail"
