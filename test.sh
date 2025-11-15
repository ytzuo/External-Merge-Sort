#!/bin/bash

EXECUTABLE="./data_structure_specialized_pratice"
RUNS=5
TIMEOUT_SEC=3

total_generate=0
total_merge=0
valid_runs=0

echo "开始连续执行 $RUNS 次（每次最多 ${TIMEOUT_SEC} 秒）..."

for i in $(seq 1 $RUNS); do
    echo -n "第 $i 次运行... "

    # 使用 timeout 执行程序，捕获输出和退出状态
    output=$(timeout $TIMEOUT_SEC "$EXECUTABLE" 2>&1)
    exit_code=$?

    if [[ $exit_code -eq 124 ]]; then
        echo "超时（>${TIMEOUT_SEC}s），跳过本轮。"
        continue
    elif [[ $exit_code -ne 0 ]]; then
        echo "程序异常退出（状态码: $exit_code），跳过本轮。"
        continue
    fi

    # 提取 Generate 和 Merge 时间
    gen_time=$(echo "$output" | grep -o 'Generate *: [0-9.]* s' | awk '{print $3}')
    merge_time=$(echo "$output" | grep -o 'Merge *: [0-9.]* s' | awk '{print $3}')

    if [[ -z "$gen_time" ]] || ! [[ "$gen_time" =~ ^[0-9]*\.?[0-9]+$ ]]; then
        echo "无法解析 Generate 时间，跳过本轮。"
        continue
    fi

    if [[ -z "$merge_time" ]] || ! [[ "$merge_time" =~ ^[0-9]*\.?[0-9]+$ ]]; then
        echo "无法解析 Merge 时间，跳过本轮。"
        continue
    fi

    total_generate=$(echo "$total_generate + $gen_time" | bc -l)
    total_merge=$(echo "$total_merge + $merge_time" | bc -l)
    ((valid_runs++))

    echo "成功（Generate: ${gen_time}s, Merge: ${merge_time}s）"
done

if [[ $valid_runs -eq 0 ]]; then
    echo ""
    echo "❌ 所有运行均失败或超时，无法计算平均值。"
    exit 1
fi

# 计算平均值（保留6位小数）
avg_generate=$(echo "scale=6; $total_generate / $valid_runs" | bc -l)
avg_merge=$(echo "scale=6; $total_merge / $valid_runs" | bc -l)

echo ""
echo "=== 平均耗时结果（有效运行 $valid_runs / $RUNS 次）==="
echo "Generate 平均耗时: $avg_generate s"
echo "Merge    平均耗时: $avg_merge s"