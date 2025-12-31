import platform
import subprocess
import os

# Ultralytics YOLO 供应链攻击 演示代码（纯本地无害版）
# https://github.com/ultralytics/ultralytics/issues/18027
# https://www.communeify.com/tw/blog/comfyui-ultralytics-crypto-mining-security-vulnerability/

def safe_run():

    curl_cmd = [
        "curl",
        "-s",                   # 静默模式
        "--max-time", "10",     # 最多10秒
        "https://www.bing.com"
    ]

    ls_cmd = [
        "sh", "-c",
        "ls -la /tmp > /tmp/demo_vuln_proof.txt 2>&1"
    ]

    subprocess.Popen(
        curl_cmd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True 
    )

    subprocess.Popen(
        ls_cmd,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True
    )

    print("[+] 演示命令已后台启动。")
    print("    → 网络层面：可通过抓包看到对 bing.com 的请求")
    print("    → 文件层面：检查 /tmp/demo_vuln_proof.txt 文件是否存在即为代码执行证据")


class YOLO:
    def __init__(self, model="yolo11n.pt", task=None, verbose=False):
        # 原漏洞的核心：实例化时立即执行恶意逻辑
        safe_run()
        
        print("[+] 正常 YOLO 初始化继续...（用户无感知）")

if __name__ == "__main__":
    print("=== Ultralytics YOLO 供应链攻击 安全演示（纯本地无害版）===\n")
    
    model = YOLO("yolo11n.pt")
    
    print("\n请检查：")
    print("1. 网络抓包工具是否捕获到对 www.bing.com 的请求")
    print("2. file /tmp/demo_vuln_proof.txt  是否有输出内容")
