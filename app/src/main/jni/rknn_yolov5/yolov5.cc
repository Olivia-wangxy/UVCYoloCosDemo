// Copyright (c) 2023 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "yolov5.h"
#include "utils/common.h"
#include "utils/file_utils.h"
#include "utils/image_utils.h"

//#define PERF_DETAIL

static void dump_tensor_attr(rknn_tensor_attr *attr) {
    LOGI("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
         "zp=%d, scale=%f\n",
         attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2],
         attr->dims[3],
         attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
         get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

int init_yolov5_model(const char *model_path, rknn_app_context_t *app_ctx) {
    int ret;
    int model_len = 0;
    char *model;
    rknn_context ctx = 0;

    // Load RKNN Model
    model_len = read_data_from_file(model_path, &model);
    if (model == NULL) {
        LOGE("load_model fail!\n");
        return -1;
    }

#ifdef PERF_DETAIL
    //如果想打印逐层耗时，将第四个参数设为：RKNN_FLAG_COLLECT_PERF_MASK
    int flag = RKNN_FLAG_COLLECT_PERF_MASK;
#else
    int flag = 0;
#endif

    // 1.初始化模型
    ret = rknn_init(&ctx, model, model_len, flag, NULL);
    free(model);
    if (ret < 0) {
        LOGE("rknn_init fail! ret=%d\n", ret);
        return -1;
    }

// 假设 ctx 是你的 rknn_context
    rknn_tensor_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.index = 0; // 第 0 个输入
    int ret1 = rknn_query(ctx,RKNN_QUERY_INPUT_ATTR, &attr, sizeof(attr));
    if (ret1 < 0) {
        LOGE("rknn_query input attr failed");
    } else {
        switch (attr.type) {
            case RKNN_TENSOR_UINT8:
                LOGI("Model input type: UINT8");
                break;
            case RKNN_TENSOR_INT8:
                LOGI("Model input type: INT8");
                break;
            case RKNN_TENSOR_FLOAT32:
                LOGI("Model input type: FLOAT32");
                break;
            default:
                LOGI("Model input type unknown: %d", attr.type);
                break;
        }
    }

    // 2.查询模型的输入输出属性

    // Get Model Input Output Number
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        LOGE("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    LOGI("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    // Get Model Input Info
    LOGI("input tensors:\n");
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));


    // 查询输入
//    rknn_tensor_attr input_attrs[app_ctx->io_num.n_input];
    LOGI("==== rknn_init ==== %d", app_ctx->io_num.n_input);
    for (int i = 0; i < app_ctx->io_num.n_input; i++) {
        memset(&input_attrs[i], 0, sizeof(rknn_tensor_attr));
        input_attrs[i].index = i;
        rknn_query(app_ctx->rknn_ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
        LOGI("rknn_init : Input[%d]: n_dims=%d size=%d fmt=%d type=%d, n_elems=%d", i,
             input_attrs[i].n_dims, input_attrs[i].size, input_attrs[i].fmt, input_attrs[i].type, input_attrs[i].n_elems);
        for (int d = 0; d < input_attrs[i].n_dims; d++)
            LOGI(" dim[%d]=%d", d, input_attrs[i].dims[d]);
    }

    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        LOGI(" io_num.n_input = %d, ret = %d", io_num.n_input, ret);
        if (ret != RKNN_SUCC) {
            LOGE("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    // Get Model Output Info
    LOGI("output tensors:\n");

    LOGI("==== output rknn_tensor_attr ==== %d", app_ctx->io_num.n_output);
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));

    // 查询输出
//    rknn_tensor_attr output_attrs[app_ctx->io_num.n_output];
    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
        memset(&output_attrs[i], 0, sizeof(rknn_tensor_attr));
        output_attrs[i].index = i;
        rknn_query(app_ctx->rknn_ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
        LOGI("Output[%d]: n_dims=%d size=%d fmt=%d type=%d", i, output_attrs[i].n_dims, output_attrs[i].size, output_attrs[i].fmt, output_attrs[i].type);
    }

    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            LOGE("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(output_attrs[i]));
    }

    // Set to context
    app_ctx->rknn_ctx = ctx;

    // TODO
    if (output_attrs[0].qnt_type == RKNN_TENSOR_QNT_AFFINE_ASYMMETRIC &&
        output_attrs[0].type != RKNN_TENSOR_FLOAT16) {
        app_ctx->is_quant = true;
    } else {
        app_ctx->is_quant = false;
    }

    app_ctx->io_num = io_num;
    app_ctx->input_attrs = (rknn_tensor_attr *) malloc(io_num.n_input * sizeof(rknn_tensor_attr));
    memcpy(app_ctx->input_attrs, input_attrs, io_num.n_input * sizeof(rknn_tensor_attr));
    app_ctx->output_attrs = (rknn_tensor_attr *) malloc(io_num.n_output * sizeof(rknn_tensor_attr));
    memcpy(app_ctx->output_attrs, output_attrs, io_num.n_output * sizeof(rknn_tensor_attr));

    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        LOGI("model is NCHW input fmt\n");
        app_ctx->model_channel = input_attrs[0].dims[1];
        app_ctx->model_height = input_attrs[0].dims[2];
        app_ctx->model_width = input_attrs[0].dims[3];
    } else {
        LOGI("model is NHWC input fmt\n");
        app_ctx->model_height = input_attrs[0].dims[1];
        app_ctx->model_width = input_attrs[0].dims[2];
        app_ctx->model_channel = input_attrs[0].dims[3];
    }
    LOGI("model input height=%d, width=%d, channel=%d\n",
         app_ctx->model_height, app_ctx->model_width, app_ctx->model_channel);

    return 0;
}

int release_yolov5_model(rknn_app_context_t *app_ctx) {
    if (app_ctx->rknn_ctx != 0) {
        // 9.销毁 RKNN
        rknn_destroy(app_ctx->rknn_ctx);
        app_ctx->rknn_ctx = 0;
    }
    if (app_ctx->input_attrs != NULL) {
        free(app_ctx->input_attrs);
        app_ctx->input_attrs = NULL;
    }
    if (app_ctx->output_attrs != NULL) {
        free(app_ctx->output_attrs);
        app_ctx->output_attrs = NULL;
    }
    return 0;
}





int inference_yolov5_model(rknn_app_context_t *app_ctx, image_buffer_t *img,
                           object_detect_result_list *od_results) {
    if ((!app_ctx) || !(img) || (!od_results)) return -1;

    LOGI("==== inference_yolov5_model ====");

    int ret = 0;
    int64_t start_us, elapse_us;
    image_buffer_t dst_img;
    letterbox_t letter_box;
    rknn_input inputs[app_ctx->io_num.n_input];
    rknn_output outputs[app_ctx->io_num.n_output];
    void *output_data[app_ctx->io_num.n_output];
    const float nms_threshold = NMS_THRESH;
    const float box_conf_threshold = BOX_THRESH;
    int bg_color = 114;

    memset(od_results, 0, sizeof(*od_results));
    memset(&letter_box, 0, sizeof(letterbox_t));
    memset(&dst_img, 0, sizeof(image_buffer_t));
    memset(inputs, 0, sizeof(inputs));
    memset(outputs, 0, sizeof(outputs));

    // ----------------- Step 1: 分配 dst_img -----------------
    dst_img.width = app_ctx->model_width;
    dst_img.height = app_ctx->model_height;
    dst_img.format = IMAGE_FORMAT_RGB888;
    dst_img.size = get_image_size(&dst_img);
    dst_img.virt_addr = (unsigned char *)malloc(dst_img.size);
    if (!dst_img.virt_addr) {
        LOGE("malloc dst_img fail, size=%d", dst_img.size);
        return -1;
    }


    // Step 2: letterbox
    LOGI("letterbox before src img: width=%d, height=%d, format=%d, size=%d, virt_addr=%p",
         img->width, img->height, img->format, img->size, img->virt_addr);
    LOGI("letterbox before dst img: width=%d, height=%d, format=%d, size=%d, virt_addr=%p",
         dst_img.width, dst_img.height, dst_img.format, dst_img.size, dst_img.virt_addr);
    LOGI("letterbox before bg_color=%d", bg_color);

    ret = convert_image_with_letterbox(img, &dst_img, &letter_box, bg_color);
    if (ret < 0) {
        LOGE("convert_image_with_letterbox fail! ret=%d", ret);
        free(dst_img.virt_addr);
        return -1;
    }

    LOGI("Letterbox after: scale=%f, x_pad=%d, y_pad=%d",letter_box.scale, letter_box.x_pad, letter_box.y_pad);

    // Step 3: INT8量化
    signed char* int8_buf = (signed char*)malloc(dst_img.size);
    if (!int8_buf) {
        LOGE("malloc int8_buf fail!");
        free(dst_img.virt_addr);
        return -1;
    }

    float scale = 0.003922f;
    int zp = -128;
    for (int i = 0; i < dst_img.size; i++) {
        float val = dst_img.virt_addr[i] * scale + zp;
        if (val > 127) val = 127;
        if (val < -128) val = -128;
//        int8_buf[i] = (int8_t)val;
        int8_buf[i] = (int8_t)((dst_img.virt_addr[i] / scale) + zp);
    }


    // ----------------- Step 4: 设置 RKNN 输入 -----------------
    inputs[0].index = 0;
    inputs[0].buf = int8_buf;
    inputs[0].type = RKNN_TENSOR_INT8;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].size = dst_img.size;

//    LOGI("rknn_inputs_set： inputs[0]: size=%d, first 10 bytes=", inputs[0].size);
//    for (int i = 0; i < 10; i++) {
//        LOGI("%d ", ((unsigned char*)inputs[0].buf)[i]);
//    }

    ret = rknn_inputs_set(app_ctx->rknn_ctx, app_ctx->io_num.n_input, inputs);
    if (ret < 0) {
        LOGE("rknn_inputs_set fail! ret=%d", ret);
        goto out_int8;
    }

    // ----------------- Step 5: 推理 -----------------
    start_us = getCurrentTimeUs();
    ret = rknn_run(app_ctx->rknn_ctx, NULL);
    elapse_us = getCurrentTimeUs() - start_us;
    LOGI("rknn_run done, Elapse = %.2f ms, FPS = %.2f",
         elapse_us / 1000.f, 1000.f*1000.f / elapse_us);
    if (ret < 0) {
        LOGE("rknn_run fail! ret=%d", ret);
        goto out_int8;
    }

    // ----------------- Step 6: 获取输出 -----------------
    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
        outputs[i].index = i;
        outputs[i].want_float = (!app_ctx->is_quant);
    }

    ret = rknn_outputs_get(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs, NULL);
    if (ret < 0) {
        LOGE("rknn_outputs_get fail! ret=%d", ret);
        goto out_int8;
    }

    LOGI("============= Output Info =============");

    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
        rknn_tensor_attr* attr = &app_ctx->output_attrs[i];

        LOGI("Output %d:", i);
        LOGI("  n_dims = %d", attr->n_dims);
        for (int d = 0; d < attr->n_dims; d++) {
            LOGI("  dim[%d] = %d", d, attr->dims[d]);
        }

        int last_dim = attr->dims[attr->n_dims - 1];
        int num_classes = (last_dim > 5) ? (last_dim - 5) : 0;
        LOGI("  => last_dim = %d, inferred num_classes = %d",
             last_dim, num_classes);
    }

    LOGI("=======================================");


    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
        output_data[i] = outputs[i].buf;


//        if (output_data[i] == NULL) {
//            LOGE("rknn_outputs_get： output_data[%d].buf is NULL!", i);
//        } else {
//            float* ptr = (float*)output_data[i];
//            int print_num = 10;
//            if (i < print_num) {
//                LOGI("rknn_outputs_get： output[%d]=%f", i, ptr[i]);
//            }
//        }
    }

    // ----------------- Step 7: 后处理 -----------------
    post_process(app_ctx, output_data, &letter_box, box_conf_threshold, nms_threshold, od_results);

    // ----------------- Step 8: 释放输出 -----------------
    rknn_outputs_release(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs);

    out_int8:
    if (int8_buf) free(int8_buf);
    out:
    if (dst_img.virt_addr) free(dst_img.virt_addr);

    return ret;
}





//int inference_yolov5_model(rknn_app_context_t *app_ctx, image_buffer_t *img,
//                           object_detect_result_list *od_results) {
//    LOGI("==== inference_yolov5_model ====");
//    int ret;
//    int64_t start_us, elapse_us;
//    image_buffer_t dst_img;
//    letterbox_t letter_box;
//    rknn_input inputs[app_ctx->io_num.n_input];
//    rknn_output outputs[app_ctx->io_num.n_output];
//    void *output_data[app_ctx->io_num.n_output];
//    const float nms_threshold = NMS_THRESH;      // 默认的NMS阈值
//    const float box_conf_threshold = BOX_THRESH; // 默认的置信度阈值
//    int bg_color = 114;
//
//    unsigned char* src = dst_img.virt_addr;
//
//
//    if ((!app_ctx) || !(img) || (!od_results)) {
//        return -1;
//    }
//
//    memset(od_results, 0x00, sizeof(*od_results));
//    memset(&letter_box, 0, sizeof(letterbox_t));
//    memset(&dst_img, 0, sizeof(image_buffer_t));
//    memset(inputs, 0, sizeof(inputs));
//    memset(outputs, 0, sizeof(outputs));
//
//    // Pre Process
//    dst_img.width = app_ctx->model_width;
//    dst_img.height = app_ctx->model_height;
//    dst_img.format = IMAGE_FORMAT_RGB888;
//    dst_img.size = get_image_size(&dst_img);
//    dst_img.virt_addr = (unsigned char *) malloc(dst_img.size);
//    if (dst_img.virt_addr == NULL) {
//        LOGE("malloc buffer size:%d fail!\n", dst_img.size);
//        return -1;
//    }
//
//
////    signed char* int8_buf = (signed char*)malloc(inputs[0].size);
////    if (!int8_buf) {
////        free(dst_img.virt_addr);
////        return -1;
////    }
////
////
////    for (int i = 0; i < inputs[0].size; i++) {
////        int8_buf[i] = (signed char)(dst_img.virt_addr[i] - 128); // UINT8 -> INT8
////        if (i<10) LOGE("UINT8 -> INT8: %d", int8_buf[i]);
////    }
//
//    // 3.对输入进行前处理
//    // letterbox操作：在对图片进行resize时，保持原图的长宽比进行等比例缩放，当长边 resize 到需要的长度时，短边剩下的部分采用灰色填充。
//    // letterbox
//    LOGI("==== convert_image_with_letterbox Input ====");
//    LOGI("src img: width=%d, height=%d, format=%d, size=%d, virt_addr=%p",
//         img->width, img->height, img->format, img->size, img->virt_addr);
//
//    LOGI("dst img: width=%d, height=%d, format=%d, size=%d, virt_addr=%p",
//         dst_img.width, dst_img.height, dst_img.format, dst_img.size, dst_img.virt_addr);
//
//    LOGI("letterbox bg_color=%d", bg_color);
//    ret = convert_image_with_letterbox(img, &dst_img, &letter_box, bg_color);
//    if (ret < 0) {
//        LOGE("convert_image_with_letterbox fail! ret=%d\n", ret);
//        goto out;
//    }
//    LOGI("Letterbox: scale=%f, x_pad=%d, y_pad=%d",
//         letter_box.scale, letter_box.x_pad, letter_box.y_pad);
//
//    // Set Input Data
////    inputs[0].index = 0;
////    inputs[0].type = RKNN_TENSOR_UINT8;
////    inputs[0].fmt = RKNN_TENSOR_NHWC;
////    inputs[0].size = app_ctx->model_width * app_ctx->model_height * app_ctx->model_channel;
////    inputs[0].buf = dst_img.virt_addr;
//
//// 量化输入
//
//// 然后根据 dst_img 生成 int8_buf
//    signed char* int8_buf = (signed char*)malloc(dst_img.size);
//    if (!int8_buf) { goto out; }
//
//// 遍历 dst_img，按模型的 scale 和 zp 量化
//    float scale = app_ctx->input_scale;  // 或者 0.003922f
//    int zp = app_ctx->input_zp;           // 或者 -128
//
//    for (int i = 0; i < dst_img.size; i++) {
//        float val = (float)dst_img.virt_addr[i] * scale + zp;
//        if (val > 127) val = 127;
//        if (val < -128) val = -128;
//        int8_buf[i] = (int8_t)val;
//    }
//
//    inputs[0].index = 0;
//    inputs[0].buf = int8_buf;
//    inputs[0].type = RKNN_TENSOR_INT8;    // ★★ 关键修改
//    inputs[0].fmt = RKNN_TENSOR_NHWC;
//    inputs[0].size = app_ctx->model_width * app_ctx->model_height * app_ctx->model_channel;
////    inputs[0].buf = dst_img.virt_addr;    // 注意！不能再传 uint8 的 dst_img.virt_addr
//
//    LOGI("inputs[0]: size=%d, first 10 bytes=", inputs[0].size);
//    for (int i = 0; i < 10; i++) {
//        LOGI("%d ", ((unsigned char*)inputs[0].buf)[i]);
//    }
//
//    // 4.设置输入数据
//    ret = rknn_inputs_set(app_ctx->rknn_ctx, app_ctx->io_num.n_input, inputs);
//    if (ret < 0) {
//        LOGE("rknn_input_set fail! ret=%d\n", ret);
//        goto out;
//    }
//
//    // 5.进行模型推理
//    // Run
//    LOGI("rknn_run\n");
//    start_us = getCurrentTimeUs();
//    ret = rknn_run(app_ctx->rknn_ctx, nullptr);
//    elapse_us = getCurrentTimeUs() - start_us;
//    LOGI("normal Elapse Time = %.2fms, FPS = %.2f\n", elapse_us / 1000.f,
//         1000.f * 1000.f / elapse_us);
//    if (ret < 0) {
//        LOGE("rknn_run fail! ret=%d\n", ret);
//        goto out;
//    }
//
//    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
//        LOGI("outputs[%d].buf=%p", i, outputs[i].buf);
//        if (!outputs[i].buf) {
//            LOGE("outputs[%d].buf is NULL! Cannot print values.", i);
//        }
//    }
//
//    // Get Output
//    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
//        outputs[i].index = i;
//        outputs[i].want_float = (!app_ctx->is_quant);
//    }
//    // 6.获取推理结果数据
//    ret = rknn_outputs_get(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs, NULL);
//
//
//    // free 临时 buffer
//    if (int8_buf) free(int8_buf);
//    if (dst_img.virt_addr) free(dst_img.virt_addr);
//
//
//    if (ret < 0) {
//        LOGE("rknn_outputs_get fail! ret=%d\n", ret);
//        goto out;
//    }
//
//    for (int i = 0; i < app_ctx->io_num.n_output; i++) {
//        output_data[i] = outputs[i].buf;
//
//        if (output_data[i] == NULL) {
//            LOGE("output_data[%d].buf is NULL!", i);
//        } else {
//            LOGI("output[%d] first 10 values:", i);
//            float* ptr = (float*)output_data[i];
//            int print_num = 10;
//            for (int j = 0; j < print_num; j++) {
//                LOGI("output[%d][%d]=%f", i, j, ptr[j]);
//            }
//        }
//    }
//
//#ifdef PERF_DETAIL
//    // 查询模型逐层耗时，单位是微妙
//    rknn_perf_detail perf_detail;
//    ret = rknn_query(app_ctx->rknn_ctx, RKNN_QUERY_PERF_DETAIL, &perf_detail, sizeof(perf_detail));
////    LOGI("-->模型逐层耗时:%s\n", perf_detail.perf_data);
//    // __android_log_print输出不全，使用__android_log_write输出大量数据
//    __android_log_write(ANDROID_LOG_INFO, "YOLOV5", perf_detail.perf_data);
//#endif
//
//    // 7.对输出进行后处理
//    LOGI("post_process start, letterbox scale=%f, x_pad=%d, y_pad=%d", letter_box.scale, letter_box.x_pad, letter_box.y_pad);
//    post_process(app_ctx, output_data, &letter_box, box_conf_threshold, nms_threshold, od_results);
//    LOGI("post_process end, detected count=%d", od_results->count);
//    // Post Process
////    post_process(app_ctx, output_data, &letter_box, box_conf_threshold, nms_threshold, od_results);
//
//    // 8.释放输出数据内存
//    // Remeber to release rknn output
//    rknn_outputs_release(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs);
//
//    out:
//    if (dst_img.virt_addr != NULL) {
//        free(dst_img.virt_addr);
//    }
//
//    return ret;
//}