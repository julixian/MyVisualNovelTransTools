import numpy as np
import matplotlib.pyplot as plt
import sys
from PIL import Image

def file_to_grayscale_image(input_file, output_file, width, height):
    try:
        # 读取文件
        with open(input_file, 'rb') as f:
            data = f.read()
        
        # 检查文件大小是否为64字节
        if len(data) != width * height:
            print(f"错误：文件大小必须为宽 * 高字节，当前文件大小为{len(data)}字节")
            return False
        
        # 将字节数据转换为0-255的整数数组
        pixel_values = np.frombuffer(data, dtype=np.uint8)
        
        # 重塑为8x8矩阵
        image_array = pixel_values.reshape(height, width)
        image_array = np.flip(image_array, axis=0)
        
        # 使用PIL创建并保存图像
        image = Image.fromarray(image_array, mode='L')  # 'L'表示灰度图
        image.save(output_file)
        
        # 可选：显示图像
        #plt.figure(figsize=(5, 5))
        #plt.imshow(image_array, cmap='gray')
        #plt.title('8x8 灰度图')
        #plt.colorbar(label='灰度值')
        #plt.show()
        
        print(f"成功将文件转换为灰度图并保存为 {output_file}")
        return True
    
    except Exception as e:
        print(f"发生错误：{e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("用法：python script.py 输入文件 输出文件.png 宽 高")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        width = int(sys.argv[3])
        height = int(sys.argv[4])
        file_to_grayscale_image(input_file, output_file, width, height)
