## vmi rw 기본코드를 작성해놓자

작성자: kkongnyang2 작성일: 2025-08-19

---
### 고정주소 폴링

고정주소는 단순하게 고정 가상주소와 그게 어느 프로세스의 가상주소인지만 알려주면 된다.

주소 알아내기
```
notepad.exe 실행 후 아무 텍스트 적기
텍스트를 변경해가면서 CE에서 주소 확정: 0x21777087720
```

PID 알아내기
```
$ sudo vmi-process-list win10-seabios | grep -i notepad
[ 11784] notepad.exe (struct addr:ffffd184ad9f1080)
```

코드 작성
```
$ nano ~/watch_va_poll_wait.py
```
* 대기하다가 해당 페이지가 메모리에 올라온 순간부터 감시하여 변경을 폴링하는 스크립트

실행 커맨드
```
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/watch_va_poll_wait.py \
  win10-seabios 11784 0x21777087720 64

[.] Waiting for page to become resident... (pid=11784, va=0x21777087720) [18:52:09.471] INIT len=64 decode(utf-16le): '1334#123#1\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08獯ȗ\x00' 00000000 31 00 33 00 33 00 34 00 23 00 31 00 32 00 33 00 1.3.3.4.#.1.2.3. 00000010 23 00 31 00 00 00 00 00 00 00 00 00 00 00 00 00 #.1............. 00000020 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................ 00000030 00 00 00 00 00 00 00 00 08 00 6f 73 17 02 00 00 ..........os.... [18:52:20.162] CHANGE len=64 decode(utf-16le): '1334#123#2\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08獯ȗ\x00' 00000000 31 00 33 00 33 00 34 00 23 00 31 00 32 00 33 00 1.3.3.4.#.1.2.3. 00000010 23 00 32 00 00 00 00 00 00 00 00 00 00 00 00 00 #.2............. 00000020 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ................ 00000030 00 00 00 00 00 00 00 00 08 00 6f 73 17 02 00 00 ..........os....
```


### 주소 변환 3단계

게스트 가상주소 (GVA, VA)
   └─(게스트 프로세스의 페이지테이블: CR3/DTB)→ 게스트 물리주소 (GPA/PA)
        └─(하이퍼바이저의 2차 변환: EPT/NPT)→ 호스트 물리주소 (HPA, 실제 RAM)

EPROCESS: 커널의 프로세스 객체 구조체
내용물은 PID, ActiveProcessLinks(프로세스 더블링크드 리스트), DTB, Peb(PEB 가상주소)

PEB: 유저 모드에 있는 프로세스 환경 블록
내용물은 ImageBaseAddress(메인 exe의 베이스), Ldr(모듈 리스트로 가는 포인터)

모듈: 프로세스 주소공간에 로드된 메인 EXE + 여러 DLL
PEB->Ldr->InMemoryOrderModuleList의 각 노드는 LDR_DATA_TABLE_ENTRY임.

내가 찾는 overwatch2.exe의 베이스라는 건 일반적으로 메인EXE의 베이스.


따라서 베이스 구하는 절차
커널 리스트에서 EPROCESS를 찾고, PEB.Ldr.InMemoryOrderModuleList를 순회하여 각 LDR_DATA_TABLE_ENTRY.DllBase를 수집. 이걸로 각 모듈의 베이스를 알 수 있고 PEB.ImageBaseAddress가 메인 EXE 베이스


### 구성요소 기능

libvmi 코어는 C API로 도메인에 attach 하고, PA/VA 읽기쓰기, PID/DTB 기반 번역, 이벤트 등의 기능이 있다. 따라서 원시 메모리를 접근하거나 VA를 PA로 번역하거나 페이지 트랩 등의 저수준 작업이 가능하다.

libvmi 예제는 그 위에 만든 데모로 프로세스 나열, 메모리 덤프, GUID 확인 등의 대표 작업이 가능하다.

python 바인딩은 read_va, write_va를 호출할 수 있다.

프로파일은 구조체나 심볼 오프셋 정보가 들어있어서(EPROCESS/PEB/LDR/UNICODE_STRING/모듈 리스트 등) 이게 있으면 vmi-process-list, PID->EPROCESS, PEB->Ldr->모듈베이스, 문자열 읽기 등 의미를 해석하는 코드를 작동할 수 있다.


### 체인주소 r/w

~/game-cheat/code/chain/
 ├─ ist_helpers.py         # IST(JSON)에서 필드 오프셋 얻기 + UNICODE_STRING 읽기
 ├─ vmi_helpers.py         # LibVMI 공통 헬퍼(포인터/정수 읽기·쓰기, wait-present 등)
 ├─ find_module_base.py    # 프로세스에서 모듈 베이스(=ImageBase/DllBase) 찾기
 ├─ walk_chain.py          # 모듈 베이스 + 체인(+오프셋/*역참조) 따라 최종 주소 계산
 └─ rw_value.py            # 최종 주소에서 값 읽기/쓰기 (u32/u64/utf16)


코드 작성
```
$ nano ~/game-cheat/code/chain/ist_helpers.py
$ nano ~/game-cheat/code/chain/vmi_helpers.py
$ nano ~/game-cheat/code/chain/find_module_base.py
$ nano ~/game-cheat/code/chain/walk_chain.py
$ nano ~/game-cheat/code/chain/rw_value.py
```

에러 해결
```
ist_helpers.py 에서 [-] 'types' 오류가 떠서
$ sudo python3 - <<'PY'
import json, gzip, lzma
p="/root/symbols/ntkrnlmp.json"
with open(p,'rb') as f:
    sig=f.read(6)
op=open
if sig.startswith(b'\xFD7zXZ\x00'): op=lzma.open
elif sig.startswith(b'\x1F\x8B'): op=gzip.open
with op(p,'rt',encoding='utf-8') as fp:
    d=json.load(fp)
print("top-level keys:", list(d.keys())[:6])
print("has types:", 'types' in d, "has symbols:", 'symbols' in d)
PY
top-level keys: ['base_types', 'enums', 'metadata', 'symbols', 'user_types']
상태를 점검했더니 요즘 volatility3 isf json은 types 대신 user_types를 쓰는 걸 확인. 그래서 널널하게 고쳐줌.

그밖에 다른 py에서도 인자 개수나 python-libvmi (v3.7.1)엔 어떤 배포본에서는 Libvmi.get_process_by_pid(...) 같은 편의 래퍼가 포함되지 않는 문제 등을 하드코딩 해줌.
```

실행 커맨드
```
$ sudo vmi-process-list win10-seabios | grep -i overwatch
[12164] Overwatch.exe (struct addr:ffffb50fd508a280)
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/find_module_base.py \
  win10-seabios 12164 Overwatch.exe /root/symbols/ntkrnlmp.json
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
0x00007ff6134e0000
```
* 참고로 vmi-process-list는 15바이트까지만 돼서 모듈명 뒤 이름은 짤림. 알아서 적어주기.

실행 커맨드2
```
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/walk_chain.py   win10-seabios 10220 Tutorial-x86_64.exe /root/symbols/ntkrnlmp.json "+0x34ECA0 * +0x10 * +0x18 * +0x0 * +0x18" 64
[+] base=Tutorial-x86_64.exe@0x0000000100000000
[+] final address = 0x0000000001587108
00000000  e6 0b 00 00 00 00 00 00 00 00 00 00 00 00 00 00   ................
00000010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   ................
00000020  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   ................
00000030  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00   ................
```
* 64는 그 주소에서 몇 바이트를 덤프해서 보여줄지를 뜻하는 len임.

실행 커맨드3
```
# 읽기 (u32)
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/rw_value.py \
  win10-seabios 10220 Tutorial-x86_64.exe /root/symbols/ntkrnlmp.json "+0x34ECA0 * +0x10 * +0x18 * +0x0 * +0x18" --read --type u32
[+] final address: 0x0000000001587108
u32 = 3046 (0x00000be6)

# 쓰기 (u32)
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/rw_value.py \
  win10-seabios 10220 Tutorial-x86_64.exe /root/symbols/ntkrnlmp.json "+0x34ECA0 * +0x10 * +0x18 * +0x0 * +0x18" --write --type u32 --value 1337
[+] final address: 0x0000000001587108
[OK] wrote u32 1337 at 0x0000000001587108
```
* u32는 4bytes로 값 읽기
* 문자열(UTF-16LE)로 쓰기를 하고 싶다면 --write --type utf16 --text "KKN!" 해주면 됨



### 모듈로 소환

이제 모든 from에 레포 이름을 포함시켜라(ex chain.ist_helpers) 같은 경로에 있다고 해도 마찬가지이다
그리고 모든 레포 앞에 __init__.py라는 빈 파일을 넣어주면 절대경로를 서로 잘 인식한다.

커맨드
```
$ cd ~/game-cheat
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_list_sections \
  win10 4888 Overwatch.exe root/symbols/ntkrnlmp.json
```

* 툴로 사용할 예정
* 여기서 작성한 코드는 rw에 저장되어 있음.