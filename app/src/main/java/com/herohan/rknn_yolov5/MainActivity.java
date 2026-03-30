package com.herohan.rknn_yolov5;

import android.Manifest;
import android.app.Activity;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.media.ExifInterface;
import android.graphics.Matrix;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;
import android.view.View;
import android.webkit.MimeTypeMap;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.Toast;

import com.herohan.rknn_yolov5.utils.AssetHelper;
import com.herohan.rknn_yolov5.utils.DefaultModel;
import com.tencent.cos.xml.CosXmlService;
import com.tencent.cos.xml.CosXmlServiceConfig;
import com.tencent.cos.xml.exception.CosXmlClientException;
import com.tencent.cos.xml.exception.CosXmlServiceException;
import com.tencent.cos.xml.listener.CosXmlProgressListener;
import com.tencent.cos.xml.listener.CosXmlResultListener;
import com.tencent.cos.xml.model.CosXmlRequest;
import com.tencent.cos.xml.model.CosXmlResult;
import com.tencent.cos.xml.transfer.COSXMLDownloadTask;
import com.tencent.cos.xml.transfer.COSXMLUploadTask;
import com.tencent.cos.xml.transfer.TransferConfig;
import com.tencent.cos.xml.transfer.TransferManager;
import com.tencent.cos.xml.transfer.TransferState;
import com.tencent.cos.xml.transfer.TransferStateListener;
import com.tencent.qcloud.core.auth.ShortTimeCredentialProvider;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class MainActivity extends Activity {
    private static final String TAG = MainActivity.class.getSimpleName();

    private static final int SELECT_IMAGE = 1;

    private ImageView imageView;
    private Bitmap bitmap = null;
    private Bitmap yourSelectedImage = null;

    private YoloV5Detect yolov5Detect = new YoloV5Detect();

    private CosXmlService cosXmlService;
    private TransferManager transferManager;

    private static final int REQUEST_PICK_IMAGE = 1001;

    private void openGallery() {
        Log.d("wxy", "openGallery: ");
//        Intent intent = new Intent(Intent.ACTION_PICK);
//        intent.setType("image/*");
//        startActivityForResult(intent, REQUEST_PICK_IMAGE);

        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{
                "image/*",
                "video/*"
        });
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(intent, REQUEST_PICK_IMAGE);
    }


    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        Log.d(TAG, "onCreate: ");

        initCos();

        try {
            boolean retInit = yolov5Detect.init(AssetHelper.assetFilePath(this, DefaultModel.YOLOV5S_640_INT8.name),
                    AssetHelper.assetFilePath(this, "coco_80_labels_list.txt"), false);
            if (!retInit) {
                Log.e(TAG, "YoloV5Detect Init failed");
            }
        } catch (IOException e) {
            Log.e(TAG, "Error reading assets", e);
            finish();
        }

        // 选图上传疼腾讯云
        Button buttonUpload = (Button) findViewById(R.id.buttonUpload);
        buttonUpload.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                openGallery();
//                downloadAndSave("https://uvc2cos-1410788524.cos.ap-guangzhou.myqcloud.com/images/1773310725049.jpg");
            }
        });

        // 跳转摄像头抓图
        Button buttonCapture = (Button) findViewById(R.id.buttonCapture);
        buttonCapture.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                startActivity(new Intent(MainActivity.this, com.serenegiant.usbcameratest7.MainActivity.class));
            }
        });

        // 图库选图
        imageView = (ImageView) findViewById(R.id.imageView);
        Button buttonImage = (Button) findViewById(R.id.buttonImage);
        buttonImage.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {
                Intent i = new Intent(Intent.ACTION_PICK);
                i.setType("image/*");
                startActivityForResult(i, SELECT_IMAGE);
            }
        });

        // 图片yolo识别
        Button buttonDetect = (Button) findViewById(R.id.buttonDetect);
        buttonDetect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {
                if (yourSelectedImage == null)
                    return;

                printBitmapInfo(yourSelectedImage);

                new Thread(() -> {
                    // 子线程执行 detect
                    yolov5Detect.detect(yourSelectedImage);
                    // 回到主线程更新 UI
                    runOnUiThread(() -> {
                        imageView.setImageBitmap(yourSelectedImage);

                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                            requestPermissions(new String[]{
                                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                            }, 100);
                        }
                        // 保存识别结果
                        saveImageToGallery(yourSelectedImage);
                    });
                }).start();
//                yolov5Detect.detect(yourSelectedImage);
//
//                imageView.setImageBitmap(yourSelectedImage);
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        boolean retInit = yolov5Detect.release();
        if (!retInit) {
            Log.e(TAG, "YoloV5Detect Release failed");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == RESULT_OK && null != data) {
            Uri selectedImage = data.getData();

            try {
                if (requestCode == SELECT_IMAGE) {
                    bitmap = decodeUri(selectedImage);

                    yourSelectedImage = bitmap.copy(Bitmap.Config.ARGB_8888, true);
                    imageView.setImageBitmap(bitmap);
                }

            } catch (FileNotFoundException e) {
                Log.e("MainActivity", "FileNotFoundException");
                return;
            }
        }

        if (requestCode == REQUEST_PICK_IMAGE && resultCode == RESULT_OK) {
            if (data != null && data.getData() != null) {
                Uri uri = data.getData();
                Log.d("wxy", "选择图片: " + uri);
                uploadImage(uri);
            }
        }
    }

    public static void printBitmapInfo(Bitmap bitmap) {
        Log.d(TAG, "printBitmapInfo: ");
        Bitmap.Config config = bitmap.getConfig();
        int bytesPerPixel = 0;
        switch (config) {
            case ARGB_8888: bytesPerPixel = 4; break;
            case RGB_565: bytesPerPixel = 2; break;
            case ALPHA_8: bytesPerPixel = 1; break;
            case ARGB_4444: bytesPerPixel = 2444; break;
            default: bytesPerPixel = 0; break;
        }

        int rowBytes = bitmap.getRowBytes();
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        boolean isContinuous = (rowBytes == width * bytesPerPixel);

        Log.i("BitmapInfo", "Config=" + config + " width=" + width + " height=" + height +
                " bytesPerPixel=" + bytesPerPixel + " rowBytes=" + rowBytes +
                " isContinuous=" + isContinuous);
    }

    private Bitmap decodeUri(Uri selectedImage) throws FileNotFoundException {
        // Decode image size
        BitmapFactory.Options o = new BitmapFactory.Options();
        o.inJustDecodeBounds = true;
        BitmapFactory.decodeStream(getContentResolver().openInputStream(selectedImage), null, o);

        // The new size we want to scale to
        final int REQUIRED_SIZE = 640;

        // Find the correct scale value. It should be the power of 2.
        int width_tmp = o.outWidth, height_tmp = o.outHeight;
        int scale = 1;
        while (true) {
            if (width_tmp / 2 < REQUIRED_SIZE
                    || height_tmp / 2 < REQUIRED_SIZE) {
                break;
            }
            width_tmp /= 2;
            height_tmp /= 2;
            scale *= 2;
        }

        // Decode with inSampleSize
        BitmapFactory.Options o2 = new BitmapFactory.Options();
        o2.inSampleSize = scale;
        Bitmap bitmap = BitmapFactory.decodeStream(getContentResolver().openInputStream(selectedImage), null, o2);

        // Rotate according to EXIF
        int rotate = 0;
        try {
            ExifInterface exif = new ExifInterface(getContentResolver().openInputStream(selectedImage));
            int orientation = exif.getAttributeInt(ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_NORMAL);
            switch (orientation) {
                case ExifInterface.ORIENTATION_ROTATE_270:
                    rotate = 270;
                    break;
                case ExifInterface.ORIENTATION_ROTATE_180:
                    rotate = 180;
                    break;
                case ExifInterface.ORIENTATION_ROTATE_90:
                    rotate = 90;
                    break;
            }
        } catch (IOException e) {
            Log.e("MainActivity", "ExifInterface IOException");
        }

        Matrix matrix = new Matrix();
        matrix.postRotate(rotate);
        return Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), matrix, true);
    }

    private void saveImageToGallery(Bitmap bitmap) {

        String fileName = "YOLO_" + System.currentTimeMillis() + ".jpg";

        try {
            // 保存路径：Pictures/YOLO
            String path = Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_PICTURES).toString();

            File dir = new File(path + "/YOLO");
            if (!dir.exists()) {
                dir.mkdirs();
            }

            File file = new File(dir, fileName);
            FileOutputStream fos = new FileOutputStream(file);

            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, fos);
            fos.flush();
            fos.close();

            // 👉 通知系统刷新图库
            Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
            intent.setData(Uri.fromFile(file));
            sendBroadcast(intent);

            Toast.makeText(this, "保存成功：" + file.getAbsolutePath(), Toast.LENGTH_SHORT).show();

        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "保存失败", Toast.LENGTH_SHORT).show();
        }
    }



    /**
     * 初始化 COS
     */
    private void initCos() {

        String region = "ap-guangzhou";

        ShortTimeCredentialProvider credentialProvider =
                new ShortTimeCredentialProvider(
                        "你的id",
                        "你的key",
                        300
                );

        CosXmlServiceConfig serviceConfig = new CosXmlServiceConfig.Builder()
                .setRegion(region)
                .isHttps(true)
                .builder();

        cosXmlService = new CosXmlService(getApplicationContext(), serviceConfig, credentialProvider);

        TransferConfig transferConfig = new TransferConfig.Builder().build();

        transferManager = new TransferManager(cosXmlService, transferConfig);
    }

    /**
     * 上传图片
     */
    private void uploadImage(Uri uri) {

        String bucket = "uvc2cos-1410788524";

        String extension = MimeTypeMap.getSingleton()
                .getExtensionFromMimeType(getContentResolver().getType(uri));

        if (extension == null) {
            extension = "mp4"; // 默认兜底
        }

        String cosPath = "images/" + System.currentTimeMillis() + "." + extension;
//        String cosPath = "images/" + System.currentTimeMillis() + ".jpg";

        Log.d("wxy", "准备上传");

        File file = copyUriToFile(uri);

        Log.d("wxy", "上传文件路径: uri=" + uri + " file=" + file.getAbsolutePath());

        COSXMLUploadTask uploadTask = transferManager.upload(
                bucket,
                cosPath,
                file.getAbsolutePath(),
                null
        );

        uploadTask.setTransferStateListener(new TransferStateListener() {
            @Override
            public void onStateChanged(TransferState state) {
                Log.d("wxy", "上传状态: " + state);
            }
        });

        uploadTask.setCosXmlProgressListener(new CosXmlProgressListener() {
            @Override
            public void onProgress(long complete, long target) {
                long progress = complete * 100 / target;
                Log.d("wxy", "上传进度: " + progress + "%");
            }
        });

        uploadTask.setCosXmlResultListener(new CosXmlResultListener() {
            @Override
            public void onSuccess(CosXmlRequest request, CosXmlResult result) {

                final String url = "https://" + bucket + ".cos.ap-guangzhou.myqcloud.com/" + cosPath;

                Log.d("wxy", "上传成功: " + url);

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "上传成功", Toast.LENGTH_LONG).show();
                    }
                });
            }

            @Override
            public void onFail(
                    CosXmlRequest request,
                    CosXmlClientException clientException,
                    CosXmlServiceException serviceException) {

                Log.e("wxy", "上传失败");

                if (clientException != null) {
                    clientException.printStackTrace();
                }

                if (serviceException != null) {
                    Log.e("wxy", "statusCode=" + serviceException.getStatusCode());
                    Log.e("wxy", "errorCode=" + serviceException.getErrorCode());
                    Log.e("wxy", "requestId=" + serviceException.getRequestId());
                    Log.e("wxy", "message=" + serviceException.getMessage());
                }

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "上传失败", Toast.LENGTH_LONG).show();
                    }
                });
            }
        });
    }

    /**
     * Uri -> File
     */
    private File copyUriToFile(Uri uri) {

        File file = new File(getCacheDir(),
                "upload_" + System.currentTimeMillis() + ".jpg");

        try {
            InputStream input = getContentResolver().openInputStream(uri);
            FileOutputStream output = new FileOutputStream(file);

            byte[] buffer = new byte[1024];
            int len;
            while ((len = input.read(buffer)) > 0) {
                output.write(buffer, 0, len);
            }

            input.close();
            output.close();

        } catch (Exception e) {
            e.printStackTrace();
        }

        return file;
    }


    private void downloadAndSave(String url) {

        new Thread(() -> {
            try {

                // ✅ 1. 创建文件
                File dir = new File(getExternalFilesDir(null), "images");
                if (!dir.exists()) dir.mkdirs();

                File file = new File(dir,
                        "download_" + System.currentTimeMillis() + ".jpg");

                // 防止同名目录问题
                if (file.exists()) {
                    file.delete();
                }

                // ✅ 2. 下载
                URL u = new URL(url);
                HttpURLConnection conn = (HttpURLConnection) u.openConnection();
                conn.connect();

                InputStream in = conn.getInputStream();
                FileOutputStream out = new FileOutputStream(file);

                byte[] buffer = new byte[4096];
                int len;

                while ((len = in.read(buffer)) != -1) {
                    out.write(buffer, 0, len);
                }

                in.close();
                out.close();

                // ✅ 3. 校验文件
                Log.d("wxy", "exists=" + file.exists());
                Log.d("wxy", "isDir=" + file.isDirectory());
                Log.d("wxy", "length=" + file.length());

                if (!file.exists() || file.isDirectory() || file.length() == 0) {
                    Log.e("wxy", "❌ 下载失败（无效文件）");
                    return;
                }

                // ✅ 4. 保存到图库（主线程）
                runOnUiThread(() -> saveToGallery(file));

            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }


    private void saveToGallery(File file) {

        // ✅ 1. 基础校验（防止 EISDIR）
        if (file == null || !file.exists() || file.isDirectory()) {
            Log.e("wxy", "文件无效: " + (file == null ? "null" : file.getAbsolutePath()));
            return;
        }

        ContentValues values = new ContentValues();
        values.put(MediaStore.Images.Media.DISPLAY_NAME,
                "IMG_" + System.currentTimeMillis() + ".jpg");
        values.put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg");

        // ✅ Android 10+ 写入相册路径
        if (Build.VERSION.SDK_INT >= 29) {
//            values.put(MediaStore.Images.Media.RELATIVE_PATH, "DCIM/Camera");
        }

        Uri uri = null;
        OutputStream out = null;
        FileInputStream in = null;

        try {
            // ✅ 2. 插入 MediaStore
            uri = getContentResolver().insert(
                    MediaStore.Images.Media.EXTERNAL_CONTENT_URI,
                    values
            );

            if (uri == null) {
                Log.e("wxy", "创建 MediaStore 记录失败");
                return;
            }

            // ✅ 3. 打开输出流
            out = getContentResolver().openOutputStream(uri);
            if (out == null) {
                Log.e("wxy", "输出流为空");
                return;
            }

            // ✅ 4. 读取文件写入
            in = new FileInputStream(file);

            byte[] buffer = new byte[4096];
            int len;

            while ((len = in.read(buffer)) != -1) {
                out.write(buffer, 0, len);
            }

            out.flush();

            Log.d("wxy", "✅ 已保存到图库: " + uri.toString());

        } catch (Exception e) {
            e.printStackTrace();

            // ❗写入失败时删除残留
            if (uri != null) {
                getContentResolver().delete(uri, null, null);
            }

        } finally {
            try {
                if (in != null) in.close();
                if (out != null) out.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }


}
