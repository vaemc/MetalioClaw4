# SD 卡资源说明

本目录存放数字人等功能的 SD 卡资源。将内容复制到 SD 卡根目录即可使用（保持目录结构不变）。

数字人表情默认使用 **SJPG** 格式（LVGL 分块 JPEG），也可在数字人设置页切换为 **EAF** 动画。资源路径为：

```
/sdcard/system/emotion/{category}.sjpg   # 或 .eaf
```

其中 `{category}` 为：`crying`、`happy`、`loving`、`neutral`、`surprised`、`thinking` 之一。

---

## jpg_to_sjpg.py — JPG 转 SJPG

将普通 JPG 图片转换为 LVGL 可用的 `.sjpg` 二进制文件，同时生成可嵌入固件的 `.c` 数组文件。

### 环境要求

| 项 | 说明 |
|:---|:---|
| **Python** | 3.x |
| **依赖** | [Pillow](https://pypi.org/project/Pillow/) |

安装依赖：

```bash
pip install Pillow
```

### 用法

```bash
cd sd_images
python jpg_to_sjpg.py input.jpg
```

也可指定任意路径的 JPG 文件：

```bash
python jpg_to_sjpg.py /path/to/photo.jpg
```

### 输出文件

脚本会在**当前工作目录**生成两个文件（文件名取自输入 JPG 的主文件名）：

| 文件 | 说明 |
|:---|:---|
| `{name}.sjpg` | SJPG 二进制，复制到 SD 卡 `system/emotion/` 等目录供 LVGL 加载 |
| `{name}.c` | C 数组源码，含 `lv_image_dsc_t` 描述符，可编译进固件（按需使用） |

示例：转换 `happy.jpg` 后得到 `happy.sjpg` 与 `happy.c`。

### 示例：制作数字人表情

```bash
# 1. 准备 720×720 的 JPG（或与设备屏幕分辨率匹配）
# 2. 转换
python jpg_to_sjpg.py happy.jpg

# 3. 将生成的 happy.sjpg 复制到 SD 卡
#    /sdcard/system/emotion/happy.sjpg
```

### 转换原理（简要）

- 将 JPG 按高度 **16 像素** 为一行块切分
- 每块单独编码为 JPEG，再按 LVGL SJPG V1.00 格式打包
- 运行时 LVGL 可按块解码，降低大图显示时的内存占用

### 注意事项

- 输入文件必须是有效的 JPG/JPEG 图片
- 转换过程中会在当前目录产生临时 `{0..n}.jpg`，脚本结束会自动删除
- 若只需 SD 卡资源，使用 `.sjpg` 即可；`.c` 文件用于把图片编译进程序，一般数字人场景用 SD 卡方案
- `system/` 下为系统内置资源，请勿随意删除
