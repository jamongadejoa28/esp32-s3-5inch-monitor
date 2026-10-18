#pragma once

// 최초 설치: 이 줄을 켜면 부팅할 때마다 5점 보정을 실행합니다.
// #define BOARD_TOUCH_CALIBRATE_ON_BOOT

// 보정 완료 후: 아래 예시 대신 터미널에 출력된 설정 블록을 붙여 넣고 위 줄은 주석 처리하세요.
// 예시 계수(identity)를 실측 보정값으로 바꾸기 전에는 고정 모드를 켜지 마세요.
#define BOARD_TOUCH_USE_FIXED_CALIBRATION
#define BOARD_TOUCH_CALIBRATION_COEFFICIENTS \
    { .a = 1.018766522f, .b = -0.019118981f, .c = -3.691864014f, \
      .d = 0.004455850f, .e = 0.996872962f, .f = -0.536091566f }

// 재보정: 고정 모드와 계수 정의를 주석 처리하고 최초 설치 줄을 켜세요.
// 이 파일은 EEZ 생성물과 분리되어 있어 UI를 다시 가져와도 유지됩니다.
