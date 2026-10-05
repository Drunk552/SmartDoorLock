#include "lock_service.h"
#include "crc.h"
#include "storage_service.h"
#include "credential_service.h"
#include "as608_protocol.h"
#include "auth_service.h"
#include "time_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool s_outputUnlocked;
static unsigned int s_outputCalls;
static uint8_t s_eeprom[256];

static bool MemoryRead(uint16_t address, uint8_t *data, size_t length, void *context)
{
  (void)context;
  if ((data == NULL) || ((size_t)address + length > sizeof(s_eeprom)))
  {
    return false;
  }
  memcpy(data, &s_eeprom[address], length);
  return true;
}

static bool MemoryWrite(uint16_t address, const uint8_t *data, size_t length, void *context)
{
  (void)context;
  if ((data == NULL) || ((size_t)address + length > sizeof(s_eeprom)))
  {
    return false;
  }
  memcpy(&s_eeprom[address], data, length);
  return true;
}

static void TestOutput(bool unlocked, void *context)
{
  (void)context;
  s_outputUnlocked = unlocked;
  ++s_outputCalls;
}

static void TestLockAutomaticRelock(void)
{
  LockService service;
  s_outputUnlocked = true;
  s_outputCalls = 0U;
  LockService_Init(&service, TestOutput, NULL);
  assert(!s_outputUnlocked);
  assert(s_outputCalls == 1U);
  assert(LockService_RequestUnlock(&service, 100U, 5000U));
  assert(LockService_IsUnlocked(&service));
  LockService_Task(&service, 5099U);
  assert(LockService_IsUnlocked(&service));
  LockService_Task(&service, 5100U);
  assert(!LockService_IsUnlocked(&service));
  assert(!s_outputUnlocked);
}

static void TestLockRejectsInvalidDuration(void)
{
  LockService service;
  LockService_Init(&service, TestOutput, NULL);
  assert(!LockService_RequestUnlock(&service, 0U, 99U));
  assert(!LockService_RequestUnlock(&service, 0U, 30001U));
}

static void TestTickWrap(void)
{
  uint32_t start = UINT32_MAX - 10U;
  assert(!TimeUtils_Elapsed(5U, start, 20U));
  assert(TimeUtils_Elapsed(10U, start, 20U));
}

static void TestCrcVectors(void)
{
  static const uint8_t vector[] = "123456789";
  assert(Crc16Ccitt(vector, sizeof(vector) - 1U) == 0x29B1U);
  assert(Crc8(vector, sizeof(vector) - 1U) == 0xF4U);
}

static void TestStorageRecovery(void)
{
  StorageBackend backend = {MemoryRead, MemoryWrite, NULL};
  SystemConfig config;
  SystemConfig restored;

  memset(s_eeprom, 0xFF, sizeof(s_eeprom));
  assert(StorageService_LoadConfig(&backend, &config) == STORAGE_LOAD_DEFAULT);
  assert(StorageService_ConfigIsValid(&config));
  assert(config.sequence == 0U);
  assert(StorageService_SaveConfig(&backend, &config));
  assert(config.sequence == 1U);

  config.userPin[0] = '9';
  assert(StorageService_SaveConfig(&backend, &config));
  assert(config.sequence == 2U);
  assert(StorageService_LoadConfig(&backend, &restored) == STORAGE_LOAD_COPY_B);
  assert(restored.sequence == 2U);
  assert(restored.userPin[0] == '9');

  s_eeprom[0x20U + 12U] ^= 0x01U;
  assert(StorageService_LoadConfig(&backend, &restored) == STORAGE_LOAD_COPY_A);
  assert(restored.sequence == 1U);
  assert(restored.userPin[0] == '1');
}

static void TestCredentialStorage(void)
{
  StorageBackend backend = {MemoryRead, MemoryWrite, NULL};
  CredentialService service;
  CredentialService restored;
  static const uint8_t uid[4] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
  static const uint8_t otherUid[4] = {1U, 2U, 3U, 4U};

  memset(s_eeprom, 0xFF, sizeof(s_eeprom));
  assert(CredentialService_Load(&service, &backend));
  assert(!CredentialService_IsCardAuthorized(&service, uid));
  assert(CredentialService_AddCard(&service, &backend, uid) == CREDENTIAL_OK);
  assert(CredentialService_AddCard(&service, &backend, uid) == CREDENTIAL_EXISTS);
  assert(CredentialService_IsCardAuthorized(&service, uid));
  assert(!CredentialService_IsCardAuthorized(&service, otherUid));
  assert(CredentialService_Load(&restored, &backend));
  assert(CredentialService_IsCardAuthorized(&restored, uid));
  assert(CredentialService_RemoveCard(&restored, &backend, uid) == CREDENTIAL_OK);
  assert(CredentialService_RemoveCard(&restored, &backend, uid) == CREDENTIAL_NOT_FOUND);

  assert(CredentialService_AddFinger(&service, &backend, 299U) == CREDENTIAL_OK);
  assert(CredentialService_AddFinger(&service, &backend, 300U) == CREDENTIAL_INVALID);
  assert(CredentialService_IsFingerAuthorized(&service, 299U));
  assert(CredentialService_RemoveFinger(&service, &backend, 299U) == CREDENTIAL_OK);
  assert(!CredentialService_IsFingerAuthorized(&service, 299U));
}

static void TestAs608Protocol(void)
{
  As608Parser parser;
  As608Ack ack;
  uint8_t encoded[AS608_PROTOCOL_PACKET_MAX];
  static const uint8_t expectedCommand[] = {
      0xEFU, 0x01U, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
      0x01U, 0x00U, 0x03U, 0x01U, 0x00U, 0x05U};
  static const uint8_t searchAck[] = {
      0xEFU, 0x01U, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
      0x07U, 0x00U, 0x07U, 0x00U, 0x00U, 0x12U,
      0x00U, 0x34U, 0x00U, 0x54U};
  size_t index;

  assert(As608Protocol_EncodeCommand(0x01U, NULL, 0U, encoded, sizeof(encoded)) ==
         sizeof(expectedCommand));
  assert(memcmp(encoded, expectedCommand, sizeof(expectedCommand)) == 0);
  As608Protocol_Init(&parser);
  for (index = 0U; index < sizeof(searchAck) - 1U; ++index)
  {
    assert(As608Protocol_Feed(&parser, searchAck[index]) == AS608_PARSE_NONE);
  }
  assert(As608Protocol_Feed(&parser, searchAck[sizeof(searchAck) - 1U]) == AS608_PARSE_READY);
  assert(As608Protocol_TakeAck(&parser, &ack));
  assert(ack.confirmation == 0U);
  assert(ack.parameterLength == 4U);
  assert(ack.parameters[1] == 0x12U);
  assert(ack.parameters[3] == 0x34U);

  As608Protocol_Init(&parser);
  for (index = 0U; index < sizeof(searchAck); ++index)
  {
    uint8_t byte = searchAck[index];
    if (index == (sizeof(searchAck) - 1U))
    {
      byte ^= 1U;
    }
    (void)As608Protocol_Feed(&parser, byte);
  }
  assert(!As608Protocol_TakeAck(&parser, &ack));
}

static void TestAuthenticationLockout(void)
{
  AuthService service;
  AuthService_Init(&service, 3U, 10U);
  assert(AuthService_CanAttempt(&service));
  assert(!AuthService_RecordFailure(&service, 100U));
  assert(!AuthService_RecordFailure(&service, 200U));
  assert(AuthService_FailureCount(&service) == 2U);
  AuthService_RecordSuccess(&service);
  assert(AuthService_FailureCount(&service) == 0U);
  assert(!AuthService_RecordFailure(&service, 300U));
  assert(!AuthService_RecordFailure(&service, 400U));
  assert(AuthService_RecordFailure(&service, UINT32_MAX - 100U));
  assert(!AuthService_CanAttempt(&service));
  assert(AuthService_RemainingMs(&service, 9898U) == 1U);
  AuthService_Task(&service, 9898U);
  assert(!AuthService_CanAttempt(&service));
  AuthService_Task(&service, 9899U);
  assert(AuthService_CanAttempt(&service));
  assert(AuthService_FailureCount(&service) == 0U);
}

int main(void)
{
  TestLockAutomaticRelock();
  TestLockRejectsInvalidDuration();
  TestTickWrap();
  TestCrcVectors();
  TestStorageRecovery();
  TestCredentialStorage();
  TestAs608Protocol();
  TestAuthenticationLockout();
  puts("host service tests: PASS");
  return 0;
}
