# Homo_Vision

此程序为 海韵五队 参加 第十七届全国大学生智能汽车竞赛完全模型组省赛 比赛时，EdgeBoard 计算卡程序，比赛时运行在 EdgeBoard 上。

技术方案参见博客文章：[完全模型组 EdgeBoard 端快速调车经验分享 | lookas](https://18kas.com/homo-vision)

## 环境需求

程序运行在 EdgeBoard Linux 环境上，要求在 `/opt/opencv` 目录下安装有 OpenCV 4.5.5 版本，方式如下文所述。

程序也可运行在 Windows 系统，需要安装有 Microsoft Visual Studio Tools (cl.exe)，Windows SDK (windows.h)，OpenCV，CMake，Git Bash

## 编译项目

```powershell
cd build
cmake ..
cmake --build .
```

## 摄像头图像大小，裁剪图像大小以及透视标定

根据摄像头的不同，需要设置摄像头原始大小以及帧率

```cpp
#define CAP_WIDTH 1280
#define CAP_HEIGHT 720
#define CAP_FPS 60
```

设置裁剪图像大小

```cpp
#define IMG_H 300 // TODO 根据自己小车的摄像头角度位置填充
#define IMG_W 400 
```

和相应的裁剪代码

```cpp
// 裁剪图像
cv::resize(frame, frame, cv::Size(400, 300), 0, 0, cv::INTER_LINEAR);
frame = frame(cv::Rect(0, 0, IMG_W, IMG_H));
```

最后将小车放在直道上，填充两个全局变量

```cpp
static double my_perspective_m[3][3];
static int my_track_width[IMG_H];
```

## 代码部署

`deploy` 目录下提供了非常多方便部署的脚本，项目下载后需要先补全 `env.template.sh` 中空缺的部分，然后将文件改名成 `env.sh` 。通过 `init.sh` 可以将车的程序重新上传全量编译，通过 `upload.sh` 可以将车的程序重新上传增量编译。

同时也可以使用 Visual Studio Code 的 Remote 功能，直接连接到小车上去调试代码，配置好证书登录后非常方便（推荐）。

## 录像与回放

此程序提供有录像回放功能，可实现跑车时高性能录像，并且在跑车结束后回放录像进行计算，方便图像程序快速调试。

程序有四种运行模式：

- `./main` 正常运行程序，不开启录像程序
- `./main 1` 运行程序，并且开启录像程序，将摄像头图片保存到文件中
- `./main 2` 运行程序，并且开启回放程序，将文件中的图片读入当摄像头数据进行处理，处理后的图片在屏幕上输出
- `./main 2 1000` 运行程序，并且开启回放程序，运算到 1000 帧后再开启屏幕显示
- `./main 3` 运行程序，不开启录像程序，并且开启屏幕显示，讲实时摄像头数据处理结束后在屏幕上显示

## Wiki

以下为 Wiki 部分，提供了我们踩坑过程中留下的经验

## 20220400 串口线连接

Flow control 没有设置为 None 会导致中断启动：

`plink -serial COM6 -sercfg 115200,8,n,1,N`

## 20220400 网络设置

```bash
nmcli c modify edgeboard_eth0 ipv4.addr '192.168.137.254/24'
nmcli c modify edgeboard_eth0 ipv4.gateway 192.168.137.1
nmcli c modify edgeboard_eth0 ipv4.dns 223.5.5.5
nmcli c up edgeboard_eth0
```

用网线直接连接电脑和 Edgeboard，需要注意的是，需要在 Wifi 网络接口下设置共享，共享 Wifi 的网络给网线（使用 192.168.137.x 网段），让 Edgeboard 能用上电脑的网络。

`ssh root@192.168.137.254`

## 20220400 ssh 连接设置

`ssh-keygen` 生成一组连接用的密钥

需要从 id.pub 设置一下 .ssh/authorized_keys

## 20220400 vnc 连接设置

安装了 tightvnc server

## 20220513 硬盘扩容

需要将一个 Edgeboard 板子的 sd 卡通过 usb 转接器连到另外一个 Edgeboard 上。（建议）

```bash
parted
# p
# rm 3
# resizepart 2 100%
resize2fs /dev/sda2
vi etc/fstab
# #/dev/mmcblk1p3 /root/workspace           auto        nofail    0   0
```

Ref: [EdgeBoard——SD卡分区扩容并增加虚拟内存_Irving.Gao的博客-CSDN博客_sd卡扩容](https://blog.csdn.net/qq_45779334/article/details/118530365)

## 20220514 摄像头相关

```shell
# 安装 v4l2 工具包
sudo apt install v4l2-utils

# 查看摄像头设备
v4l2-ctl --list-devices

# 查看当前摄像头支持的视频压缩格式、分辨率
v4l2-ctl -d /dev/video0 --list-formats-ext

# 查看摄像头所有参数
v4l2-ctl -d /dev/video0 --all

# source: https://blog.csdn.net/sinat_37322535/article/details/118527937

ffmpeg -f v4l2 -list_formats all -i /dev/video0

ffmpeg -f v4l2 -input_format yuyv422 -framerate 30 -video_size 640x480 -i /dev/video0 -c:v libx264 -vf format=yuv420p output.mp4

ffplay -f v4l2 -input_format yuyv422 -framerate 30 -video_size 640x480 /dev/video0

# source: https://superuser.com/questions/494575/ffmpeg-open-webcam-using-yuyv-but-i-want-mjpeg

# 用命令行枚举采集设备和采集数据
ffmpeg -f dshow -list_devices true -i dummy

# 获取指定视频采集设备支持的分辨率、帧率和像素格式等属性
ffmpeg -f dshow -list_options true -i video="USB 2861 Device"

# 用video=指定视频设备，用audio=指定音频设备，后面的参数是定义编码器的格式和属性，输出为一个名为mycamera.mkv的文件
# 若加上“-s 720x576”，则FFmpeg就会以720x576的分辨率进行采集，如果不设置，则以默认的分辨率输出。
ffmpeg -f dshow -i video="USB 2861 Device" -i audio="线路 (3- USB Audio Device)" -vcodec libx264 -acodec aac -strict -2 mycamera.mkv

ffplay -f dshow -vcodec mjpeg -framerate 180 -video_size 640x480 video="USB 2861 Device"

# source: https://blog.csdn.net/zhoubotong2012/article/details/79338093

# 这个命令在虚拟机下用180帧摄像头失败，但是在eb上成功
ffplay -f v4l2 -input_format yuyv422 -framerate 30 -video_size 320x240 -loglevel debug /dev/video0

# 采集摄像头片段并且录制（有点bug）
ffmpeg -f v4l2 -input_format mjpeg -framerate 60 -video_size 640x480 -i /dev/video0 -y 640x480_60fps_nolabel_new_cam.mkv -c copy -f nut - | ffplay -

# 不显示画面采集摄像头片段（稳定）
ffmpeg -f v4l2 -input_format mjpeg -video_size 640x480 -framerate 30 -i /dev/video0 -c copy 1.mkv
```

## 20220515 部署工具使用

deploy 文件夹下提供的工具用于帮助在本地写好代码后，部署到 Edgeboard 上。

使用的 shell 为 bash ，推荐在本地安装 Git for Windows 作为环境。

推荐本地安装 tightvnc 作为远程连接工具。

```
# 上传文件（会删除之前上传上去的文件（如果有）），并在 Edgeboard 上第一次（全量）编译
./deploy/init.sh
# 上传文件（只覆盖文件，不会删除文件），并在 Edgeboard 上增量编译
./deploy/upload.sh
# 在 Edgeboard 上启动 VNC 服务器，并打开 VNC 屏幕
./deploy/vnc.sh
# 在 Edgeboard 上运行编译好的程序
./deploy/run.sh
```

## 20220515 OpenCV 更新

板载 OpenCV 版本太过于陈旧，不支持摄像头选项，故编译了新版本的 OpenCV

```bash
cd /opt

wget -O opencv-4.5.5.zip https://github.com/opencv/opencv/archive/4.5.5.zip
# wget -O opencv_contrib-4.5.5.zip https://github.com/opencv/opencv_contrib/archive/4.5.5.zip
unzip opencv-4.5.5.zip
# unzip opencv_contrib-4.5.5.zip
cd opencv-4.5.5
mkdir build
cd build
cmake -D BUILD_PERF_TESTS=OFF -D BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/opt/opencv ../
# cmake -DOPENCV_EXTRA_MODULES_PATH=../opencv_contrib-4.5.5/modules ../
cmake --build . -- -j 4
cmake --build . --target install
```

Ref:

- [OpenCV 4.x installation on Ubuntu 18 – Meccanismo Complesso](https://www.meccanismocomplesso.org/en/opencv-4-x-installation-on-ubuntu-18/)
- [OpenCV: Installation in Linux](https://docs.opencv.org/4.x/d7/d9f/tutorial_linux_install.html)

## 20220516 Windows 下编译项目

推荐安装 Git Bash 和 Microsoft Visual Studio Tools

```powershell
cd build
cmake ..
cmake --build .
```

## 20220518 5G 无线网卡驱动安装

```bash
cd /lib/modules/4.14.0-xilinx-v2018.3
wget -O linux-xlnx-xilinx-v2018.3.zip https://github.com/Xilinx/linux-xlnx/archive/refs/tags/xilinx-v2018.3.zip
unzip linux-xlnx-xilinx-v2018.3.zip
mv linux-xlnx-xilinx-v2018.3 build
cd build

cp /proc/config.gz .
gunzip config.gz
mv config .config

# Enable the mt76x2u module
make menuconfig

# .. 到这里停止了，因为 mt7621 的最早提交是在 2018 年 10 月份...而这版代码是 3 月的...
```

## 20220706 摄像头参数调整

```
v4l2-ctl -d /dev/video0 --all

# 自己配置的参数
v4l2-ctl -d /dev/video0 -c sharpness=0 # 去除锐化带来的噪点

# 修改一次即可，参数会保存到摄像头中
```

## 20220708 拉跨的老摄像头

```
# 显示摄像头实际帧率
# 老摄像头只有17帧，或者可能是 eb 的时钟走的快（应该不是）
rm o.mkv
ffmpeg -f v4l2 -input_format mjpeg -framerate 30 -video_size 176x144 -i /dev/video0 -c copy o.mkv
```

## 许可

[MIT](http://opensource.org/licenses/MIT)

Copyright (c) 2022, lookas & xiaoxi
