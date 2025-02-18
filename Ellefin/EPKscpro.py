import os
import sys

def reverse_byte(byte):
    # 将字节反转，比如0x3a变成0xa3
    return ((byte << 4) & 0xF0) | ((byte >> 4) & 0x0F)

def process_file(input_path, output_path):
    try:
        with open(input_path, 'rb') as f_in:
            # 读取二进制数据
            data = f_in.read()
            
        # 处理每个字节
        processed_data = bytearray()
        for byte in data:
            # XOR 0xd9
            xored = byte ^ 0xd9
            # 反转字节
            reversed_byte = reverse_byte(xored)
            processed_data.append(reversed_byte)
            
        # 写入新文件
        with open(output_path, 'wb') as f_out:
            f_out.write(processed_data)
            
        print(f"Successfully processed: {input_path}")
        
    except Exception as e:
        print(f"Error processing {input_path}: {str(e)}")

def main():
    if len(sys.argv) != 3:
        print("Usage: python script.py <input_folder> <output_folder>")
        return
        
    input_folder = sys.argv[1]
    output_folder = sys.argv[2]
    
    # 确保输出文件夹存在
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
    
    # 遍历输入文件夹中的所有文件
    for root, dirs, files in os.walk(input_folder):
        for file in files:
            input_path = os.path.join(root, file)
            
            # 创建对应的输出路径
            rel_path = os.path.relpath(input_path, input_folder)
            output_path = os.path.join(output_folder, rel_path)
            
            # 确保输出文件的目录存在
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            
            # 处理文件
            process_file(input_path, output_path)

if __name__ == "__main__":
    main()
