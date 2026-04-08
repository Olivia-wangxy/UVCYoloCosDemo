# 1. 背景
  新项目可能会加入新的摄像头模块，前期对摄像头和Android开发板进行调研，初期目标如下：
  1. RK3576性能，能否同时支持多路摄像头的视频流捕获和处理。
  2. 开发板的NPU性能是否能满足运行主流的图像识别的算法（比如YOLO）。
  
# 2. Demo概述
  为了调研上述功能，本地编写了一个集成USB摄像头数据处理、Yolo图像识别和图像上传腾讯云COS存储功能的Demo，证明了目前办公室已有设备可以满足调研初期目标的，实现了一条完整的摄像头相关功能链路示例。
  Demo的页面比较简单，通过按钮触发功能，功能实现的流程图如下：
  1. 通过弹窗选择USB摄像头预览，支持多路同时预览、抓图、录制视频。
  2. 图库选图，使用Yolov5s模型推理，将识别结果显示在屏幕上，同时上传腾讯云。
  3. 抓取的摄像头图片或视频可以上传到腾讯云COS桶，也可从腾讯云COS桶中下载图片到Android设备中。

# 3. UVCCamera逻辑
  由于系统原生Camera API不支持USB摄像头（UVC协议），于是下载了UVC开源库来实现USB摄像头预览等功能，开源库路径：https://github.com/saki4510t/UVCCamera
  UVCCamera所需模块如下图，其中libuvccamera提供了JNI接口，负责底层的USB摄像头绑定，UVC协议，拉流、解码、渲染等逻辑，usbCameraCommon封装了预览的TextureView，多路预览线程管理等业务能力，而usbCameraTest7则为上层UI，多路预览展示页面，通过点击按钮来抓图和录制视频。
<img width="1128" height="410" alt="image" src="https://github.com/user-attachments/assets/b16dbf91-881c-4718-94a9-1df20cad4267" />

## 3.1 预览
  USB摄像头画面预览的核心逻辑是走UVC协议控制的，主要在native层实现，上层主要负责触发和展示。摄像头画面预览流程图如下：
  1. 在上层触发预览逻辑，选择USB摄像头获取权限后将可操作句柄传给UVCCamera对象。
  2. 创建预览类对象UVCPreview，与数据来源方Camera和输出方Surface绑定，通过setPreviewSize设置视频流参数，该方法已属于UVC控制请求。
  3. 通过uvc_start_streaming开始拉流，通过uvc_preview_frame_callback回调视频数据。
  4. waitPreviewFrame收到数据进行解码、转换等数据操作，然后将渲染数据给到上层显示。

## 3.2 抓图
  抓图逻辑比较简单，视频数据最终是显示在UVCCameraTextureView上的，抓图直接在UVCCameraTextureView中通过getBitmap()获取一帧，写入文件即可。
<img width="1535" height="1255" alt="image" src="https://github.com/user-attachments/assets/a4020f9f-2141-464b-b2fb-1e43a66fa0d0" />

## 3.3 录制
  视频录制相较抓图会复杂很多，涉及视频流音视频数据处理、编码、合并音视频生成文件的逻辑。视频录制与预览是同时进行的，在接收的数据流解码后会回调到对应的逻辑中，这里除了添加预览的回调还有录制的回调。
<img width="2174" height="906" alt="image" src="https://github.com/user-attachments/assets/5a9841b7-d6ff-475a-80c5-c70b6b69882f" />

  所以数据链路图如下图分为两个分支，一支负责预览渲染，一支负责视频录制。

录制流程图如下：
  1. 音视频编码类继承编码器MediaEncoder类，创建音视频编码类对象时会将自身添加到MediaMuxerWrapper。
  2. muxer.startRecording()开始录制，需要先通过muxer.addTrack()添加轨道声明该对象是一个音频轨（格式 AAC）还是视频轨（格式 H264），否则会报错，然后设置回调监听后才能收到数据。
  3. 编码类对象会将数据处理后传入MediaMuxer封装器，由封装器根据时间戳合并音视频生成MP4文件存储。

# 4. Yolov5s识别逻辑
  Yolov5s是用来图像识别的小模型，在Android开发板Soc上运行需要转为指定的rknn模型才能在NPU上运行。rknn模型可在Rockchip官方资料中找到适合RK3576和RK3568的模型，Android推理Demo是基于该Demo修改运行的：https://github.com/shiyinghan/rknn-android-yolov5。
  Yolov5s推理的整体逻辑简单，处理模型输入数据 -> 模型推理 -> 处理模型输出数据，但是需要注意的细节较多。图像识别流程图如下：

注意点：
  1. 图片裁剪：神经网络的输入尺寸是固定的，训练时通常用 640×640 尺寸的图片，推理输入需要输入相同尺寸，为了保持形状不变可能还需填充黑边。
  2. 通道转换：Android的图片常见是RGBA（4通道）数据， 但是YOLOv5 是按 RGB训练的，需要转换为与模型通道顺序一致的通道。
  3. 数据类型转换：需要将图片像素数据0 ~ 255（uint8），归一化为FP模型支持的数据0.0 ~ 1.0（float），或INT8量化为INT8模型支持的数据 -128 ~ 127（int8），在NPU上模型推理一般推荐INT8。
  4. 数据排列方式转换：Android图片数据是按像素存（HWC），yolo是按通道存（CHW），同样需要转换为与模型一致的方式。

# 5. 总结
  该Demo实现了关于USB摄像头相关的基础操作，如有USB摄像头开发任务，该文档可作为基础功能参考。
  1. USB摄像头相关操作可使用UVC库中提供的接口，包含基础的预览、设置分辨率、抓图、录制等功能。
  2. 图像识别可调用YoloV5Detect中的JNI接口，需要注意输入图片类型要符合使用模型的要求，不同版本的开发板需要对应的模型，否则无法推理。有关Yolov5s自定义训练模型相关内容也可参考文档Android运行Yolov5s知识储备
  3. 该Demo还写了上传腾讯云存储或下载图片的使用示例可供参考。


