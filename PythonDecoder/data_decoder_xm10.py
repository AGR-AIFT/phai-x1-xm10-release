import struct
import os

# --- 설정 (Configuration) ---
INPUT_FILE = 'data_010.bin'   # USB에서 가져온 바이너리 파일명
OUTPUT_FILE = 'result10.csv'    # 변환되어 저장될 엑셀/CSV 파일명

# [중요] MyData_t 구조체 포맷 (총 126 Bytes, __attribute__((packed)) 기준)
# <  : 리틀 엔디안 (STM32 기본)
# I  : uint32_t (LoopCnt) - 4 bytes
# B  : uint8_t  (Mode)    - 1 byte
# B  : uint8_t  (Level)   - 1 byte
# 8f : float * 8개 (Angles, Vel) - 32 bytes
# ?  : bool     (L_Contact) - 1 byte
# ?  : bool     (R_Contact) - 1 byte
# B  : uint8_t  (GaitState) - 1 byte
# B  : uint8_t  (GaitCycle) - 1 byte
# 21f: float * 21개 (Vel, Torque, IMU...) - 84 bytes
STRUCT_FMT = '<IBB' + '8f' + '??BB' + '21f'

# 엑셀(CSV) 첫 줄에 들어갈 헤더 (구조체 멤버 순서와 100% 일치해야 함)
CSV_HEADER = (
    "LoopCnt,SuitMode,AssistLevel,"
    "LeftHipAngle,RightHipAngle,LeftThighAngle,RightThighAngle,"
    "PelvicAngle,PelvicVelY,LeftKneeAngle,RightKneeAngle,"
    "IsLeftFootContact,IsRightFootContact,GaitState,GaitCycle,"
    "ForwardVelocity,LeftHipTorque,RightHipTorque,"
    "LeftHipMotorAngle,RightHipMotorAngle,"
    "L_HipRoll,L_HipPitch,R_HipRoll,R_HipPitch,"
    "L_AccX,L_AccY,L_AccZ,L_GyrX,L_GyrY,L_GyrZ,"
    "R_AccX,R_AccY,R_AccZ,R_GyrX,R_GyrY,R_GyrZ\n"
)

# --- 변환 로직 (Main Logic) ---
def decode_bin_to_csv():
    # 1. 구조체 1개당 예상 크기 계산 (126 바이트여야 함)
    struct_size = struct.calcsize(STRUCT_FMT)
    print(f"구조체 크기: {struct_size} Bytes (예상: 126 Bytes)")

    if not os.path.exists(INPUT_FILE):
        print(f"오류: '{INPUT_FILE}' 파일을 찾을 수 없습니다.")
        return

    with open(INPUT_FILE, 'rb') as f_in, open(OUTPUT_FILE, 'w') as f_out:
        # 2. CSV 헤더 쓰기
        f_out.write(CSV_HEADER)
        
        count = 0
        while True:
            # 3. 패킷 헤더(Size) 읽기 (4바이트, data_logger가 붙인 것)
            size_header = f_in.read(4)
            if not size_header: 
                break # 파일 끝 도달
            
            # 저장된 데이터 크기 확인
            packet_len = struct.unpack('<I', size_header)[0]
            
            # 4. 실제 데이터 읽기
            packet_data = f_in.read(packet_len)
            if len(packet_data) != packet_len:
                print("경고: 파일이 중간에 잘렸습니다.")
                break
            
            # 데이터 크기가 구조체 크기와 맞는지 확인 (다르면 건너뜀)
            if packet_len != struct_size:
                print(f"경고: 데이터 크기 불일치 (저장됨:{packet_len} != 정의됨:{struct_size}) - 건너뜀")
                continue

            # 5. 바이너리 -> 숫자 변환 (Unpacking)
            try:
                # unpack 결과는 튜플(tuple)로 반환됨
                data = struct.unpack(STRUCT_FMT, packet_data)
                
                # 6. CSV 포맷으로 변환 (소수점 4자리, bool은 0/1로 변환)
                # data[0]: LoopCnt (int)
                # data[1]: Mode (int)
                # data[2]: Level (int)
                # data[3~10]: Angles (float)
                # data[11~12]: Contacts (bool -> int 변환)
                # data[13~14]: Gait (int)
                # data[15~35]: Rest (float)
                
                csv_line = []
                
                # 정수형 데이터 처리
                csv_line.append(str(data[0])) # LoopCnt
                csv_line.append(str(data[1])) # SuitMode
                csv_line.append(str(data[2])) # Level
                
                # Float 그룹 1 (8개)
                for val in data[3:11]:
                    csv_line.append(f"{val:.4f}")
                
                # Bool -> Int (0 or 1)
                csv_line.append("1" if data[11] else "0")
                csv_line.append("1" if data[12] else "0")
                
                # Gait Info
                csv_line.append(str(data[13]))
                csv_line.append(str(data[14]))
                
                # Float 그룹 2 (21개)
                for val in data[15:]:
                    csv_line.append(f"{val:.4f}")
                
                # 파일에 쓰기 (줄바꿈 추가)
                f_out.write(",".join(csv_line) + "\n")
                count += 1
                
            except struct.error as e:
                print(f"파싱 오류 발생: {e}")
                continue

    print(f"\n변환 완료! 총 {count}개의 데이터가 '{OUTPUT_FILE}'에 저장되었습니다.")

if __name__ == "__main__":
    decode_bin_to_csv()