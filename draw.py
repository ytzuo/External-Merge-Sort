import matplotlib.pyplot as plt

# 横坐标：数据总量
data_sizes = [500000, 600000, 700000, 800000, 900000, 1000000]

# 👇👇👇 请在这里替换为你自己的时间数据（单位：秒，每个数组包含6个数值）
times_1 = [0.1210, 0.1422, 0.1549, 0.1974, 0.2092, 0.2760]  # 示例数据，请按实际修改！
times_2 = [0.0855, 0.1085, 0.1348, 0.1615, 0.1912, 0.2191]  # 示例数据，请按实际修改！
times_3 = [0.0533, 0.0666, 0.0768, 0.0976, 0.1276,  0.1270]  # 示例数据，请按实际修改！

# 数据验证
datasets = [times_1, times_2, times_3]
for i, times in enumerate(datasets):
    if len(times) != len(data_sizes):
        raise ValueError(f"第{i+1}组时间数据长度必须为6，与数据总量一一对应！")

# 绘制图形
plt.figure(figsize=(10, 6))

colors = ['r', 'g', 'b']  # 红色、绿色、蓝色
labels = ['Project1', 'Project2', 'Project3']  # 替换为你的曲线名称

for i, times in enumerate(datasets):
    plt.plot(data_sizes, times, marker='o', linestyle='-', color=colors[i], linewidth=2, markersize=6, label=labels[i])

# 设置图表标题和坐标轴标签
plt.title('Performance Trend of Different Methods with Data Volume', fontsize=14)
plt.xlabel('Data Size (records)', fontsize=12)
plt.ylabel('Time Consumed (seconds)', fontsize=12)

# 添加网格
plt.grid(True, linestyle='--', alpha=0.6)

# 设置横坐标显示为整数（避免科学计数法）
plt.xticks(data_sizes)

# 显示数值标签（可选）
for times in datasets:
    for x, y in zip(data_sizes, times):
        plt.text(x, y + max(max(datasets))*0.01, f'{y:.3f}s', ha='center', va='bottom', fontsize=9)

# 添加图例
plt.legend()

# 展示图形
plt.tight_layout()
plt.show()