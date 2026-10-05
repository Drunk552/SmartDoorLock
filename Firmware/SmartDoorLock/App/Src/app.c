/**
 * @file    app.c
 * @brief   智能门锁应用状态机与多认证方式的统一调度。
 * @details 本模块只编排界面、认证、门锁和告警服务；硬件驱动层不在此处决定
 *          开锁。所有等待均由系统节拍推进，避免主循环长时间阻塞。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-APP-001、REQ-AUTH-001、REQ-LOCK-001、REQ-SEC-001。
 * 关联设计：DES-APP-001。
 */
#include "app.h"

#include "auth_service.h"
#include "bsp_as608.h"
#include "bsp_at24c02.h"
#include "bsp_board.h"
#include "bsp_ds3231.h"
#include "bsp_keypad.h"
#include "bsp_log.h"
#include "bsp_oled.h"
#include "bsp_rc522.h"
#include "credential_service.h"
#include "lock_service.h"
#include "storage_service.h"
#include "stm32f1xx_hal.h"
#include "time_utils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** 所有周期均以毫秒计；由回绕安全的 TimeUtils_Elapsed() 判断。 */
#define HEARTBEAT_PERIOD_MS 500U
/** 密码输入无人操作超时，超时后清空输入，避免遗留敏感数据。 */
#define INPUT_TIMEOUT_MS 30000U
/** 管理菜单超时，防止管理员离开后界面长期暴露。 */
#define ADMIN_TIMEOUT_MS 60000U
/** 成功或失败提示在 OLED 上的最短停留时间。 */
#define FEEDBACK_PERIOD_MS 1500U
/** DS3231 刷新周期；无需为显示时间而连续占用 I2C 总线。 */
#define RTC_REFRESH_MS 1000U
/** RC522 轮询间隔，兼顾刷卡响应与 SPI 总线占用。 */
#define CARD_POLL_PERIOD_MS 200U
/** 同一张卡的重复触发抑制窗口，防止一次停留多次认证。 */
#define CARD_REPEAT_GUARD_MS 1500U
/** 空闲时指纹图像采集轮询间隔。 */
#define FINGER_POLL_PERIOD_MS 500U
/** 指纹模块通信异常后的重试等待，避免持续发送命令。 */
#define FINGER_RETRY_PERIOD_MS 5000U
/** 不同结果对应的蜂鸣器提示时长。 */
#define BUZZER_SUCCESS_MS 120U
#define BUZZER_FAILURE_MS 450U
/** AS608 的初始化认证参数由应用层集中管理，量产时应按部署策略调整。 */
#define AS608_DEFAULT_PASSWORD 0x00000000UL
/** 本项目为 AT24C02 中的指纹 ID 位图预留的最大模板编号。 */
#define FINGER_ID_MAX 299U

typedef enum
{
  /** 空闲等待三种认证输入。 */
  APP_STATE_IDLE = 0,
  /** 普通用户密码输入。 */
  APP_STATE_USER_PIN,
  /** 管理员密码输入；成功后进入菜单。 */
  APP_STATE_ADMIN_PIN,
  /** 门锁服务处于开锁保持窗口。 */
  APP_STATE_UNLOCKED,
  /** OLED、LED、蜂鸣器给出一次结果反馈。 */
  APP_STATE_FEEDBACK,
  /** 连续认证失败后的临时锁定状态。 */
  APP_STATE_LOCKED,
  /** 管理员菜单总入口。 */
  APP_STATE_ADMIN_MENU,
  /** 管理员修改普通用户密码。 */
  APP_STATE_EDIT_USER_PIN,
  /** 管理员修改管理员密码。 */
  APP_STATE_EDIT_ADMIN_PIN,
  /** 等待一张待授权的 RFID 卡。 */
  APP_STATE_CARD_ADD,
  /** 等待一张待删除的 RFID 卡。 */
  APP_STATE_CARD_REMOVE,
  /** 等待键盘输入待录入的指纹模板 ID。 */
  APP_STATE_FINGER_ADD_ID,
  /** 等待键盘输入待删除的指纹模板 ID。 */
  APP_STATE_FINGER_DELETE_ID,
  /** 指纹录入子状态机运行中。 */
  APP_STATE_FINGER_ENROLL,
  /** 指纹模板删除命令运行中。 */
  APP_STATE_FINGER_DELETE
} AppState;

typedef enum
{
  /** 空闲验证流程。 */
  FINGER_STATE_VERIFY = 0,
  /** 没有待处理的指纹命令。 */
  FINGER_STATE_IDLE,
  /** 发起采集图像命令。 */
  FINGER_STATE_AUTH_IMAGE,
  /** 将采集图像生成特征。 */
  FINGER_STATE_AUTH_CHAR,
  /** 在模块模板库中搜索特征。 */
  FINGER_STATE_AUTH_SEARCH,
  /** 录入流程的第一次图像采集。 */
  FINGER_STATE_ENROLL_IMAGE1,
  /** 录入流程的第一次特征生成。 */
  FINGER_STATE_ENROLL_CHAR1,
  /** 等待手指移开，防止两次采集同一帧图像。 */
  FINGER_STATE_ENROLL_WAIT_REMOVE,
  /** 录入流程的第二次图像采集。 */
  FINGER_STATE_ENROLL_IMAGE2,
  /** 录入流程的第二次特征生成。 */
  FINGER_STATE_ENROLL_CHAR2,
  /** 合成两次采集的特征模型。 */
  FINGER_STATE_ENROLL_MODEL,
  /** 将模型写入模块指定模板 ID。 */
  FINGER_STATE_ENROLL_STORE,
  /** 删除模块中的模板。 */
  FINGER_STATE_DELETE,
  /** 录入失败后清理可能残留的模板。 */
  FINGER_STATE_ROLLBACK,
  /** 通信异常或指纹操作失败后的延时重试。 */
  FINGER_STATE_RETRY
} FingerState;

/** 应用层唯一的顶层状态；仅在主循环上下文读写。 */
static AppState s_state;
/** AS608 多步骤命令状态；回调只投递事件，状态在主循环中推进。 */
static FingerState s_fingerState;
static LockService s_lockService;
static AuthService s_authService;
static StorageBackend s_storageBackend;
static CredentialService s_credentials;
static SystemConfig s_config;
static uint32_t s_heartbeatTick;
static uint32_t s_rtcTick;
static uint32_t s_displayTick;
static uint32_t s_stateTick;
static uint32_t s_cardPollTick;
static uint32_t s_cardEventTick;
static uint32_t s_fingerTick;
static uint32_t s_buzzerTick;
static uint32_t s_buzzerDuration;
static uint16_t s_targetFingerId;
static char s_input[SYSTEM_PIN_MAX_LENGTH + 1U];
static char s_timeText[9] = "--:--:--";
static uint8_t s_inputLength;
static bool s_storageReady;
static bool s_rc522Ready;
static bool s_feedbackReturnAdmin;
static bool s_buzzerActive;

/** 将服务层读请求适配到 AT24C02 驱动；地址边界由驱动层再次检查。 */
static bool StorageRead(uint16_t address, uint8_t *data, size_t length, void *context)
{
  (void)context;
  return BspAt24c02_Read(address, data, length);
}

/** 将服务层写请求适配到 AT24C02 驱动，不能绕过双记录和 CRC 策略。 */
static bool StorageWrite(uint16_t address, const uint8_t *data, size_t length, void *context)
{
  (void)context;
  return BspAt24c02_Write(address, data, length);
}

/**
 * @brief 门锁服务的唯一输出适配点。
 * @details 阶段 0--6 只驱动 LED 模拟锁，禁止把 STM32 GPIO 直接接至 12 V 电磁锁。
 */
static void ApplyLockOutput(bool unlocked, void *context)
{
  (void)context;
  BspBoard_SetLockSimulation(unlocked);
}

/** 启动非阻塞蜂鸣器提示，持续时间由 TaskBuzzer() 到期关闭。 */
static void StartBuzzer(uint32_t now, uint32_t durationMs)
{
  s_buzzerTick = now;
  s_buzzerDuration = durationMs;
  s_buzzerActive = true;
  BspBoard_SetBuzzer(true);
}

/** 使用回绕安全的时间比较结束蜂鸣器，主循环不会因此延时。 */
static void TaskBuzzer(uint32_t now)
{
  if (s_buzzerActive && TimeUtils_Elapsed(now, s_buzzerTick, s_buzzerDuration))
  {
    s_buzzerActive = false;
    BspBoard_SetBuzzer(false);
  }
}

/** 清除密码或模板编号输入缓冲，减少界面切换时的残留数据。 */
static void ResetInput(void)
{
  memset(s_input, 0, sizeof(s_input));
  s_inputLength = 0U;
}

/** 按需以星号遮蔽密码；模板编号等非敏感输入可直接显示。 */
static void DrawInput(const char *title, bool hidden)
{
  char shown[SYSTEM_PIN_MAX_LENGTH + 1U];
  uint8_t index;
  for (index = 0U; index < s_inputLength; ++index)
  {
    shown[index] = hidden ? '*' : s_input[index];
  }
  shown[s_inputLength] = '\0';
  BspOled_Clear();
  BspOled_DrawString(0U, 1U, title);
  BspOled_DrawString(0U, 3U, shown);
  BspOled_DrawString(0U, 6U, "#:OK  *:BACK");
}

/** 绘制空闲页；实际刷卡和指纹轮询仍由各自任务周期运行。 */
static void ShowIdle(void)
{
  BspOled_Clear();
  BspOled_DrawString(0U, 0U, "SMART LOCK");
  BspOled_DrawString(0U, 2U, "A:USER PIN");
  BspOled_DrawString(0U, 4U, "D:ADMIN");
  BspOled_DrawString(0U, 6U, s_timeText);
}

/** 菜单仅提供阶段 4--6 已实现的用户凭据和安全策略入口。 */
static void ShowAdminMenu(void)
{
  BspOled_Clear();
  BspOled_DrawString(0U, 0U, "ADMIN MENU");
  BspOled_DrawString(0U, 2U, "1:PIN 2:CARD+");
  BspOled_DrawString(0U, 4U, "3:CARD- 4:FP+");
  BspOled_DrawString(0U, 6U, "5:FP- 6:ADM 0:X");
}

/** 将剩余锁定时间格式化为 OLED 可显示的短文本。 */
static void FormatSeconds(uint32_t seconds, char text[14])
{
  memcpy(text, "WAIT 0000 SEC", 14U);
  text[5] = (char)('0' + (uint8_t)((seconds / 1000U) % 10U));
  text[6] = (char)('0' + (uint8_t)((seconds / 100U) % 10U));
  text[7] = (char)('0' + (uint8_t)((seconds / 10U) % 10U));
  text[8] = (char)('0' + (uint8_t)(seconds % 10U));
}

/** 锁定界面动态显示剩余时间，不允许认证服务继续接收尝试。 */
static void ShowLocked(uint32_t now)
{
  char text[14];
  uint32_t remaining = AuthService_RemainingMs(&s_authService, now);
  FormatSeconds((remaining + 999U) / 1000U, text);
  BspOled_Clear();
  BspOled_DrawString(0U, 2U, "TEMP LOCKED");
  BspOled_DrawString(0U, 4U, text);
}

/** 判断反馈完成后是否应返回管理员菜单，而不是错误回到普通空闲页。 */
static bool IsAdminState(AppState state)
{
  return (state == APP_STATE_ADMIN_MENU) || (state == APP_STATE_EDIT_USER_PIN) ||
         (state == APP_STATE_EDIT_ADMIN_PIN) || (state == APP_STATE_CARD_ADD) ||
         (state == APP_STATE_CARD_REMOVE) || (state == APP_STATE_FINGER_ADD_ID) ||
         (state == APP_STATE_FINGER_DELETE_ID) || (state == APP_STATE_FINGER_ENROLL) ||
         (state == APP_STATE_FINGER_DELETE);
}

/** 恢复正常认证入口，并清除上一次交互的临时输入。 */
static void EnterIdle(void)
{
  ResetInput();
  s_state = APP_STATE_IDLE;
  s_stateTick = HAL_GetTick();
  ShowIdle();
}

/** 记录菜单进入时刻，用于无操作超时退出。 */
static void EnterAdminMenu(uint32_t now)
{
  ResetInput();
  s_state = APP_STATE_ADMIN_MENU;
  s_stateTick = now;
  ShowAdminMenu();
}

/** 统一显示操作结果，避免各流程自行处理反馈时长和返回路径。 */
static void EnterFeedback(const char *message, bool returnAdmin, uint32_t now)
{
  s_state = APP_STATE_FEEDBACK;
  s_stateTick = now;
  s_feedbackReturnAdmin = returnAdmin;
  BspOled_Clear();
  BspOled_DrawString(0U, 2U, message);
}

/**
 * @brief 统一记录认证失败并应用临时锁定策略。
 * @details 只有认证服务决定是否达到阈值；界面层不保存失败计数，避免多入口绕过限制。
 */
static void AuthenticationFailed(uint32_t now, const char *logMessage)
{
  BspLog_Write(logMessage);
  if (AuthService_RecordFailure(&s_authService, now))
  {
    s_state = APP_STATE_LOCKED;
    s_stateTick = now;
    ShowLocked(now);
    StartBuzzer(now, BUZZER_FAILURE_MS);
    BspLog_Write("[SEC] temporary lockout started\r\n");
  }
  else
  {
    EnterFeedback("ACCESS DENIED", false, now);
    StartBuzzer(now, BUZZER_FAILURE_MS);
  }
}

/**
 * @brief 将任一已验证凭据转换为一次门锁服务请求。
 * @details 密码、卡和指纹共享此出口，保证成功认证都会清除失败计数并使用同一自动上锁逻辑。
 */
static void GrantAccess(uint32_t now, const char *source)
{
  if (!AuthService_CanAttempt(&s_authService))
  {
    BspLog_Write("[SEC] unlock request ignored during lockout\r\n");
    return;
  }
  AuthService_RecordSuccess(&s_authService);
  if (LockService_RequestUnlock(&s_lockService,
                                now,
                                (uint32_t)s_config.unlockSeconds * 1000U))
  {
    s_state = APP_STATE_UNLOCKED;
    s_stateTick = now;
    BspOled_Clear();
    BspOled_DrawString(0U, 2U, "UNLOCKED");
    BspOled_DrawString(0U, 4U, "AUTO LOCK");
    StartBuzzer(now, BUZZER_SUCCESS_MS);
    BspLog_Write(source);
  }
}

/** 依据 EEPROM 位图判断模板 ID 是否可用，避免覆盖已登记的指纹。 */
static bool FingerSlotAvailable(void)
{
  uint8_t index;
  for (index = 0U; index < CREDENTIAL_FINGER_LIMIT; ++index)
  {
    if (!s_credentials.fingerUsed[index])
    {
      return true;
    }
  }
  return false;
}

/** 将已限定长度的数字键盘输入转换为模板 ID；调用方仍须检查允许范围。 */
static uint16_t InputToNumber(void)
{
  uint16_t value = 0U;
  uint8_t index;
  for (index = 0U; index < s_inputLength; ++index)
  {
    value = (uint16_t)((value * 10U) + (uint16_t)(s_input[index] - '0'));
  }
  return value;
}

/** 保存修改后的 PIN，并通过存储服务的 CRC/双记录机制保证掉电恢复。 */
static bool SavePin(bool adminPin)
{
  SystemConfig oldConfig = s_config;
  uint8_t *target = adminPin ? s_config.adminPin : s_config.userPin;
  uint8_t *targetLength = adminPin ? &s_config.adminPinLength : &s_config.userPinLength;
  memset(target, 0, SYSTEM_PIN_MAX_LENGTH);
  memcpy(target, s_input, s_inputLength);
  *targetLength = s_inputLength;
  if (!s_storageReady || !StorageService_SaveConfig(&s_storageBackend, &s_config))
  {
    s_config = oldConfig;
    return false;
  }
  return true;
}

/** 进入输入子状态时统一初始化缓冲区、显示和输入超时起点。 */
static void BeginInput(AppState state, const char *title, bool hidden, uint32_t now)
{
  ResetInput();
  s_state = state;
  s_stateTick = now;
  DrawInput(title, hidden);
}

/** 指纹通信异常时不忙等，延时后再恢复空闲验证轮询。 */
static void FingerCommunicationFailure(uint32_t now)
{
  bool adminOperation = (s_state == APP_STATE_FINGER_ENROLL) ||
                        (s_state == APP_STATE_FINGER_DELETE);
  s_fingerState = FINGER_STATE_RETRY;
  s_fingerTick = now;
  BspLog_Write("[WARN] AS608 communication failure; retry scheduled\r\n");
  if (adminOperation)
  {
    EnterFeedback("FINGER OFFLINE", true, now);
  }
}

/** 只在驱动成功接收命令时迁移指纹子状态，防止忙状态下丢失流程。 */
static bool StartFingerCommand(bool started, FingerState next, uint32_t now)
{
  if (started)
  {
    s_fingerState = next;
    return true;
  }
  FingerCommunicationFailure(now);
  return false;
}

/**
 * @brief 消费 AS608 驱动投递的完成结果并推进多步骤指纹状态机。
 * @details 回调中不处理协议或 UI；本函数在主循环执行，故录入、删除和失败回滚均不会占用中断。
 */
static void HandleFingerResult(const BspAs608Result *result, uint32_t now)
{
  if ((result == NULL) || (result->type != AS608_RESULT_ACK))
  {
    FingerCommunicationFailure(now);
    return;
  }
  if ((((s_fingerState >= FINGER_STATE_ENROLL_IMAGE1) &&
        (s_fingerState <= FINGER_STATE_ENROLL_STORE)) ||
       (s_fingerState == FINGER_STATE_ROLLBACK)) &&
      (s_state != APP_STATE_FINGER_ENROLL))
  {
    s_fingerState = FINGER_STATE_RETRY;
    s_fingerTick = now;
    return;
  }
  if ((s_fingerState == FINGER_STATE_DELETE) &&
      (s_state != APP_STATE_FINGER_DELETE))
  {
    s_fingerState = FINGER_STATE_RETRY;
    s_fingerTick = now;
    return;
  }
  switch (s_fingerState)
  {
    case FINGER_STATE_VERIFY:
      if (result->confirmation == 0U)
      {
        s_fingerState = FINGER_STATE_IDLE;
        s_fingerTick = now;
        BspLog_Write("[OK] AS608 password verified\r\n");
      }
      else
      {
        FingerCommunicationFailure(now);
      }
      break;
    case FINGER_STATE_AUTH_IMAGE:
      if ((s_state != APP_STATE_IDLE) || !AuthService_CanAttempt(&s_authService))
      {
        s_fingerState = FINGER_STATE_IDLE;
        s_fingerTick = now;
      }
      else if (result->confirmation == 0U)
      {
        (void)StartFingerCommand(BspAs608_StartImageToChar(1U, now),
                                 FINGER_STATE_AUTH_CHAR, now);
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        s_fingerTick = now;
      }
      break;
    case FINGER_STATE_AUTH_CHAR:
      if ((s_state != APP_STATE_IDLE) || !AuthService_CanAttempt(&s_authService))
      {
        s_fingerState = FINGER_STATE_IDLE;
        s_fingerTick = now;
      }
      else if (result->confirmation == 0U)
      {
        (void)StartFingerCommand(BspAs608_StartSearch(1U, now),
                                 FINGER_STATE_AUTH_SEARCH, now);
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        s_fingerTick = now;
      }
      break;
    case FINGER_STATE_AUTH_SEARCH:
      s_fingerState = FINGER_STATE_IDLE;
      s_fingerTick = now;
      if ((s_state != APP_STATE_IDLE) || !AuthService_CanAttempt(&s_authService))
      {
        break;
      }
      if ((result->confirmation == 0U) &&
          CredentialService_IsFingerAuthorized(&s_credentials, result->templateId))
      {
        GrantAccess(now, "[AUTH] fingerprint success\r\n");
      }
      else
      {
        AuthenticationFailed(now, "[AUTH] fingerprint failure\r\n");
      }
      break;
    case FINGER_STATE_ENROLL_IMAGE1:
      if (result->confirmation == 0U)
      {
        (void)StartFingerCommand(BspAs608_StartImageToChar(1U, now),
                                 FINGER_STATE_ENROLL_CHAR1, now);
      }
      else if (result->confirmation == 0x02U)
      {
        s_fingerTick = now;
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("BAD FINGER IMAGE", true, now);
      }
      break;
    case FINGER_STATE_ENROLL_CHAR1:
      if (result->confirmation == 0U)
      {
        s_fingerState = FINGER_STATE_ENROLL_WAIT_REMOVE;
        s_fingerTick = now;
        BspOled_Clear();
        BspOled_DrawString(0U, 2U, "REMOVE FINGER");
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("FEATURE ERROR", true, now);
      }
      break;
    case FINGER_STATE_ENROLL_WAIT_REMOVE:
      s_fingerTick = now;
      if (result->confirmation == 0x02U)
      {
        s_fingerState = FINGER_STATE_ENROLL_IMAGE2;
        BspOled_Clear();
        BspOled_DrawString(0U, 2U, "PLACE FINGER 2");
      }
      break;
    case FINGER_STATE_ENROLL_IMAGE2:
      if (result->confirmation == 0U)
      {
        (void)StartFingerCommand(BspAs608_StartImageToChar(2U, now),
                                 FINGER_STATE_ENROLL_CHAR2, now);
      }
      else if (result->confirmation == 0x02U)
      {
        s_fingerTick = now;
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("BAD FINGER IMAGE", true, now);
      }
      break;
    case FINGER_STATE_ENROLL_CHAR2:
      if ((result->confirmation == 0U) &&
          StartFingerCommand(BspAs608_StartCreateModel(now),
                             FINGER_STATE_ENROLL_MODEL, now))
      {
        BspOled_Clear();
        BspOled_DrawString(0U, 2U, "BUILD TEMPLATE");
      }
      else if (result->confirmation != 0U)
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("FEATURE ERROR", true, now);
      }
      break;
    case FINGER_STATE_ENROLL_MODEL:
      if (result->confirmation == 0U)
      {
        (void)StartFingerCommand(BspAs608_StartStore(2U, s_targetFingerId, now),
                                 FINGER_STATE_ENROLL_STORE, now);
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("FINGER MISMATCH", true, now);
      }
      break;
    case FINGER_STATE_ENROLL_STORE:
      if (result->confirmation == 0U)
      {
        CredentialResult addResult = CredentialService_AddFinger(&s_credentials,
                                                                  &s_storageBackend,
                                                                  s_targetFingerId);
        if (addResult == CREDENTIAL_OK)
        {
          s_fingerState = FINGER_STATE_IDLE;
          EnterFeedback("FINGER ADDED", true, now);
          StartBuzzer(now, BUZZER_SUCCESS_MS);
        }
        else
        {
          (void)StartFingerCommand(BspAs608_StartDelete(s_targetFingerId, now),
                                   FINGER_STATE_ROLLBACK, now);
        }
      }
      else
      {
        s_fingerState = FINGER_STATE_IDLE;
        EnterFeedback("STORE FAILED", true, now);
      }
      break;
    case FINGER_STATE_DELETE:
      s_fingerState = FINGER_STATE_IDLE;
      if ((result->confirmation == 0U) &&
          (CredentialService_RemoveFinger(&s_credentials,
                                          &s_storageBackend,
                                          s_targetFingerId) == CREDENTIAL_OK))
      {
        EnterFeedback("FINGER DELETED", true, now);
      }
      else
      {
        EnterFeedback("DELETE FAILED", true, now);
      }
      break;
    case FINGER_STATE_ROLLBACK:
      s_fingerState = FINGER_STATE_IDLE;
      EnterFeedback("EEPROM FAILED", true, now);
      break;
    case FINGER_STATE_IDLE:
    case FINGER_STATE_RETRY:
      break;
  }
}

/** 根据当前输入状态解释确认键，密码比较和管理操作均在此集中完成。 */
static void HandleSubmittedInput(uint32_t now)
{
  if (s_state == APP_STATE_USER_PIN)
  {
    if ((s_inputLength == s_config.userPinLength) &&
        (memcmp(s_input, s_config.userPin, s_inputLength) == 0))
    {
      GrantAccess(now, "[AUTH] password success\r\n");
    }
    else
    {
      AuthenticationFailed(now, "[AUTH] password failure\r\n");
    }
  }
  else if (s_state == APP_STATE_ADMIN_PIN)
  {
    if ((s_inputLength == s_config.adminPinLength) &&
        (memcmp(s_input, s_config.adminPin, s_inputLength) == 0))
    {
      AuthService_RecordSuccess(&s_authService);
      EnterAdminMenu(now);
      BspLog_Write("[AUTH] administrator success\r\n");
    }
    else
    {
      AuthenticationFailed(now, "[AUTH] administrator failure\r\n");
    }
  }
  else if ((s_state == APP_STATE_EDIT_USER_PIN) ||
           (s_state == APP_STATE_EDIT_ADMIN_PIN))
  {
    if ((s_inputLength >= 4U) && SavePin(s_state == APP_STATE_EDIT_ADMIN_PIN))
    {
      EnterFeedback("PIN SAVED", true, now);
    }
    else
    {
      EnterFeedback("PIN SAVE FAILED", true, now);
    }
  }
  else if ((s_state == APP_STATE_FINGER_ADD_ID) ||
           (s_state == APP_STATE_FINGER_DELETE_ID))
  {
    uint16_t id = InputToNumber();
    bool adding = s_state == APP_STATE_FINGER_ADD_ID;
    if ((s_inputLength == 0U) || (id > FINGER_ID_MAX) || !s_storageReady)
    {
      EnterFeedback("INVALID ID", true, now);
    }
    else if (adding && (CredentialService_IsFingerAuthorized(&s_credentials, id) ||
                        !FingerSlotAvailable()))
    {
      EnterFeedback("ID USED OR FULL", true, now);
    }
    else if (!adding && !CredentialService_IsFingerAuthorized(&s_credentials, id))
    {
      EnterFeedback("ID NOT FOUND", true, now);
    }
    else if (s_fingerState != FINGER_STATE_IDLE)
    {
      EnterFeedback("FINGER OFFLINE", true, now);
    }
    else
    {
      s_targetFingerId = id;
      s_stateTick = now;
      if (adding)
      {
        s_state = APP_STATE_FINGER_ENROLL;
        s_fingerState = FINGER_STATE_ENROLL_IMAGE1;
        s_fingerTick = now - FINGER_POLL_PERIOD_MS;
        BspOled_Clear();
        BspOled_DrawString(0U, 2U, "PLACE FINGER 1");
      }
      else
      {
        s_state = APP_STATE_FINGER_DELETE;
        if (StartFingerCommand(BspAs608_StartDelete(id, now),
                               FINGER_STATE_DELETE, now))
        {
          BspOled_Clear();
          BspOled_DrawString(0U, 2U, "DELETING...");
        }
      }
    }
  }
}

/** 处理数字、退格和确认键，并在每次有效按键后刷新输入超时。 */
static void HandleInputKey(char key, uint32_t now)
{
  bool idInput = (s_state == APP_STATE_FINGER_ADD_ID) ||
                 (s_state == APP_STATE_FINGER_DELETE_ID);
  uint8_t maximum = idInput ? 3U : SYSTEM_PIN_MAX_LENGTH;
  if ((key >= '0') && (key <= '9'))
  {
    if (s_inputLength < maximum)
    {
      s_input[s_inputLength] = key;
      ++s_inputLength;
      s_input[s_inputLength] = '\0';
      s_stateTick = now;
      DrawInput(idInput ? "TEMPLATE ID 0-299" : "ENTER PIN", !idInput);
    }
  }
  else if (key == '*')
  {
    if (IsAdminState(s_state))
    {
      EnterAdminMenu(now);
    }
    else
    {
      EnterIdle();
    }
  }
  else if (key == '#')
  {
    HandleSubmittedInput(now);
  }
}

/** 菜单按键仅切换已实现功能；未将接口预留误表示为可用功能。 */
static void HandleAdminMenuKey(char key, uint32_t now)
{
  switch (key)
  {
    case '1': BeginInput(APP_STATE_EDIT_USER_PIN, "NEW USER PIN", true, now); break;
    case '2':
      s_state = APP_STATE_CARD_ADD;
      s_stateTick = now;
      BspOled_Clear();
      BspOled_DrawString(0U, 2U, "PRESENT NEW CARD");
      break;
    case '3':
      s_state = APP_STATE_CARD_REMOVE;
      s_stateTick = now;
      BspOled_Clear();
      BspOled_DrawString(0U, 2U, "PRESENT OLD CARD");
      break;
    case '4': BeginInput(APP_STATE_FINGER_ADD_ID, "TEMPLATE ID 0-299", false, now); break;
    case '5': BeginInput(APP_STATE_FINGER_DELETE_ID, "TEMPLATE ID 0-299", false, now); break;
    case '6': BeginInput(APP_STATE_EDIT_ADMIN_PIN, "NEW ADMIN PIN", true, now); break;
    case '0': EnterIdle(); break;
    default: break;
  }
}

/**
 * @brief 定期轮询 RC522 并消除同卡重复事件。
 * @details RC522 驱动的单次 SPI 事务有自身超时；本层只在间隔到期时发起，避免主循环阻塞。
 */
static void TaskCard(uint32_t now)
{
  bool cardState = (s_state == APP_STATE_IDLE) || (s_state == APP_STATE_CARD_ADD) ||
                   (s_state == APP_STATE_CARD_REMOVE);
  if (!s_rc522Ready || !cardState ||
      !TimeUtils_Elapsed(now, s_cardPollTick, CARD_POLL_PERIOD_MS) ||
      !TimeUtils_Elapsed(now, s_cardEventTick, CARD_REPEAT_GUARD_MS))
  {
    return;
  }
  s_cardPollTick = now;
  {
    uint8_t uid[4];
    if (BspRc522_ReadUid(uid))
    {
      s_cardEventTick = now;
      if (s_state == APP_STATE_IDLE)
      {
        if (AuthService_CanAttempt(&s_authService) &&
            CredentialService_IsCardAuthorized(&s_credentials, uid))
        {
          GrantAccess(now, "[AUTH] RFID success\r\n");
        }
        else if (AuthService_CanAttempt(&s_authService))
        {
          AuthenticationFailed(now, "[AUTH] RFID failure\r\n");
        }
      }
      else if (!s_storageReady)
      {
        EnterFeedback("EEPROM OFFLINE", true, now);
      }
      else
      {
        CredentialResult result = (s_state == APP_STATE_CARD_ADD)
                                    ? CredentialService_AddCard(&s_credentials,
                                                                &s_storageBackend, uid)
                                    : CredentialService_RemoveCard(&s_credentials,
                                                                   &s_storageBackend, uid);
        EnterFeedback((result == CREDENTIAL_OK) ? "CARD UPDATED" : "CARD OP FAILED",
                      true, now);
      }
    }
  }
}

/** 指纹命令完成后才处理结果；空闲时按固定周期发起下一次图像采集。 */
static void TaskFinger(uint32_t now)
{
  BspAs608Result result;
  BspAs608_Task(now);
  if (BspAs608_TakeResult(&result))
  {
    HandleFingerResult(&result, now);
  }
  if ((s_fingerState == FINGER_STATE_RETRY) &&
      TimeUtils_Elapsed(now, s_fingerTick, FINGER_RETRY_PERIOD_MS) &&
      BspAs608_StartVerifyPassword(AS608_DEFAULT_PASSWORD, now))
  {
    s_fingerState = FINGER_STATE_VERIFY;
  }
  else if ((s_fingerState == FINGER_STATE_IDLE) && (s_state == APP_STATE_IDLE) &&
           AuthService_CanAttempt(&s_authService) &&
           TimeUtils_Elapsed(now, s_fingerTick, FINGER_POLL_PERIOD_MS) &&
           BspAs608_StartGetImage(now))
  {
    s_fingerState = FINGER_STATE_AUTH_IMAGE;
  }
  else if (((s_fingerState == FINGER_STATE_ENROLL_IMAGE1) ||
            (s_fingerState == FINGER_STATE_ENROLL_WAIT_REMOVE) ||
            (s_fingerState == FINGER_STATE_ENROLL_IMAGE2)) &&
           TimeUtils_Elapsed(now, s_fingerTick, FINGER_POLL_PERIOD_MS) &&
           BspAs608_StartGetImage(now))
  {
    s_fingerTick = now;
  }
}

/** RTC 读取失败时保留占位文本，不能使用未校验的时间作为安全判定依据。 */
static void UpdateRtc(uint32_t now)
{
  if (TimeUtils_Elapsed(now, s_rtcTick, RTC_REFRESH_MS))
  {
    BspDs3231DateTime dateTime;
    s_rtcTick = now;
    if (BspDs3231_Read(&dateTime))
    {
      s_timeText[0] = (char)('0' + (dateTime.hour / 10U));
      s_timeText[1] = (char)('0' + (dateTime.hour % 10U));
      s_timeText[2] = ':';
      s_timeText[3] = (char)('0' + (dateTime.minute / 10U));
      s_timeText[4] = (char)('0' + (dateTime.minute % 10U));
      s_timeText[5] = ':';
      s_timeText[6] = (char)('0' + (dateTime.second / 10U));
      s_timeText[7] = (char)('0' + (dateTime.second % 10U));
      s_timeText[8] = '\0';
    }
    else
    {
      memcpy(s_timeText, "--:--:--", sizeof(s_timeText));
    }
    if (s_state == APP_STATE_IDLE)
    {
      ShowIdle();
    }
  }
}

/**
 * @brief 初始化应用服务与外设适配关系。
 * @details 锁输出在 GPIO 初始化阶段已进入安全状态；此处仅建立 LED 模拟锁回调，随后才开放认证输入。
 */
void App_Init(void)
{
  uint32_t now;
  BspBoard_SetLockSimulation(false);
  BspBoard_SetStatusLed(false);
  BspBoard_SetBuzzer(false);
  LockService_Init(&s_lockService, ApplyLockOutput, NULL);
  BspKeypad_Init();
  s_storageBackend.read = StorageRead;
  s_storageBackend.write = StorageWrite;
  s_storageBackend.context = NULL;
  s_storageReady = BspAt24c02_IsReady();
  if (s_storageReady)
  {
    StorageLoadResult loadResult = StorageService_LoadConfig(&s_storageBackend, &s_config);
    BspLog_Write((loadResult == STORAGE_LOAD_DEFAULT)
                  ? "[WARN] EEPROM config invalid; defaults loaded\r\n"
                  : "[OK] EEPROM config restored\r\n");
  }
  else
  {
    StorageService_DefaultConfig(&s_config);
    BspLog_Write("[WARN] AT24C02 unavailable; volatile defaults\r\n");
  }
  CredentialService_Init(&s_credentials);
  if (s_storageReady && !CredentialService_Load(&s_credentials, &s_storageBackend))
  {
    BspLog_Write("[WARN] credential storage read error\r\n");
  }
  AuthService_Init(&s_authService, s_config.failureLimit, s_config.lockoutSeconds);
  s_rc522Ready = BspRc522_Init();
  BspLog_Write(s_rc522Ready ? "[OK] RC522 ready\r\n"
                            : "[WARN] RC522 unavailable; degraded mode\r\n");
  BspAs608_Init();
  now = HAL_GetTick();
  s_fingerState = FINGER_STATE_VERIFY;
  s_fingerTick = now;
  if (!BspAs608_StartVerifyPassword(AS608_DEFAULT_PASSWORD, now))
  {
    FingerCommunicationFailure(now);
  }
  BspLog_Write(BspOled_Init() ? "[OK] OLED 0x3C ready\r\n"
                              : "[WARN] OLED unavailable; degraded mode\r\n");
  BspLog_Write("[BOOT] SmartDoorLock stage6\r\n");
  s_heartbeatTick = now;
  s_rtcTick = now - RTC_REFRESH_MS;
  s_displayTick = now;
  s_cardPollTick = now;
  s_cardEventTick = now - CARD_REPEAT_GUARD_MS;
  s_buzzerActive = false;
  EnterIdle();
}

/**
 * @brief 每次主循环调用一次，按小步骤推进全部非阻塞服务。
 * @details 调用顺序保证先处理认证/门锁安全状态，再刷新显示；本函数中不得加入 HAL_Delay()。
 */
void App_Task(void)
{
  uint32_t now = HAL_GetTick();
  char key;
  bool wasLocked = !AuthService_CanAttempt(&s_authService);

  BspKeypad_Task(now);
  LockService_Task(&s_lockService, now);
  AuthService_Task(&s_authService, now);
  TaskBuzzer(now);
  TaskFinger(now);
  TaskCard(now);
  UpdateRtc(now);

  if (wasLocked && AuthService_CanAttempt(&s_authService))
  {
    BspLog_Write("[SEC] temporary lockout ended\r\n");
    EnterIdle();
  }
  if (BspKeypad_GetKey(&key))
  {
    if (s_state == APP_STATE_IDLE)
    {
      if ((key == 'A') && AuthService_CanAttempt(&s_authService))
      {
        BeginInput(APP_STATE_USER_PIN, "USER PIN", true, now);
      }
      else if ((key == 'D') && AuthService_CanAttempt(&s_authService))
      {
        BeginInput(APP_STATE_ADMIN_PIN, "ADMIN PIN", true, now);
      }
    }
    else if (s_state == APP_STATE_ADMIN_MENU)
    {
      HandleAdminMenuKey(key, now);
    }
    else if ((s_state == APP_STATE_USER_PIN) || (s_state == APP_STATE_ADMIN_PIN) ||
             (s_state == APP_STATE_EDIT_USER_PIN) ||
             (s_state == APP_STATE_EDIT_ADMIN_PIN) ||
             (s_state == APP_STATE_FINGER_ADD_ID) ||
             (s_state == APP_STATE_FINGER_DELETE_ID))
    {
      HandleInputKey(key, now);
    }
    else if (IsAdminState(s_state) && (key == '*'))
    {
      EnterAdminMenu(now);
    }
  }

  if (((s_state == APP_STATE_USER_PIN) || (s_state == APP_STATE_ADMIN_PIN)) &&
      TimeUtils_Elapsed(now, s_stateTick, INPUT_TIMEOUT_MS))
  {
    BspLog_Write("[AUTH] input timeout\r\n");
    EnterIdle();
  }
  else if (IsAdminState(s_state) &&
           TimeUtils_Elapsed(now, s_stateTick, ADMIN_TIMEOUT_MS))
  {
    BspLog_Write("[ADMIN] session timeout\r\n");
    EnterIdle();
  }
  else if ((s_state == APP_STATE_UNLOCKED) && !LockService_IsUnlocked(&s_lockService))
  {
    BspLog_Write("[LOCK] automatic relock\r\n");
    EnterIdle();
  }
  else if ((s_state == APP_STATE_FEEDBACK) &&
           TimeUtils_Elapsed(now, s_stateTick, FEEDBACK_PERIOD_MS))
  {
    if (s_feedbackReturnAdmin)
    {
      EnterAdminMenu(now);
    }
    else
    {
      EnterIdle();
    }
  }
  else if ((s_state == APP_STATE_LOCKED) &&
           TimeUtils_Elapsed(now, s_displayTick, RTC_REFRESH_MS))
  {
    s_displayTick = now;
    ShowLocked(now);
  }

  if (TimeUtils_Elapsed(now, s_heartbeatTick, HEARTBEAT_PERIOD_MS))
  {
    s_heartbeatTick = now;
    BspBoard_ToggleStatusLed();
  }
  BspOled_Task();
}
