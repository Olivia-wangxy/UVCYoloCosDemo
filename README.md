本Demo是在该博客提供Demo的基础上进行修改使用的https://blog.csdn.net/hanshiying007/article/details/137332023

修改点：
1. 在官方github中找到支持RK3576开发板的RKNN模型，添加到assets中（yolov5s-640-640.rknn）
2. 为确保算法是在NPU上运行的，对输入图片做了INT8量化处理，修改位置为yolov5.cc/inference_yolov5_model

运行效果：
1. 原Demo中的yolov5s-int8.rknn可在RK3568中的NPU运行，用时65ms左右（rknn_run done, Elapse = 64.13 ms, FPS = 15.59）
2. 使用yolov5s-640-640.rknn在RK3576上对同一张图多次识别，用时约18ms~60ms，平均在40ms左右（normal Elapse Time = 41.07 ms, FPS = 24.35）


