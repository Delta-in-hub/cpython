import math
import os
import sys
from functools import lru_cache

# ===== 1. 手动实现的简单缓存 wrapper =====
def my_cache(func):
    cache = {}
    def wrapper(*args):
        if args in cache:
            return cache[args]
        cache[args] = func(*args)
        return cache[args]
    return wrapper

# ===== 2. 使用 functools 的缓存（更简洁）=====
@lru_cache
def cached_sqrt_functools(x):
    return math.sqrt(x)

# ===== 被缓存的函数（用 math）=====
def cached_sqrt_manual(x):
    return math.sqrt(x)

# ===== 主程序（用 os 和 sys）=====
def main():
    print(f"[PID {os.getpid()}] Running {sys.argv[0]}...")

    nums = [4, 9, 4, 16, 9]  # 有重复，可测试缓存

    print("\n✅ Manual cache:")
    for n in nums:
        print(f"sqrt({n}) = {cached_sqrt_manual(n)}")

    print("\n✅ functools cache:")
    for n in nums:
        print(f"sqrt({n}) = {cached_sqrt_functools(n)}")

if __name__ == "__main__":
    main()
