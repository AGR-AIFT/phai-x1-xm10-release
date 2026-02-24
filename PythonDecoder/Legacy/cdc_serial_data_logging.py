import serial
import threading
import queue
import struct
import time
import os

# --- 1. 통신 설정 (Configuration) ---
SERIAL_PORT = 'COM6'  # 장치관리자에서 확인된 포트 번호
BAUD_RATE   = 921600  # USB-CDC에서는 실제 속도와 무관하지만 설정 필요
TIMEOUT_SEC = 1       # 읽기 타임아웃

# --- 2. 데이터 포맷 정의 (펌웨어 구조체와 100% 일치) ---
# <  : 리틀 엔디안
# I  : uint32_t (LoopCnt) - 4 bytes
# B  : uint8_t  (Mode)    - 1 byte
# B  : uint8_t  (Level)   - 1 byte
# 8f : float * 8개       - 32 bytes
# ?  : bool              - 1 byte
# ?  : bool              - 1 byte
# B  : uint8_t           - 1 byte
# B  : uint8_t           - 1 byte
# 21f: float * 21개      - 84 bytes
# --------------------------------------
# Total: 4+1+1+32+1+1+1+1+84 = 126 Bytes
STRUCT_FMT = '<IBB' + '8f' + '??BB' + '21f'
PACKET_SIZE = struct.calcsize(STRUCT_FMT)

# CSV 헤더 (데이터 순서대로)
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

# 스레드 간 데이터 공유를 위한 큐
data_queue = queue.Queue()
is_running = True

def rx_thread_func():
    """ 
    [수신 스레드] 
    오직 시리얼 포트에서 데이터를 읽어 큐에 넣는 역할만 수행합니다.
    파싱이나 파일 쓰기 같은 무거운 작업은 하지 않습니다.
    """
    global is_running
    ser = None
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT_SEC)
        print(f"✅ 시리얼 포트 연결됨: {SERIAL_PORT}")
        print(f"📦 패킷 크기: {PACKET_SIZE} Bytes")
        
        # 1. 모니터링 시작 명령 전송
        # (펌웨어의 cdc_handler가 이를 감지하고 전송을 시작함)
        print("🚀 모니터링 시작 명령 전송 (AGRB MON START)")
        ser.write(b"AGRB MON START")
        
        while is_running:
            # 2. 정해진 패킷 크기만큼 읽기 (Blocking Read)
            # MSC 파일과 달리, 여기서는 4바이트 사이즈 헤더가 없습니다. 바로 데이터입니다.
            try:
                raw_data = ser.read(PACKET_SIZE)
                if len(raw_data) == PACKET_SIZE:
                    data_queue.put(raw_data)
                # 타임아웃 등으로 데이터가 덜 들어온 경우
                if len(raw_data) > 0:
                    print(f"⚠️ 불완전한 패킷 수신: {len(raw_data)} bytes")
            except serial.SerialException:
                break
                    
    except Exception as e:
        print(f"❌ 통신 에러 발생: {e}")
        is_running = False

    finally:
        if ser and ser.is_open:
            # 종료 시 중단 명령 전송 시도
            print("\n🛑 모니터링 중지 명령 전송 (AGRB_MON_STOP)")
            try:
                ser.write(b"AGRB MON STOP")
                time.sleep(0.1) # 전송될 시간 확보
            except:
                pass
            ser.close()
            print("🔌 시리얼 포트 닫힘")

def main():
    global is_running
    
    print("--- USB CDC 실시간 데이터 로거 시작 ---")
    print(f"구조체 크기: {PACKET_SIZE} Bytes")
    
    # 수신 스레드 시작
    t = threading.Thread(target=rx_thread_func)
    t.daemon = True
    t.start()
    
    output_filename = "cdc_monitoring_data_log.csv"
    
    # 파일 열기 (쓰기 모드)
    with open(output_filename, "w", encoding='utf-8', newline='') as f:
        # 헤더 쓰기
        f.write(CSV_HEADER)
        print(f"💾 저장 파일: {output_filename}")
        print("데이터 수신 대기 중... (Ctrl+C로 종료)")
        
        total_count = 0
        start_time = time.time()
        
        try:
            while is_running:
                try:
                    # 큐에서 데이터 꺼내기 (1초 대기)
                    packet = data_queue.get(timeout=1)
                except queue.Empty:
                    if not t.is_alive(): break # 수신 스레드 죽으면 종료
                    continue

                # --- 데이터 파싱 (Decoding) ---
                try:
                    data = struct.unpack(STRUCT_FMT, packet)
                    
                    # data tuple 구조:
                    # [0]: LoopCnt, [1]: Mode, [2]: Level
                    # [3~10]: Angles(8f), [11~12]: Contact(2?), [13~14]: Gait(2B), [15~35]: Rest(21f)

                    csv_line = []
                    
                    # 1. 정수형 데이터
                    csv_line.append(str(data[0])) # LoopCnt
                    csv_line.append(str(data[1])) # SuitMode
                    csv_line.append(str(data[2])) # Level
                    
                    # 2. Float 그룹 1 (8개)
                    for val in data[3:11]:
                        csv_line.append(f"{val:.4f}")
                    
                    # 3. Bool -> Int 변환 (0 or 1)
                    csv_line.append("1" if data[11] else "0")
                    csv_line.append("1" if data[12] else "0")
                    
                    # 4. Gait Info
                    csv_line.append(str(data[13]))
                    csv_line.append(str(data[14]))
                    
                    # 5. Float 그룹 2 (21개)
                    for val in data[15:]:
                        csv_line.append(f"{val:.4f}")
                    
                    # 파일 저장
                    f.write(",".join(csv_line) + "\n")
                    total_count += 1
                    
                    # 상태 표시 (1000개마다 점 찍기)
                    if total_count % 1000 == 0:
                        print(f". (수신: {total_count}개, LoopCnt: {data[0]})", end='', flush=True)
                        
                except struct.error as e:
                    print(f"\n❌ 파싱 오류: {e}")
                    continue

        except KeyboardInterrupt:
            print("\n🛑 사용자 종료 요청")
    
    is_running = False
    t.join()
    print(f"\n✅ 종료 완료. 총 {total_count}개의 데이터가 저장되었습니다.")

if __name__ == "__main__":
    main()