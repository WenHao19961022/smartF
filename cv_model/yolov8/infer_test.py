import os
import time
import numpy as np
import cv2
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit

TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

class HostDeviceMem(object):
    def __init__(self, host_mem, device_mem):
        self.host = host_mem
        self.device = device_mem

def load_engine(engine_path):
    with open(engine_path, "rb") as f, trt.Runtime(TRT_LOGGER) as runtime:
        return runtime.deserialize_cuda_engine(f.read())

def allocate_buffers(engine):
    inputs, outputs, bindings = [], [], []
    stream = cuda.Stream()
    for i in range(engine.num_bindings):
        size = trt.volume(engine.get_binding_shape(i))
        dtype = trt.nptype(engine.get_binding_dtype(i))
        host_mem = cuda.pagelocked_empty(size, dtype)
        device_mem = cuda.mem_alloc(host_mem.nbytes)
        bindings.append(int(device_mem))
        if engine.binding_is_input(i):
            inputs.append(HostDeviceMem(host_mem, device_mem))
        else:
            outputs.append(HostDeviceMem(host_mem, device_mem))
    return inputs, outputs, bindings, stream

def do_inference(context, bindings, inputs, outputs, stream):
    [cuda.memcpy_htod_async(inp.device, inp.host, stream) for inp in inputs]
    context.execute_async_v2(bindings=bindings, stream_handle=stream.handle)
    [cuda.memcpy_dtoh_async(out.host, out.device, stream) for out in outputs]
    stream.synchronize()
    return [out.host for out in outputs]

def main():
    # 修正路径确保能直接读到 yolov8s.engine
    engine_path = "./yolov8s.engine"
    if not os.path.exists(engine_path):
        print(f"❌ 找不到 engine 文件: {engine_path}")
        return

    print("--> 正在加载 TensorRT Engine...")
    engine = load_engine(engine_path)
    context = engine.create_execution_context()
    
    # 检查并打印模型的实际输入输出通道名称和形状
    print("\n========= [Debug] 引擎输入输出绑定信息 =========")
    for i in range(engine.num_bindings):
        name = engine.get_binding_name(i)
        shape = engine.get_binding_shape(i)
        is_input = engine.binding_is_input(i)
        print(f"Binding {i} -> Name: {name} | Shape: {shape} | {'[输入]' if is_input else '[输出]'}")
    print("==================================================\n")

    inputs, outputs, bindings, stream = allocate_buffers(engine)

    print("--> 正在初始化摄像头 /dev/video0...")
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, 30)

    if not cap.isOpened():
        print("❌ 无法打开摄像头 /dev/video0")
        return

    print("--> 开始捕获图像，即将打印第一帧的全部输出细节 (请拿一个水果对着镜头)...")
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                continue

            # 图像预处理
            img_in = cv2.resize(frame, (640, 640))
            img_in = img_in[:, :, ::-1].transpose((2, 0, 1))
            img_in = np.ascontiguousarray(img_in, dtype=np.float32) / 255.0
            
            np.copyto(inputs[0].host, img_in.ravel())

            # 硬件加速推理
            trt_outputs = do_inference(context, bindings=bindings, inputs=inputs, outputs=outputs, stream=stream)
            
            # 拿到原始展平的一维数组
            raw_output = trt_outputs[0]
            
            # 获取输出绑定的目标形状 (比如 YOLOv8 常见的 (1, 84, 8400))
            output_shape = engine.get_binding_shape(1) 
            
            # 将一维数据重塑为原本的张量维度，便于分析
            try:
                reshaped_output = raw_output.reshape(output_shape)
            except Exception:
                reshaped_output = raw_output # 若Shape不对则保持一维

            print("\n🚨 ====== [Debug] 监测到水果检测模型原始输出数据 ======")
            print(f"1. 原始一维数组大小 (Total Elements): {raw_output.size}")
            print(f"2. 还原后的张量形状 (Reshaped Shape): {reshaped_output.shape}")
            print(f"3. 原始数据前 100 个浮点数样例:")
            print(raw_output[:100])
            print("\n4. 尝试打印前 5 个锚点(Anchors)的完整特征向量数据:")
            
            # 兼容处理：如果维度是三维 (1, 84, 8400) 或者是 (1, 8400, 84)
            if len(reshaped_output.shape) == 3:
                # 针对标准 YOLOv8 格式 [1, 84, 8400]，转置后打印更直观
                if reshaped_output.shape[1] < reshaped_output.shape[2]:
                    view_data = reshaped_output[0].T # 转为 [8400, 84]
                    print(f"   [格式 A 检测] 提取到的前5个锚点数据 [X, Y, W, H, 各水果类别得分...]:")
                    for k in range(min(5, view_data.shape[0])):
                        print(f"   Anchor {k}: {view_data[k][:15]} ... (总长度 {view_data.shape[1]})")
                else:
                    view_data = reshaped_output[0] # 本身就是 [8400, 84]
                    print(f"   [格式 B 检测] 提取到的前5个锚点数据:")
                    for k in range(min(5, view_data.shape[0])):
                        print(f"   Anchor {k}: {view_data[k][:15]} ... (总长度 {view_data.shape[1]})")
            else:
                print(f"   [非常规维度] 数组前100个元素切片展示: {raw_output[:100]}")
            
            print("========================================================\n")
            
            # 打印完毕后直接断开，避免刷屏
            break

    except KeyboardInterrupt:
        pass
    finally:
        cap.release()

if __name__ == "__main__":
    main()